/**Module: schedule
  *@author Michael Reitgruber
  *@brief Implements a random scheduler
  *@detail The program spawns a childprocess at specified random intervals that executes a program given as parameter (output 
  *        will be logged, in case of failure the emergency program will be started (successfull execution will be logged) 
  *@date 11.11.2015
  */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

/*Buffersize for pipe*/
#define PIPE_BUF 1 

/*program name*/
static const char *program_name = "schedule";

/*file descriptor for pipe*/
static int fd[2];
/*file descriptor for logfile*/
static int file;
/**
  *Credit to OSUE team
  *@brief terminate program on program error
  *@param exitcode exit code
  *@param fmt format string
  */
static void bail_out(int exitcode, const char *fmt, ...);

/**
  *@brief Parses a given string to an integer
  *@param string The string to be parsed
  @return The parsed integer, or -1 on failure
  */
static int parse_int(char *string);

/**
  *@brief Prints correct usage of program to stderr
  */
static void usage(void);


/**
  *@brief The procedure representing the scheduling childprocess
  *@detail Schedules the execution of program every begin to (begin + duration) seconds
  *@param begin Beginning time of the execution window in seconds
  *@param duration Length of the execution window in seconds
  *@param program The program to be executed
  *@param emergency The program to be executed in case program fails
  **/
static void schedule(int begin, int duration, char *program, char *emergency);

int main(int argc, char *argv[])
{
    int opt_s = 0;
    int opt_f = 0;
    int begin=1;
    int duration=0;
    char *program;
    char *emergency;
    char *logfile;
    int c;
    if(argc > 0) {
        program_name = argv[0];
    }
    while((c = getopt(argc, argv, "s:f:")) != -1) {
        switch(c) {
        case 's':
            if(opt_s != 0) {
                usage();
                return EXIT_FAILURE;
            }
            opt_s = 1;
            if(optarg != NULL && (begin = parse_int(optarg)) == -1) {
                bail_out(EXIT_FAILURE, "Argument for -s not an integer");
            }            
            break;
        case 'f':
            if(opt_f != 0) {
                usage();
                return EXIT_FAILURE;
            }
            opt_f = 1;
            if(optarg != NULL && (duration = parse_int(optarg)) == -1) {
                bail_out(EXIT_FAILURE, "Argument for -f not an integer");
            }
            break;
        case '?':
            usage();
            return EXIT_FAILURE;
            break;
        default: assert(0);
        }
    }
    if((argc - optind) != 3) {
        usage();
        return EXIT_FAILURE;
    }
    program =argv[optind];
    emergency = argv[optind+1];
    logfile = argv[optind+2];

    if(pipe(fd) != 0) {
        bail_out(EXIT_FAILURE, "Error creating pipe");
    }
    int pid = fork();
    if(pid == 0) {
        schedule(begin, duration, program, emergency);
        
        return EXIT_SUCCESS;
    }
    else if(pid >0) {
        int status;
        char buffer[PIPE_BUF];
        file = open(logfile, O_CREAT|O_RDWR|O_APPEND);
        (void) close(fd[1]); //close writing side of pipe

        while(read(fd[0], buffer, PIPE_BUF) > 0) {
           (void) write(fileno(stdout), buffer, PIPE_BUF);
           (void) write(file, buffer, PIPE_BUF);
        }
        close(fd[0]); //close reading side, because we are done
        int i = wait(&status);

        close(file);
        return EXIT_SUCCESS;
    } 
    else {
        bail_out(EXIT_FAILURE, "Error forking");
    }
}

static void bail_out(int exitcode, const char *fmt, ...)
{
    va_list ap;

    (void) fprintf(stderr, "%s: ", program_name);
    if(fmt != NULL)  {
        va_start(ap, fmt);
        (void) vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    if(errno != 0) {
        (void) fprintf(stderr, ": %s", strerror(errno));
    }
    (void) fprintf(stderr, "\n");

    exit(exitcode);
}

static void usage()
{
    (void) fprintf(stderr, "Usage: %s [-s <seconds>] [-f <seconds>] <program> <emergency> <logfile>\n", program_name);
}

static int parse_int(char *string)
{
    int ret = 0;
    errno = 0;
    ret = strtol(string, NULL, 10);
    if((errno != 0 && ret ==0)) {
        return -1;
    }
    return ret;
}

static void schedule(int begin, int duration, char *program, char *emergency)
{
    srand(time(NULL));
    int r = begin + rand()%(duration+1);
    int s = 0;
    int childpipe[2];
    (void) close(fd[0]); //close read side of pipe
    if(pipe(childpipe) != 0) {
        bail_out(EXIT_FAILURE, "Error creating pipe");
    }

    int pid = fork();

    if(pid == 0) {
        (void) close(childpipe[0]); //close read side of pipe
        if(dup2(childpipe[1], fileno(stdout)) == -1) {
            bail_out(EXIT_FAILURE, "dup failed");
        }
        if(execlp(program, program, NULL) == -1) {
            bail_out(EXIT_FAILURE, "Executing %s failed", program);            
        }
    } 
    else if(pid > 0) {
        int status;
        char buffer[PIPE_BUF];
        (void) close(childpipe[1]); //close write side of pipe

        while(read(childpipe[0], buffer, PIPE_BUF) > 0) {
            (void) write(fd[1], buffer, PIPE_BUF);

        }

        (void) wait(&status);

    }
    else {
        bail_out(EXIT_FAILURE, "Error forking");
    }


   /* while(1) {
        if(s == r) {
            r = begin + rand()%(duration+1);
            
        }

        printf("%d", s);
        sleep(1);
        s++;
    }*/
}
