/**
 *   Module: Palindrome
 *   @file: ispalindrome.h
 *   @author: Michael Reitgruber
 *   @brief: Check if a given String is a palindrome
 *   @details: Ceck if a given String is a palindrome. Possible command line options: -i to ignore case, -s to ignore spaces.
 *   @date: 12.10.2015
 */

#ifndef ISPALINDROME_H
#define ISPALINDROME_H

void toLower(char *string);
void removeSpaces(char *string);
void removeNewLine(char *string);
int palindrome(char *string);

#endif
