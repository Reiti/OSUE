#Author: Michael Reitgruber
#Mastermind

CC = gcc
DEFS = -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

SCHEDOBJECTS = schedule.o

.PHONY: all clean

all: schedule rventgas rshutdown

sched: $(SCHEDOBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

rventgas.o: rventgas.c

rshutdown.o: rshutdown.c
 

clean:
	rm -f $(SCHEDOBJECTS) rventgas.o rshutdown.o rventgas rshutdown schedule
