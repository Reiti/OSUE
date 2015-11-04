/** Mastermind Client
* @file: client.c
* @author Michael Reitgruber
* @date 22.10.2015
* @brief A client for the Mastermind Server
* @details Plays a game of Mastermind against the Server. This client will create the guesses automatically
*/


#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "strategy.h"

#define BUFFER_BYTES (2)
#define COLORS (8)
#define PINS (5)
#define PARITY_SHIFT (15)
#define SHIFT_WIDTH (3)
#define PARITY_ERROR_SHIFT (6)
#define GAME_LOST_SHIFT (7)
struct opts {
    long int portno;
    char *addr;
};

enum color {beige = 0, darkblue, green, orange, red, black, violet, white};

static const char *progname = "client";

void parse_args(int argc, char *argv[], struct opts* arg);
static void bail_out(int exitcode, const char *fmt, ...);
static void free_resources(void);
void send_guess(int *guess);
static uint8_t *receive_answer(int fd, uint8_t *buff);
static int sockfd = -1;


int main(int argc, char *argv[]) 
{
    int initial_guess[PINS] = {beige, beige, darkblue, darkblue, green}; 
    struct opts arg;
    parse_args(argc, argv, &arg);
    struct sockaddr_in sin;
    static uint8_t response[1];
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(arg.portno);
    int red = 0;
    int white = 0;
    int parity_err = 0;
    int lost = 0;
    int rounds_played = 0;
    guess *next = NULL;
    if(inet_pton(AF_INET, arg.addr, &sin.sin_addr) <= 0) {
        bail_out(EXIT_FAILURE, "Invalid server IP");
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        bail_out(EXIT_FAILURE, "Error creating socket");
    }

    if(connect(sockfd, (struct sockaddr *)&sin, sizeof sin) == -1) {
         bail_out(EXIT_FAILURE, "Error connecting socket");
    } 
    next = init_strat(initial_guess);

    do {
        send_guess(next->pattern);
        if(receive_answer(sockfd, response)==NULL) {
             bail_out(EXIT_FAILURE, "Error reading server reply");
        }
        lost = response[0]&(0x1<<GAME_LOST_SHIFT);
        parity_err = response[0]&(0x1<<PARITY_ERROR_SHIFT);
        red = response[0]&0x7;
        white = (response[0]>>SHIFT_WIDTH)&0x7;
        next = next_guess(red, white);
        rounds_played++;
    }
    while(lost == 0 && parity_err == 0 && red != PINS);
    if(lost != 0) {
        if(parity_err != 0) {
            (void)printf("Game lost");
            bail_out(4, "Parity error");
        }
        bail_out(3, "Game lost");
    }
    else if(parity_err != 0) {
        bail_out(2, "Parity error");
    }
    if(red == PINS) {
        (void)printf("%d", rounds_played);
        free_all();
        return EXIT_SUCCESS;
    }          
}

static uint8_t *receive_answer(int fd, uint8_t *buff) 
{
    ssize_t rb;
    rb = recv(fd, buff, 1, 0);
    if(rb <= 0) {
        return NULL;
    }
    return buff;
}
    
void print_bits(uint16_t guess)
{
    for(int i=0; i<sizeof(guess)*8; i++) {
	printf("%d", guess&0x1);
        guess >>= 1;
    }
    printf("\n");
}

void send_guess(int *guess)
{
    uint16_t enc_guess = 0;
    uint8_t parity = 0; 
    int tmp;
    for(int i=1; i<=PINS; i++) {
        enc_guess <<= SHIFT_WIDTH;
        enc_guess += guess[PINS-i];
        tmp = enc_guess & 0x7;
        parity ^= tmp ^ (tmp >> 1) ^ (tmp >> 2);
    }
    parity &= 0x1;
    enc_guess += parity << PARITY_SHIFT;   

    errno = 0;
    send(sockfd, &enc_guess, sizeof(enc_guess), 0);
    if(errno != 0) {
        bail_out(EXIT_FAILURE, "Error sending guess to server!");
    }
}
       

    
static void free_resources(void)
{
    if(sockfd >= 0) {
        (void) close(sockfd);
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
    (void) fprintf(stderr, "\n");
   
    free_resources();
    free_all();
    exit(exitcode);

} 
void parse_args(int argc, char *argv[], struct opts* arg) 
{
    char *endptr;
    if(argc != 3) {
        bail_out(EXIT_FAILURE, "Usage: client <server-hostname> <server-port>");
    }
    
    progname = argv[0];

    errno = 0;
    arg->portno = strtol(argv[2], &endptr, 10);
    if((errno = ERANGE && (arg->portno == LONG_MAX || arg->portno == LONG_MIN)) || (errno != 0 && arg->portno == 0)) {
        bail_out(EXIT_FAILURE, "strtol");
    }

    if(endptr == argv[2]) {
        bail_out(EXIT_FAILURE, "Specified port is not a number");
    }
    
    if(arg->portno <1 || arg->portno > 65535) {
        bail_out(EXIT_FAILURE, "Port is not a valid TCP/IP port");
    }

   arg->addr = argv[1];
   if(strcmp(arg->addr, "localhost") == 0) {
       arg->addr = "127.0.0.1";
   }
}
   

