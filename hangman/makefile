#Author: Michael Reitgruber
#Mastermind

CC = gcc
DEFS = -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)
LDFLAGS = -lrt -pthread

SERVEROBJECTS = hangman-server.o
CLIENTOBJECTS = hangman-client.o 
.PHONY: all clean

all: hangman-server hangman-client 

hangman-client: $(CLIENTOBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

hangman-server: $(SERVEROBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

hangman-server.o: hangman-server.c hangman-common.h

hangman-client.o: hangman-client.c hangman-common.h



clean:
	rm -f $(CLIENTOBJECTS) $(SERVEROBJECTS) hangman-server hangman-client
