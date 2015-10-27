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

static guess *all[SOLUTION_SIZE];
static guess *current_guess;

void copy_pattern(guess *a, guess *b);

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

void fill(int idx, int c1, int c2, int c3, int c4, int c5) 
{   
    all[idx] = malloc(sizeof(guess));
    all[idx]->pattern[0] = c1;
    all[idx]->pattern[1] = c2;
    all[idx]->pattern[2] = c3;
    all[idx]->pattern[3] = c4;
    all[idx]->pattern[4] = c5;   
}

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

void copy_pattern(guess *a, guess *b) 
{
    for(int i=0; i<PINS; i++) {
        a->pattern[i] = b->pattern[i];
    }
}

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

void free_all() {
    for(int i=0; i<SOLUTION_SIZE; i++) {
        free(all[i]);
    }
    free(current_guess);
}
