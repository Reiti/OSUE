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
static const char *progname = "hangman-client"; //name of the program
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
static int valid(char c, char *letters);

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

    while(!want_quit) {

       // if(errno == EINTR) continue;


     //   cpost(sem_serv);
     //   cwait(sem_comm);
       // if(errno == EINTR) continue;
        char c;
        do {
            (void)printf("\nEnter next character: ");
            c = (char)fgetc(stdin);
            fflush(stdin);
            char t = (char) fgetc(stdin);
            fflush(stdin);
            if(t != '\n') {
                (void)printf("\nOnly one letter per turn!\n");
                continue;
            }
            
            if(valid(c, guessed_letters)) {
                break;
            }
            (void) printf("\nInvalid letter\n");
            fflush(stdout);
        }while(!want_quit);

        if(want_quit) {
            shared->rtype = SIGDC;
        }
        cwait(sem_client);

        shared->rtype = PLAY;
        shared->cno = cno;
    
        guessed_letters[toupper(c)-'A'] = 1; //mark as already used;
        shared->guess = c;
        cpost(sem_serv);
        
        cwait(sem_comm);
       // if(errno == EINTR) continue;
       

        draw_hangman(shared->mistakes);
        (void) printf("\n");
        (void) printf("%s\n", shared->word); 
        print_letters(shared->guessed_letters);

        if(shared->rtype == WON || shared->rtype == LOST) {
            if(shared->rtype == WON) {
                (void) printf("\nGlückwunsch, du hast gewonnen!\n");
            }
            else {
                (void)printf("\nLeider verloren\n");
            }

            (void) printf("Aktueller Stand: Gewonnen %d, Verloren %d. Nochmal? [y/n]", shared->wins, shared->losses);
            char c = (char)fgetc(stdin);
            (void)fgetc(stdin); //get rid of line feed
            if(tolower(c) == 'y') {
                cwait(sem_client);
                shared->cno = cno;
                for(int i=0; i<26; i++) {
                    guessed_letters[i]=0;
                }
       
                shared->rtype = NEW;
                cpost(sem_serv);
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
        bail_out(EXIT_FAILURE, "Server terminated unexpectedly");
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
        (void) fprintf(stderr, "Error closing semaphore %s", SEM_SERV_NAME);
    }
    if(sem_close(sem_client) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s", SEM_CLIENT_NAME);
    }
    if(sem_close(sem_comm) == -1) {
        (void) fprintf(stderr, "Error closing semaphore %s", SEM_COMM_NAME);
    }
}

static void allocate_resources()
{

    shmfd = shm_open(SHM_NAME, O_RDWR, 0);

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
    
    sem_serv = sem_open(SEM_SERV_NAME, 0);
    if(sem_serv == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error opening semaphore %s", SEM_SERV_NAME);
    }
    
    sem_client = sem_open(SEM_CLIENT_NAME, 0);
    if(sem_client == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error opening semaphore %s", SEM_CLIENT_NAME);
    }

    sem_comm = sem_open(SEM_COMM_NAME, 0);
    if(sem_comm == SEM_FAILED) {
        bail_out(EXIT_FAILURE, "Error opening semaphore %s", SEM_COMM_NAME);
    }
}

static void draw_hangman(int mistakes) 
{
    (void)printf("%d mistakes\n", mistakes);
    /*
    int hangman[7][5];
    (void)memset(hangman, ' ', 7*5);

    switch(mistakes) {
    case 9: hangman[5][2] = '/';
    case 8:
            hangman[7][2] = '\\';
    case 7:
            hangman[6][3] = '|';
    case 6:
            hangman[5][4] = '\\';
    case 5:
            hangman[7][4] = '/';
    case 4:
            hangman[6][4] = 'o';
    case 3:
            hangman[2][5] = '|';
            hangman[3][5] = '-';
            hangman[4][5] = '-';
            hangman[5][5] = '-';
            hangman[6][5] = '|';
    case 2:
            hangman[2][4] = '|';
            hangman[2][3] = '|';
            hangman[2][2] = '|';
    case 1:
            hangman[1][1] = '/';
            hangman[3][1] = '\\';
    case 0: break;
    default: assert(0);
    }

    for(int i=0; i<5; i++) {
        for(int j=0; j<7; j++) {
           (void) printf("%c", hangman[j][i]);
        }
        (void) printf("\n");
    }*/
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

static int valid(char c, char *letters) 
{
    char u = (char) toupper(c);

    if(u>='A' && u <= 'Z') {
        if(!contains(u, letters)) {
            return 1;
        }
    }

    return 0;
}

static void handle_signal(int signal)
{
    want_quit = 1;
}
