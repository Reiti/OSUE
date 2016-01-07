/** hangman-client
  * @author Michael Reitgruber, 1426100
  * @brief Implements a client for the game hangman
  * @detail Communicates to the server via Linux shared memory,  allows to play until the server runs out of words
  * @date 08.12.2015
  **/


#include "hangman-common.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <semaphore.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

static int shmfd = -1;
static const char *progname = "./hangman-client"; //name of the program
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
  * @brief allocate necessary resources
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
  * @brief Signal handler, exits the program gracefully on SIGINT and SIGTERM
  * @param signal The signal to handle
  */
static void handle_signal(int signal);


/**
  * @brief draws the hangman 
  * @param mistakes How much of the hangman should be drawn
  */
static void draw_hangman(int mistakes);


/**
  * @brief prints the list of already guessed letters
  * @param letters The list of letters
  */
static void print_letters(char *letters);

/**
  * @brief checks if a character is a valid choice
  * @param c the character in question
  * @param letters array containing guessed letter information
  */
static int valid(int c, char *letters);

/**
  * @brief checks if the letter array contains a given char
  * @param c the char to check for, must be a valid uppercase letter (no umlaut)
  * @param letters array containig guessed letter information
  */
static int contains(char c, char *letters);

int main(int argc, char *argv[]) 
{ 
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction ac;

    ac.sa_handler = handle_signal;
    ac.sa_flags = 0;
    if(sigfillset(&ac.sa_mask) < 0) {
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
    
    if(argc != 1) {
        (void) fprintf(stderr, "Usage: %s", progname);
        exit(EXIT_FAILURE);
    }
    if(atexit(free_resources) != 0) {
        bail_out(EXIT_FAILURE, "Error setting cleanup function on exit");
    }

    allocate_resources();


    int cno = 0;
    char guessed_letters[26]={0};

    /* Connect to server */
    cwait(sem_client);

    shared->rtype = CONNECT;

    cpost(sem_serv);
    cwait(sem_comm);

    cno = shared->cno;

    cpost(sem_serv);

    cwait(sem_client);

    shared->cno = cno;
    shared->rtype = NEW;

    cpost(sem_serv);
    cwait(sem_comm);
    while(!want_quit) {
        int c = 0;
        do {
            if(c != '\n') {
               (void)printf("\nEnter next character: ");
            }
            c = fgetc(stdin);
            if(want_quit)
                break;
            (void) fflush(stdin);
            if(c == '\n')
                continue;

            int t;
            while((t = fgetc(stdin)!='\n')); 

            if(valid(c, guessed_letters)) {
                break;
            }
            (void) printf("\nInvalid input (only 1 ascii letter per turn or letter already played)\n");
            (void) fflush(stdout);
        }while(!want_quit);

        if(want_quit) {
            break; 
        }
        cwait(sem_client);
        if(errno == EINTR) continue;

        shared->rtype = PLAY;
        shared->cno = cno;
    
        guessed_letters[toupper(c)-'A'] = 1; //mark as already used;
        shared->guess = (char)c;
        cpost(sem_serv);
        
        cwait(sem_comm);
        if(errno == EINTR) continue;
       

        draw_hangman(shared->mistakes);
        (void) printf("\n");
        (void) printf("%s\n", shared->word); 
        print_letters(shared->guessed_letters);

        if(shared->rtype == WON || shared->rtype == LOST) {
            if(shared->rtype == WON) {
                (void) printf("\nCongratulations, you won!\n");
            }
            else {
                (void)printf("\nYou lost!\n");
            }

            (void) printf("Standings: Won %d, Lost %d. Again? [y/n]", shared->wins, shared->losses);
            char c = (char)fgetc(stdin);
            (void)fgetc(stdin); //get rid of line feed
            if(tolower(c) == 'y') {
                cwait(sem_client);
                if(errno == EINTR) continue;
                shared->cno = cno;
                for(int i=0; i<26; i++) {
                    guessed_letters[i]=0;
                }
       
                shared->rtype = NEW;
                cpost(sem_serv);
                cwait(sem_comm);
                if(errno == EINTR) continue;
                if(shared->rtype==NO_MORE_WORDS) {
                    (void)printf("\nNo words left!\n");
                    (void)printf("Final standings: Won %d, Lost %d.\n", shared->wins, shared->losses);
                    break;
                }
                continue;
            }
            else {
                break;
            }
        }

    }

    /* Disconnect from server */
    cwait(sem_client);

    shared->cno = cno;
    shared->rtype = DISCONNECT;
    cpost(sem_serv);

    return EXIT_SUCCESS;
}

static void cwait(sem_t *sem) {
    if(sem_wait(sem) == -1) {
        if(errno != EINTR) {
            bail_out(EXIT_FAILURE, "Error waiting on semaphore");
        }
    }
    if(shared->terminate == 1) {
        cpost(sem_client);
        (void) fprintf(stderr, "\nServer terminated unexpectedly\n");
        exit(EXIT_SUCCESS);
    }
}

static void cpost(sem_t *sem) {
    if(sem_post(sem) == -1) {
        bail_out(EXIT_FAILURE, "Error posting to semaphore");
    }
}

static void bail_out(int exitcode, const char *fmt, ...) {
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
    if(munmap(shared, sizeof *shared) == -1) {
        (void) fprintf(stderr, "Error unmapping shared memory");
    }
    if(sem_close(sem_serv) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s\n", SEM_SERV_NAME);
    }
    if(sem_close(sem_client) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s\n", SEM_CLIENT_NAME);
    }
    if(sem_close(sem_comm) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s\n", SEM_COMM_NAME);
    }
}

static void allocate_resources()
{

    shmfd = shm_open(SHM_NAME, O_RDWR, 0);

    sem_serv = sem_open(SEM_SERV_NAME, 0);
    if(sem_serv == SEM_FAILED) {
        if(errno == ENOENT) {
            (void) fprintf(stderr, "Server not running\n");
            exit(EXIT_FAILURE);
        }
    }
    
    sem_client = sem_open(SEM_CLIENT_NAME, 0);
    if(sem_client == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error opening semaphore %s", SEM_CLIENT_NAME);
    }

    sem_comm = sem_open(SEM_COMM_NAME, 0);
    if(sem_comm == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error opening semaphore %s", SEM_COMM_NAME);
    }

    if(shmfd == -1) {
        bail_out(EXIT_FAILURE, "Error accessing shared memory");
    }

    shared = mmap(NULL, sizeof *shared, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if(shared == MAP_FAILED) {
        bail_out(EXIT_FAILURE, "Mapping shared memory failed");
    }

    if(close(shmfd) == -1) {
        bail_out(EXIT_FAILURE, "Error closing shared memory file descriptor");
    }
    
}
static void draw_hangman(int mistakes) 
{
    //(void)printf("%d mistakes\n", mistakes);
    
    int hangman[7][5];
    for(int i=0; i<5; i++) {
        for(int j=0; j<7; j++) {
            hangman[j][i] = ' ';
        }
    }

    switch(mistakes) {
    case 9: hangman[4][1] = '/';
    case 8:
            hangman[6][1] = '\\';
    case 7:
            hangman[5][2] = '|';
    case 6:
            hangman[4][3] = '\\';
    case 5:
            hangman[6][3] = '/';
    case 4:
            hangman[5][3] = 'o';
    case 3:
            hangman[1][4] = '|';
            hangman[2][4] = '-';
            hangman[3][4] = '-';
            hangman[4][4] = '-';
            hangman[5][4] = '|';
    case 2:
            hangman[1][3] = '|';
            hangman[1][2] = '|';
            hangman[1][1] = '|';
    case 1:
            hangman[0][0] = '/';
            hangman[2][0] = '\\';
    case 0: break;
    default: assert(0);
    }

    for(int i=0; i<5; i++) {
        for(int j=0; j<7; j++) {
           (void) printf("%c", hangman[j][4-i]);
        }
        (void) printf("\n");
    }
}

static void print_letters(char *letters)
{
    (void) printf("Used letters: ");
    for(int i=0; i<26; i++) {
        if(letters[i] == 1) {
            (void) printf("%c", 'A'+i); //prints the i-th letter of the alphabet, when it has already been used
        }
    }
}

static int contains(char c, char *letters) 
{
   if(letters[c-'A'] == 1) { //c - 'A' gives the index of the letter in the alphabet
       return 1;
   }
   return 0;
}

static int valid(int c, char *letters) 
{
    int u =  toupper(c);

    if(u>='A' && u <= 'Z') {
        if(!contains((char)u, letters)) {
            return 1;
        }
    }

    return 0;
}

static void handle_signal(int signal)
{
    want_quit = 1;
}
