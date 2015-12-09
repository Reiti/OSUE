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
    char *current_word; 
    int guessed_letters[26];
}client; //structure which holds client information

static client **client_list = NULL; //list of connected clients
static int connected_clients = 0; //number of connected clients
static int hi_client_number=0; //current highest client number

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
client *create_client(int client_number);

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

int main(int argc, char *argv[])
{ 
    if(argc > 0) {
        progname = argv[0];
    }
    
    if(atexit(free_resources) != 0) {
        bail_out(EXIT_FAILURE, "Error setting cleanup function on exit");
    }

    allocate_resources();
    client *c;
    
          
    while(!want_quit) { 
        cwait(sem_serv);
        switch(shared->rtype) {
        case CONNECT:
            c = create_client(hi_client_number);
            shared->cno = hi_client_number;
            hi_client_number++;
            add_client(c);
            cpost(sem_comm);
            break;
        case DISCONNECT:
            remove_client(shared->cno);
            break;
        case PLAY:
            //TODO: Insert play logic here;
            break;
        default:
            assert(0);
        }
        cpost(sem_client);   
    }


    
    return EXIT_SUCCESS;
}

static void cwait(sem_t *sem) {
    if(sem_wait(sem) == -1) {
        bail_out(EXIT_FAILURE, "Error waiting on semaphore");
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

client *create_client(int client_number)
{
    client *newC = (client *)malloc(sizeof(client));
    newC->cno=client_number;
    newC->mistakes = 0;
    newC->used_words = 0;
    newC->current_word = NULL;
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
            client **tmp = (client **)realloc(client_list, (connected_clients-1)*sizeof(client*));
            if(tmp == NULL) {
                bail_out(EXIT_FAILURE, "Error shrinking client list");
            }
            client_list = tmp;
            connected_clients--;
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
}
