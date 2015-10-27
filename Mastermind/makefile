#Author: Michael Reitgruber
#Mastermind

CC = gcc
DEFS = -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

SERVEROBJECTS = server.o
CLIENTOBJECTS = client.o strategy.o

.PHONY: all clean

all: server client

client: $(CLIENTOBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

server: $(SERVEROBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

server.o: server.c

client.o: client.c
 

clean:
	rm -f $(CLIENTOBJECTS) $(SERVEROBJECTS) server client
