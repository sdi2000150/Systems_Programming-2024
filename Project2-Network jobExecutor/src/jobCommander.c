#define _GNU_SOURCE
/* jobCommander.c */
#include <stdio.h>
#include <unistd.h>         //getpid(),write(),read(),close()
#include <sys/types.h>      //sockets
#include <sys/socket.h>	    //sockets
#include <netinet/in.h>	    //internet sockets
#include <netdb.h>	        //gethostbyname(),struct hostent
#include <stdlib.h>         //exit(),atoi()
#include <string.h>         //memcpy(),strlen(),strcat(),strcmp()

#include "../include/CommonInterface.h"


/* exit codes on errors:
general: -1
open()/close(): 1
write(): 2
read(): 3
socket()/connect(): 5

gethostbyname(): 11
write()/SIGPIPE/BROKENPIPE: 12
*/


int main(int argc, char *argv[]) {

    if (argc < 4) {
        printf("COMMANDER %d: Usage: %s [serverName] [portNum] [jobCommanderInputCommand]\n", getpid(), argv[0]);
        exit(-1);
    }

    //Declare types needed for the connection
    short unsigned int port;
    int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *hostentry;

	//Create socket (TCP over Internet)
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) { error_exit("COMMANDER: socket() failed", 5); }

	//Find server address
    if ((hostentry = gethostbyname(argv[1])) == NULL) { error_exit("COMMANDER: gethostbyname() failed", 11); }

    //Fill the server's struct of net info
    port = (short unsigned int) atoi(argv[2]);              //convert port number to short integer
    server.sin_port = htons(port);                          //define it as server's port, and use hton convertion
    server.sin_family = AF_INET;                            //server is in internet domain
    memcpy(&server.sin_addr, hostentry->h_addr_list[0], (size_t)hostentry->h_length);   //server's IP address

    //Initialize connection by connecting to server
    if (connect(sock, serverptr, sizeof(server)) < 0) { error_exit("COMMANDER: connect() failed", 5); }


////////////////////////// SEND command TO SERVER //////////////////////////

    //Compute the size of the command (with spaces as delimeters)
    size_t command_size = 0;
    for (int i = 3; i < argc; i++) {
        command_size += strlen(argv[i]);
        if (i != argc-1) {                                  //in order to count a space after each arg, except last arg
            command_size++;                                 //+1 for each space delimeter
        }
    }
    command_size++;                                         //+1 for the, last, '\0'

    //Produce the whole command as string
    char concatenated_args[command_size];
    concatenated_args[0] = '\0';                            //initialize it
    for (int i = 3; i < argc; i++) {
        strcat(concatenated_args, argv[i]);
        if (i != argc-1) {                                  //put a space after each arg, except last arg
            strcat(concatenated_args, " ");
        }
    } 

    //Compute the total size of the message to be sent
    size_t complete_message_size = sizeof(uint32_t) + command_size;         //uint32_t because integer is going to be hton converted
    //And produce the whole message as one string (commandsize + command)
    char message[complete_message_size];                                    //to hold the complete message
    uint32_t command_size_net = htonl((uint32_t)command_size);              //use hton convertion
    memcpy(message, &command_size_net, sizeof(uint32_t));                   //the first bytes of the message will hold the size of the rest message
    memcpy(message + sizeof(uint32_t), concatenated_args, command_size);    //rest message (the significant one)

    //Write the whole message with an, atomic, write into the socket
    if (write(sock, message, complete_message_size) == -1) { error_exit("COMMANDER: write command to socket failed", 2); }


////////////////////////// RECEIVE response FROM SERVER //////////////////////////

    //Read the size of the response to be read the socket
    uint32_t size_to_receive_net;
    if (myread(sock, &size_to_receive_net, sizeof(uint32_t)) == -1) { perror("COMMANDER: myread(size_to_receive) function failed"); exit(3); }

    //Read the actual response from the socket
    int size_to_receive = (int) ntohl(size_to_receive_net);  //convert the size from network byte order to host byte order
    char response[size_to_receive];
    if (myread(sock, response, size_to_receive) == -1) { error_exit("COMMANDER: myread(response) function failed", 3); }
    // printf("COMMANDER %d: Response from Server is: \n", getpid());
    printf("%s\n", response);

    //If command is issueJob then do another one extra read for the command's output
    if (strcmp(argv[3], "issueJob") == 0) {
        // printf("COMMANDER %d: Second response from Server (issueJob's output) is:\n", getpid());
        //Read the 2nd response from the socket
        ssize_t bytes_received;
        char second_response[DFLBUFSIZE];
        while ((bytes_received = read(sock, second_response, DFLBUFSIZE)) > 0) {
            if (bytes_received == -1) { error_exit("COMMANDER: read(second_response) failed", 3); }
            second_response[bytes_received] = '\0';             //null-terminate the received data
            printf("%s", second_response);                      //print received output to stdout
        }
    }
    
    //Close socket
    if (close(sock) == -1) { error_exit("COMMANDER: close(sock) failed", 1); }

    //Exit the Commander
    exit(0);
}