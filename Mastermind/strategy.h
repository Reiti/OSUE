/** Mastermind Client
* @file: strategy.h
* @author Michael Reitgruber
* @date 22.10.2015
*/

#ifndef STRATEGY_H
#define STRATEGY_H

typedef struct {
    int pattern[5];
} guess;




guess *init_strat(int *start_guess);
void play_against(guess *a, guess *b, int *res);
guess *next_guess(int red, int white);
void eliminate(int red, int white);
void fill(int idx, int c1, int c2, int c3, int c4, int c5);
void free_all();

#endif

