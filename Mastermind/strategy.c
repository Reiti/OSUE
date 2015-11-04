/** Mastermind Strategy
* @file: strategy.c
* @author Michael Reitgruber
* @date 22.10.2015
* @brief Implements the strategy for the Mastermind Client
* @details Contains a basic elimination approach strategy for Mastermind
*/

#include "strategy.h"
#include <stdlib.h>
#include <string.h>
#define COLORS (8)
#define PINS (5)
#define SOLUTION_SIZE (8*8*8*8*8)

enum color {beige = 0, darkblue, green, orange, red, black, violet, white};

//array containing all possible guesses
static guess *all[SOLUTION_SIZE];

static guess *current_guess;

void copy_pattern(guess *a, guess *b);

/**
 * @brief Initialise necessary variables and setup Strategy
 * @detail Populizes an array containing all possible combinations of colors, which will be used to determine the solution by eliminating invalid guesses
 * @param start_guess An int array containing the initial guess to seed the next_guess function
 * @return A pointer to the currently selected guess (directly after calling this method it will be equal to the initial guess)
*/
guess *init_strat(int start_guess[]) {
    current_guess = malloc(sizeof(guess));
    int first = 8*8*8*8;
    int second = 8*8*8;
    int third = 8*8;
    int fourth = 8;
    
    for(int h=0; h<PINS; h++) {
        current_guess->pattern[h] = start_guess[h];
    }    
    for(int i=0; i<COLORS; i++) {
        for(int j=0; j<COLORS; j++) {
            for(int k=0; k<COLORS; k++) {
                for(int l=0; l<COLORS; l++) {
                    for(int m=0; m<COLORS; m++) {
                        fill(first*i+second*j+third*k+fourth*l+m, i, j, k, l, m);
                    }
                }
            }
        }
    }
    return current_guess;
}

/**
 * @brief Fills one guess at the index idx with colors c1-5
 * @param idx The index of the guess to fill
 * @param c1 Color of first pin
 * @param c2 Color of second pin
 * @param c3 Color of third pin
 * @param c4 Color of fourth pin
 * @param c5 Color of fifth pin
*/
void fill(int idx, int c1, int c2, int c3, int c4, int c5) 
{   
    all[idx] = malloc(sizeof(guess));
    all[idx]->pattern[0] = c1;
    all[idx]->pattern[1] = c2;
    all[idx]->pattern[2] = c3;
    all[idx]->pattern[3] = c4;
    all[idx]->pattern[4] = c5;   
}

/*
 *@brief plays two guesses against each other using the same method as the server (Credit to OSUE-Team)
 *@detail This is used to determine which guesses to remove from the list. 
 *@param a The guess that will emulate being the solution
 *@param b The guess that will play against the "solution" (guess a)
 *@param res An array containing the number of red pins(res[0]) and white pins(res[1])
*/
void play_against(guess *a, guess *b, int *res)
{   int colors_left[COLORS];
    int red,white;
    int j;

    /* marking red and white */
    (void) memset(&colors_left[0], 0, sizeof(colors_left));
    red = white = 0;
    for (j = 0; j < PINS; ++j) {
        /* mark red */
        if (b->pattern[j] == a->pattern[j]) {
            red++;
        } else {
            colors_left[a->pattern[j]]++;
        }
    }
    for (j = 0; j < PINS; ++j) {
        /* not marked red */
        if (b->pattern[j] != a->pattern[j]) {
            if (colors_left[b->pattern[j]] > 0) {
                white++;
                colors_left[b->pattern[j]]--;
            }
        }
    }
    res[0] = red;
    res[1] = white;
}

/**
 * @brief Copies the pattern from one guess to another
*/
void copy_pattern(guess *a, guess *b) 
{
    for(int i=0; i<PINS; i++) {
        a->pattern[i] = b->pattern[i];
    }
}

/**
 * @brief Returns the next guess to play against the server
 * @detail First eliminates unwanted guesses via the eliminate function, then selects the first valid guess from the remaining
 * @param red Number of red pins returned on last guess
 * @param white Number of white pins returned on last guess
 * @return The next guess to be played
*/
guess *next_guess(int red, int white)
{
   eliminate(red, white);
   for(int i=0; i<SOLUTION_SIZE; i++) {
       if(all[i] != NULL) {
           copy_pattern(current_guess,all[i]);
           return all[i];
       }
   }
   return NULL; 
}

/**
 * @brief Eliminates all invalid guesses from the global array
 * @detail Plays the current guess against all remaining guesses. All guesses which do not return the same number of red an white pins as the server
 *         can not be valid. 
 * @param red Number of red pins returned on last guess
 * @param white Number of white pins returned on last guess
*/
void eliminate(int red, int white) {
    static int buff[2];
    for(int i=0; i<SOLUTION_SIZE; i++) { 
        if(all[i] != NULL) {
            play_against(current_guess, all[i], buff);
        }
        if(buff[0] != red || buff[1] != white) {
            free(all[i]);
            all[i] = NULL;
        }
    }

}

/**
 * @brief Used to free the remaining space allocated by the global array
*/
void free_all() {
    for(int i=0; i<SOLUTION_SIZE; i++) {
        free(all[i]);
    }
    free(current_guess);
}
