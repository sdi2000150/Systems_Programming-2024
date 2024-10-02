#define _GNU_SOURCE
/* CommonFunctions.c  */
#include <stdio.h>
#include <stdlib.h>         //exit()
#include <unistd.h>         //read()
#include <string.h>         //strlen()
#include <netdb.h>          //herror()
#include <errno.h>          //errno
#include <ctype.h>          //isdigit()

#include "../include/CommonInterface.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Custom isdigit function for strings (sequence of chars)
//(used by server's controller thread(s), in setConcurrency command)
int are_all_digits(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;                                           //empty string or NULL is not a valid number
    }
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit((unsigned char)str[i])) {
            return 0;                                       //found a non-digit character, return false
        }
    }
    return 1;                                               //all characters are digits, return true
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Function to check if a character is disallowed (used by sanitize function)
int is_disallowed_char(char c) {
    const char disallowed[] = ";|&`><$(){}[]*?~";           //hard-code the disallowed characters
    for (int i = 0; i < (int)strlen(disallowed); i++) {     //traverse all of them 
        if (c == disallowed[i]) {                           //and check if the char given equals to one of them
            return 1;                                       //if char equals a disallowed one, return true
        }
    }
    return 0;                                               //char is allowed, return false
}
//This sanitize function will be used by server's worker thread(s), when receiving messages-to-run
//(a server will exec whatever command received from client, so we want to skip some unsafe symbols-commands)
//(the bellow function is a variant of the one shown in slides)
void sanitize(char *str) {
    char *src, *dest;
    for (src = dest = str; *src; src++) {                   //traverse the whole string, char-by-char
        if (!is_disallowed_char(*src)) {                    //characters which are allowed, pass
            *dest++ = *src;
        }
    }
    *dest = '\0';                                           //end the string correctly
    return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//My Read function, using a loop for the correct reading
int myread(int fd, void* buf, int msg_size) {
    size_t to_read = (size_t) msg_size;
    ssize_t rsize = 0;
    do {
        rsize = read(fd, buf, to_read);                     //read
        if (rsize == -1) {  
            perror("myread function: read() failed");
            return -1;
        }
        to_read = to_read - (size_t) rsize;                 //decrease the bytes_to_be_read with current_read_size
    } while (to_read > 0);                                  //repeat until have read all bytes asked (until bytes_to_be_read becomes 0)

    return 0;                                               //return 0, meaning myread was successful
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Error printing and exiting function
void error_exit(char *message, int exit_code) {
    if (exit_code == 11) {                                  //gethostbyname() error checking
        herror(message);
    } else if (exit_code == 12) {                           //write() error checking when probable SIGPIPE
        perror(message);
        if (errno == EPIPE) {
            printf("Broken pipe (EPIPE) error handled, not quiting\n");
            return;
        }
    } else if (exit_code == 13) {                           //strchr()/snprintf() error checking
        fprintf(stderr,"%s: exiting\n", message);
    } else if (exit_code == 7 || exit_code == 8 || exit_code || 9) {    //pthread calls error checking
        fprintf(stderr,"%s: exiting\n", message);   //or maybe use strerror()?
    } else {                                                //general error checking
        perror(message);
    }
    exit(exit_code);                                        //exit with exit code specified
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Integer to Ascii conversion (only for positive numbers)
int myitoa(int number, char* str) {

    if (number == 0) {
        //If number is 0, "0" is returned
        str[0] = '0';
        str[1] = '\0';
        return 2;                                           //return the length of the string (including '\0' in the length)

    } else if (number < 0) {
        printf("myitoa function cannot handle negative numbers\n");
        return -1;

    } else { //if (number > 0)

        //Transform the number to individual digits (reversed)
        int i = 0;
        while (number != 0) {
            int rest = number % 10;                         //mod 10 to get each digit
            str[i++] = rest + '0';                          //+'0' so it will be the ascii char of the desired digit
            number = number / 10;                           //rest number, except th specific digit which just got woth mod 10, is / 10
        }
        str[i] = '\0';                                      //add the '\0' and the end of the string

        //Reverse the string of digits so to be in correct order
        int start = 0;
        int end = i - 1;
        while (start < end) {
            char temp = str[start];
            str[start] = str[end];
            str[end] = temp;
            start++;
            end--;
        }
        return i;                                           //return the length of the string (including '\0' in the length)
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////