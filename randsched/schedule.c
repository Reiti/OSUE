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
#define EXIT_ERROR 13 
#define EXIT_SIGNAL 15
/*program name*/
static const char *program_name = "schedule";

/*file descriptor for pipe*/
static int fd[2];

/*file descriptor for logfile*/
static FILE *file;


/*tells the program to exit gracefully if signal was caught*/
volatile sig_atomic_t quit = 0;

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
  *@return The parsed integer, or -1 on failure
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
  *@return The return value of the emergency program
  **/
static int schedule(int begin, int duration, char *program, char *emergency);

/**
  *@brief Signal handler, exits the program gracefully if SIGINT or SIGTERM are received
  */
static void handle_signal(int signal);

/**
  *@brief frees all allocated resources
  */
static void free_resources(void);

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
    const int signals[] = {SIGINT, SIGTERM};
    struct sigaction ac;

    ac.sa_handler = handle_signal;
    ac.sa_flags = 0;
    if(sigfillset(&ac.sa_mask)<0) {
        bail_out(EXIT_FAILURE, "sigfillset");
    }
    for(int i=0; i < 2; i++) {
        if(sigaction(signals[i], &ac, NULL) < 0) {
            bail_out(EXIT_FAILURE, "sigaction");
        }
    }

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
    int status;
    char buffer[PIPE_BUF];
    int rval;

    switch(pid) {
    case -1:
        bail_out(EXIT_FAILURE, "Error forking");
        break;
    case 0:
        return schedule(begin, duration, program, emergency);
    default:
        file = fopen(logfile, "a+");
        (void) close(fd[1]); //close writing side of pipe

        while(read(fd[0], buffer, PIPE_BUF) > 0) {
           (void) write(fileno(stdout), buffer, PIPE_BUF);
           (void) write(fileno(file), buffer, PIPE_BUF);
        }

        wait(&status);

        if(WIFEXITED(status)) {
            rval = WEXITSTATUS(status);
        }
        else {
            bail_out(EXIT_FAILURE, "Child process did not terminate normally");
        }
        
        char *message;
       
        if(rval == EXIT_SUCCESS) {
            message = "EMERGENCY SHUTDOWN SUCCESSFUL!\n";
        } else if(rval == EXIT_FAILURE) {
            message = "EMERGENCY SHUTDOWN FAILED!\n";
        } else if(rval == EXIT_SIGNAL) {
            message = "USER INITIATED SHUTDOWN COMPLETE\n";
        } else {
            bail_out(EXIT_FAILURE, "Child did not terminate normally");
        }
        (void)printf("%s", message);
        if(fputs(message, file) == EOF) {
            bail_out(EXIT_FAILURE, "Error writing to logfile");
        }
        free_resources();
        return EXIT_SUCCESS;
    }
}

static void free_resources() 
{
    (void)fclose(file);
    (void)close(fd[0]);
    (void)close(fd[1]);
}
static void handle_signal(int signal)
{
    quit = 1;
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
    free_resources();
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

static int schedule(int begin, int duration, char *program, char *emergency)
{
    srand(time(NULL));
    int r = begin + rand()%(duration+1);
    int status;
    int rval=0;

    (void) close(fd[0]); //close read side of pipe
  
    while(!quit && rval != EXIT_FAILURE) {
        (void)sleep(r);
        r = begin + rand()%(duration+1);
        int pid = fork();
        switch(pid) {
        case -1: 
            bail_out(EXIT_FAILURE, "Error forking");
            break;
        case 0:
            if(dup2(fd[1], fileno(stdout)) == -1) {
                bail_out(EXIT_FAILURE, "dup failed");
            }
           
            if(execlp(program, program, NULL) == -1) {
                bail_out(EXIT_FAILURE, "Executing %s failed", program);            
            }
            break;
        default:
            (void) wait(&status);
            if(WIFEXITED(status)) {
                rval = WEXITSTATUS(status);
            }
            else {
                bail_out(EXIT_FAILURE, "Child did not terminate normally");
            }
            break;
        }
    }
    if(quit != 0) {
        return EXIT_SIGNAL;
    }
    (void)close(fd[1]);
    int empid = fork(); 
    switch(empid) {
    case -1:
        bail_out(EXIT_FAILURE, "Error forking");
        break;
    case 0:
        if(execlp(emergency, emergency, NULL) == -1) {
            bail_out(EXIT_FAILURE, "Executing %s failed", emergency);
        }
        break;
    default: 
        (void)wait(&status);
        if(WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        else {
            bail_out(EXIT_FAILURE, "%s did not terminate normally", emergency);
        }
    }
    return EXIT_ERROR;
}
