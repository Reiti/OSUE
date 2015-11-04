/**
  *  Module: Palindrome
  *  @file ispalindrome.c
  *  @author Michael Reitgruber
  *  @brief Check if a given String is a palindrome
  *  @details Ceck if a given String is a palindrome. Possible command line options: -i to ignore case, -s to ignore spaces.
  *  @date 12.10.2015
  */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ispalindrome.h"

//Max length of string 
#define MAX_LENGTH 40

/**
* The main entry point of the program
* @param argc The number of command-line parameters
* @param argv Array of command-line parameters
* @return The exit code. 0 on sucess, no-zero on failure
*/
int main(int argc, char **argv)
{
    //+2 for linebreak and NULL terminator
    char input[MAX_LENGTH+2];
    int iSet = 0;
    int sSet = 0;
    int c;
    while((c = getopt(argc, argv, "is")) != -1){
        switch(c){
            case 'i':
                iSet = 1;
                break;
            case 's': 
                sSet = 1;
                break;
            case '?':
                (void)fprintf(stderr, "Usage: ispalindrome [-i] [-s]\n");
            default:  
                return 1;
        }
    }
    do{
        (void)fgets(input, MAX_LENGTH+2, stdin);
        removeNewLine(input);
        if(strlen(input)>MAX_LENGTH) {
            (void)fprintf(stderr, "ispalindrome: Eingabe zu lang, max %d Zeichen!", MAX_LENGTH);
            return 1;
        }
        (void)printf("%s ist ", input);
        if(iSet){
            toLower(input);
        }
        if(sSet){
            removeSpaces(input);
        }
        if(palindrome(input)){
            (void)printf("ein Palindrom\n");
        }
        else{
            (void)printf("kein Palindrom\n");
        }   
        
    }
    while(1);

    return 0;
}

/**
*@brief Removes he newline character from a string
*@param string The string to remove the newline from
*/
void removeNewLine(char *string)
{
    int size = strlen(string) - 1;
    if(string[size] == '\n'){
        string[size] = '\0';
    }
}

/**
*@brief Turns a string into lower case
*@param string The string to be turned into lower case
*/
void toLower(char *string)
{
    for(int i=0; string[i] != '\0'; i++){
        if(string[i]>=65 && string[i]<=90){
            string[i] = string[i]+32;
        }
    }
}

/**
*@brief Removes Spaces from a string
*@param string The string to remove the spaces from
*/
void removeSpaces(char *string)
{
    int i,j;
    for(i=0, j=0; string[i+j] != '\0'; i++){
        if(string[i+j] == ' '){
            j++;
        }
        string[i]=string[i+j];
    }
    string[i] = '\0';
}

/**
*@brief Checks if a given string is a palindrome
*@details Checks if the string is a palindrome by iterating from both sides and checking for equality
*@param string The string to be checked
*/
int palindrome(char *string)
{
    int len = strlen(string);
    for(int i=0; i<len/2; i++){
        if(string[i] != string[(len-1) - i]){
            return 0;
        }
    }
    return 1;
}

