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
#define SHUTDOWN 4

struct comm {
    int rtype;
    int cno;
    int guess;
    int mistakes;
    int guessed_letters[26];
    char *word;
};





#endif
