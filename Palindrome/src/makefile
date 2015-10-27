#Author: Michael Reitgruber
#IsPalindrome

CC = gcc
DEFS = -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS = -Wall -g -std=c99 -pedantic $(DEFS)

OBJECTFILES = ispalindrome.o

.PHONY: all clean

all: ispalindrome

ispalindrome: $(OBJECTFILES)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

ispalindrome.o: ispalindrome.c ispalindrome.h
 

clean:
	rm -f $(OBJECTFILES) ispalindrome
