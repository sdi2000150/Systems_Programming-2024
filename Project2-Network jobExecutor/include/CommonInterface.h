#ifndef COMMONINTERFACE_H
#define COMMONINTERFACE_H
/* CommonInterface.h */
#define DFLBUFSIZE 4096             //define default buffer size

//Common Functions (definitions inside CommonFunctions.c)
//(general functions that can by used by any program that finds them useful)
int are_all_digits(const char*);    //used by Server
int is_disallowed_char(char);       //used by Server
void sanitize(char*);               //used by Server
int myread(int, void*, int);        //used by both Server and Commander
void error_exit(char *, int);       //used by both Server and Commander
int myitoa(int, char*);             //used by Server

#endif