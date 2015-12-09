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

static int shmfd = -1;
static const char *progname = "hangman-client"; //name of the program
static struct comm *shared; //pointer to shared memory
static sem_t *sem_serv;
static sem_t *sem_client;
static sem_t *sem_comm;

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

int main(int argc, char *argv[]) 
{  
    if(argc > 0) {
        progname = argv[0];
    }

    if(atexit(free_resources) != 0) {
        bail_out(EXIT_FAILURE, "Error setting cleanup function on exit");
    }

    allocate_resources();
    printf("waiting for sending rights\n"); 
    fflush(stdout);
    cwait(sem_client);
    printf("sending connect\n");
    fflush(stdout);
    shared->rtype = CONNECT;
    cpost(sem_serv);
    printf("waiting for cno\n");
    fflush(stdout);
    cwait(sem_comm);
    (void)printf("%d",shared->cno);


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
