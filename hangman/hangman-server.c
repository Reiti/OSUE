/**hangman-server
  @author Michael Reitgruber, 1426100
  @brief Implements a server for the game hangman
  @detail Uses shared memory objects, to serve hangman games to a arbitrary number of clients, takes a wordlist as a file, or words from stdin
  @date 08.12.2015
  **/

#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <semaphore.h>
#include "hangman-common.h"




typedef struct {
    int cno;
    int mistakes;
    int used_words;
    int wins;
    int losses;
    char current_word[WORD_LENGTH]; 
    char guessed_letters[26];
}client; //structure which holds client information

static client **client_list = NULL; //list of connected clients
static int connected_clients = 0; //number of connected clients
static int hi_client_number=0; //current highest client number

static char **word_list = NULL; //list of words available for play
static int words = 0; //number of words

static int shmfd = -1; //shared memory file descriptor
static const char *progname = "hangman-server"; //name of the program
static struct comm *shared; //pointer to shared memory
static sem_t *sem_serv;
static sem_t *sem_client;
static sem_t *sem_comm;

static volatile sig_atomic_t want_quit = 0;

/**
  * Credit to the OSUE-Team
  * @brief terminate program on program error
  * @param exitcode exit code
  * @param fmt format string
  */
static void bail_out(int exitcode, const char *fmt, ...);

/**
  * @brief free allocated resources
  */
static void free_resources(void);

/**
  * @brief adds a new client to the list
  * @param c the client to be added
  */
static void add_client(client *c);

/**
  * @brief searches for the client with the specified client number
  * @param cno The number of the client to find
  * @return a pointer to the client, or NULL if not found
  */
static client *find_client(int cno);

/**
  * @brief searches for the client with the specified client number and removes it
  * @param cno The number of the client to remove
  */
static void remove_client(int cno);

/**
  * @brief creates a new client, to be stored in the client list
  * @param client_number The number of the client
  */
static client *create_client(int client_number);

/**
  * @brief allocates all necessary resources
  */
static void allocate_resources(void);

/**
  * @brief waits for semaphore and checks for errors
  * @param sem the semaphore to wait for
  */
static void cwait(sem_t *sem);

/**
  * @brief posts to semaphore and checks for errors
  * @param sem the semaphore to post to
  */
static void cpost(sem_t *sem);

/**
  * @brief frees all stored clients
  */
static void free_client_list(void);

/**
  * @brief frees the word list
  */
static void free_word_list(void);

/**
  * @brief adds a word to the wordlist
  * @param word the word to be added
  */
static void add_word(char *word);

/**
  * @brief Signal handler, gracefully exits the program on SIGINT or SIGTERM
  * @param signal The received signal
  */
static void handle_signal(int signal);

/**
  * @brief Prepares the shared memory for communication with the client
  * @param client The client currently communicating with the server
  */
static void prepare_mem(client *c);

/**
  * @brief checks if a guessed letter is in the clients word;
  * @param guess the guessed letter
  * @param c the client
  * @return 1 if letter is in the word, 0 else
  */
static int valid(char guess, client *c);

/**
  * @brief reveals part of the word
  * @param rword the word to reveal
  * @param cword the word to guess
  * @param letters the already guessed letters
  */
static int reveal(char *cword, char *letters);

/**
  * @brief checks if the guessed_letters array contains a letter
  * @param letter the letter to check for
  * @param arr the array to go through
  * @param length the array length
  */
static int contains(char letter, char *arr);

/**
  * @brief filters non-letter (and non-space) characters out of words
  * @param word the word to filter
  * @param length the length of the word to filter
  * @return a pointer to the filtered string
  */
static char *filter(char *word, int length);

int main(int argc, char *argv[])
{
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction ac;

    ac.sa_handler = handle_signal;
    ac.sa_flags = 0;
    if(sigfillset(&ac.sa_mask) <0) {
        bail_out(EXIT_FAILURE, "sigfillset");
    }

    for(int i=0; i<2; i++) {
        if(sigaction(signals[i], &ac, NULL) < 0) {
            bail_out(EXIT_FAILURE, "sigaction");
        }
    }

    if(argc > 0) {
        progname = argv[0];
    }

    int max_length = 0;
    FILE *f;
    if(argc == 2) {
        f = fopen(argv[1], "r");
        if(f == NULL) {
            bail_out(EXIT_FAILURE, "Invalid input file");
        }
    }
    else if(argc == 1) {
        f = stdin;
    }
    else {
        fprintf(stderr, "Usage: %s: [input_file]", progname);
        exit(EXIT_FAILURE);
    }


    int read=0;
    char *line = (char *)malloc(sizeof(char)*(WORD_LENGTH + 1));
    char *point = NULL;
    if(argc == 1) {
        (void)printf("\nEnter words to guess line by line (quit with CTRL+D): \n");
    }
    while((point = fgets(line, WORD_LENGTH+1, f))!=NULL) {
        fflush(stdin);
        read = strlen(line);
        if(read > max_length) {
            max_length = read;
            if(line[read-1] == '\n') {
                max_length --; //don't count \n
            }
        }
        if(line[read-1] == '\n') {
            line[read-1] = '\0'; //remove trailing \n
        }
        
        add_word(filter(line, read)); 
    }
    if(words == 0) {
        bail_out(EXIT_FAILURE, "Use at least one word");
    }
    (void)printf("\nWaiting for connections\n");
    if(argc == 2) { //only close if it's a file, don't close stdin
        fclose(f);
    }
    if(line)
        free(line);
    


    if(atexit(free_resources) != 0) {
        bail_out(EXIT_FAILURE, "Error setting cleanup function on exit");
    }

    allocate_resources();
    client *c;
    
          
    while(!want_quit) { 
        cwait(sem_serv);
        if(errno == EINTR) continue;

        switch(shared->rtype) {
        case CONNECT:
            c = create_client(hi_client_number);
            shared->cno = hi_client_number;
            hi_client_number++;
            add_client(c);
            cpost(sem_comm);
            cwait(sem_serv);
            if(errno == EINTR) continue;
            break;
        case DISCONNECT:
            remove_client(shared->cno);
            break;
        case NEW:
            c = find_client(shared->cno);
            if(c->used_words == words) {
                shared->rtype = NO_MORE_WORDS;
                cpost(sem_comm);
                remove_client(shared->cno);
                break;
            }
            (void)printf("\nClient %d requests a new game!\n", shared->cno);
            fflush(stdout);
            for(int i=0; i<26; i++) {
                c->guessed_letters[i] = 0;
            }
            (void) strcpy(c->current_word, word_list[c->used_words]);
            c->used_words++;
            c->mistakes = 0;
            cpost(sem_comm);
            break;
        case PLAY:
            c = find_client(shared->cno);
            prepare_mem(c);
            (void)strcpy(shared->word, c->current_word);
            char guess = (char)toupper(shared->guess); 
            (void)printf("Received guess: %d :=> %c, %d, %d\n", c->cno, guess,guess-'A', c->guessed_letters[guess-'A']);
            fflush(stdout);
            c->guessed_letters[guess-'A'] = 1; //index of guessed letter in alphabet
            
            if(valid(guess, c)) {
               printf("Guess was valid!\n");
               int done = reveal(c->current_word, c->guessed_letters);
               if(done == 1) {
                   shared->rtype = WON;
                   c->wins++;
               }
            }
            else {
                if(c->mistakes == 8) {
                    shared->rtype = LOST;     
                     c->losses++;
                }

                c->mistakes++;
                (void)reveal(c->current_word, c->guessed_letters);
            }
            
            prepare_mem(c);
            cpost(sem_comm);
           
            break;
        default:
            assert(0);
        }
        cpost(sem_client);   
    }


    
    return EXIT_SUCCESS;
}


static char *filter(char *line, int length){
    int valids = 0;
    char *filtered = NULL;
    for(int i=0; i<length; i++) {
        if((toupper(line[i])>='A' && toupper(line[i])<='Z') || line[i] == ' ') {
            valids ++;
        }
    }

    filtered = (char *)malloc(sizeof(char)*valids+1);
    int j=0;
    for(int i=0; i<length; i++) {
        if((toupper(line[i])>='A' && toupper(line[i])<='Z') || line[i] == ' ') {
            filtered[j] = toupper(line[i]);
            j++;
        }
    }
    filtered[j] = '\0';
    return filtered;
}
static void cwait(sem_t *sem) {
    if(sem_wait(sem) == -1) {
        if(errno != EINTR) {
            bail_out(EXIT_FAILURE, "Error waiting on semaphore");
        }
    }
}

static void cpost(sem_t *sem) {
    if(sem_post(sem) == -1) {
        bail_out(EXIT_FAILURE, "Error posting to semaphore");
    }
}

static void bail_out(int exitcode, const char *fmt, ...)
{
    va_list ap;
    (void) fprintf(stderr, "%s: ", progname);
    if(fmt != NULL) {
        va_start(ap, fmt);
        (void) fprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if(errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void)fprintf(stderr, "\n");

     
    exit(exitcode);
}

static void free_resources() {
    shared->terminate = 1;
    (void)sem_post(sem_client);
     // while(connected_clients>0) {
     //   printf("ok");
      //  cwait(sem_serv);
      //  remove_client(shared->cno);
   // }
    free_word_list();
    free_client_list();
    if(munmap(shared, sizeof *shared) == -1) {
        (void) fprintf(stderr, "Error unmapping shared memory");
    }
    if(shmfd != -1) {
        if(shm_unlink(SHM_NAME) == -1) {
           (void) fprintf(stderr, "Error unlinking shared memory object");
        }
    }
    if(sem_close(sem_serv) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s", SEM_SERV_NAME);
    }
    if(sem_close(sem_client) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s", SEM_CLIENT_NAME);
    }
    if(sem_close(sem_comm) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s", SEM_COMM_NAME);
    }
    if(sem_unlink(SEM_COMM_NAME) == -1) {
        (void) fprintf(stderr, "Error unlinking semaphore %s", SEM_COMM_NAME);
    }
    if(sem_unlink(SEM_SERV_NAME) == -1) {
        (void) fprintf(stderr, "Error unlinking semaphore %s", SEM_SERV_NAME);
    }
    if(sem_unlink(SEM_CLIENT_NAME) == -1) {
        (void) fprintf(stderr, "Error unlinking semaphore %s", SEM_CLIENT_NAME);
    }
}

static client *create_client(int client_number)
{
    client *newC = (client *)malloc(sizeof(client));
    newC->cno=client_number;
    newC->mistakes = 0;
    newC->used_words = 0;
    newC->wins = 0;
    newC->losses = 0;
    for(int i=0; i<26; i++) {
        newC->guessed_letters[i] = 0;
    }

    return newC;
}

static void add_client(client *c)
{
    client **tmp = (client **)realloc(client_list, (connected_clients+1)*(sizeof(client*)));
    if(tmp == NULL) {
        bail_out(EXIT_FAILURE, "Error extending client list");
    }
    client_list = tmp;
    client_list[connected_clients] = c; //no need to write +1 here, because of zero-based indexing
    connected_clients++;
}

static client *find_client(int cno)
{
    for(int i=0; i<connected_clients; i++) {
        if(client_list[i]->cno == cno) {
            return client_list[i];
        }
    }
    return NULL;
}

static void remove_client(int cno)
{
    for(int i=0; i<connected_clients; i++) {
        if(client_list[i]->cno == cno) {
            free(client_list[i]);
            for(int j=i; j<connected_clients-1; j++) {
                client_list[j] = client_list[j+1];
            }
            printf("%d", cno);
            fflush(stdout);
            connected_clients--;
            client **tmp = (client **)realloc(client_list, (connected_clients)*sizeof(client*));
            if(tmp == NULL && connected_clients != 0) {
                bail_out(EXIT_FAILURE, "Error shrinking client list");
            }
            client_list = tmp;
            break;
        }
    }
}

static void allocate_resources()
{

    shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERM); 
    
    if(shmfd == -1) {
        bail_out(EXIT_FAILURE, "Error creating shared memory");
    }
    
    if(ftruncate(shmfd, sizeof *shared) == -1) {
        bail_out(EXIT_FAILURE, "Error setting size of shared memory");
    }

    shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if(shared == MAP_FAILED) {
        bail_out(EXIT_FAILURE, "Mapping shared memory failed");
    }

    if(close(shmfd) == -1) {
        bail_out(EXIT_FAILURE, "Error closing shared memory file descriptor");
    }

    sem_serv = sem_open(SEM_SERV_NAME, O_CREAT | O_EXCL, PERM, 0);

    if(sem_serv == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error creating semaphore %s", SEM_SERV_NAME);
    }

    sem_client = sem_open(SEM_CLIENT_NAME, O_CREAT | O_EXCL, PERM, 1);
    
    if(sem_client == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error creating semaphore %s", SEM_SERV_NAME);
    }

    sem_comm = sem_open(SEM_COMM_NAME, O_CREAT | O_EXCL, PERM , 0);

    if(sem_comm == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error creating semaphore %s", SEM_COMM_NAME);
    }
    shared->terminate = 0;
}

static void add_word(char *word)
{
    char **tmp = (char**)realloc(word_list, (words+1)*sizeof(char*));
    if(tmp == NULL) {
        bail_out(EXIT_FAILURE, "Error extending word list");
    }
    word_list = tmp;
    word_list[words] = word; 
    words++;
}

static void free_word_list()
{
    for(int i=0; i<words; i++) {
        if(word_list[i] != NULL) {
            free(word_list[i]);
        }
    }
    free(word_list);
    words = 0;
}

static void free_client_list() 
{
    for(int i=0; i<connected_clients; i++) {
        if(client_list[i] != NULL) {
            free(client_list[i]);
        }
    }
    free(client_list);
    connected_clients = 0;
}

static void handle_signal(int signal)
{
    want_quit = 1;
}

static void prepare_mem(client *c)
{
    shared->mistakes = c->mistakes;
    shared->wins = c->wins;
    shared->losses = c->losses;
    for(int i=0; i<26; i++) {
        shared->guessed_letters[i] = c->guessed_letters[i];
    }

}

static int valid(char guess, client *c)
{
    for(int i=0; i<WORD_LENGTH; i++) {
        if(c->current_word[i] == '\0')
            break;
        if(c->current_word[i] == guess) {
            return 1;
        }
    }
    return 0;
}

static int contains(char letter, char *arr)
{

    if(arr[letter-'A'] == 1) {
        return 1;
    }
    return 0;  
}

static int reveal(char *cword, char *letters)
{
    int won = 1;
    for(int i=0; i<WORD_LENGTH; i++) {
        if(cword[i] == '\0') {
            break;
        }
        if(contains(cword[i], letters)) {
            shared->word[i] = cword[i]; 
        }
        else {
            if(cword[i] == ' ') {
                shared->word[i] = ' ';
            }
            else {
                won = 0;
                shared->word[i] = '_';
            }
        }
    }
    return won;
}
