#ifndef HANGMAN_COMMON_H
#define HANGMAN_COMMON_H

#define SHM_NAME "/hangman"
#define PERM (0600)

#define SEM_SERV_NAME "/sem_serv"
#define SEM_CLIENT_NAME "/sem_client"
#define SEM_COMM_NAME "/sem_comm"

#define PLAY 1
#define CONNECT 2
#define DISCONNECT 3
#define NEW 4
#define LOST 5
#define WON 6
#define NO_MORE_WORDS 7
#define SIGDC 8
#define WORD_LENGTH 64

struct comm {
    int rtype;  //what kind of request we are dealing with
    int terminate; //only written by server, read by client
    int cno;
    int wins;
    int losses;
    char guess;
    int mistakes;
    char guessed_letters[26];
    char word[WORD_LENGTH];
};





#endif
