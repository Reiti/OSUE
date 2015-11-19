/**
  *Module: Palindrome
  *@author Michael Reitgruber
  *@brief Simulates venting reactor gas
  *@date 11.11.2015
  */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    if(argc > 1) {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }
    int r = 0;
    srand(time(NULL));
    r = rand()%7;
    if(r != 6) {
        (void)printf("STATUS OK\n");
        return EXIT_SUCCESS;
    }
    (void)printf("PRESSURE TO HIGH - IMMEDIATE SHUTDOWN REQUIRED\n");
    return EXIT_FAILURE;
}



