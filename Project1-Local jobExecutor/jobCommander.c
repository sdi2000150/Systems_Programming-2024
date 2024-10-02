#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "MyHeader.h"

/* exit codes on errors:
open()/creat(): 1 
write(): 2
read(): 3
fork(): 4
exec*(): 5
kill(): 6
mkfifo(): 7
close(): 8
unlink(): 9
sigaction()/sigprocmask(): 10
others: 11 or -1
*/


/* FIFO pipes names. They can be changed from here to whichever name desired. */

//Named pipe for Commanders(Clients)-to-Server communication:
//Only one, shareable, pipe will exist for the Clients->Server communication.
char* ClientsToSerFIFO = "ClientsToSerFIFO";

//Named pipe for Server-to-Commander(Client) communication:
//Each Client will have his own Server->Client pipe. This is the template of that name,
//the real form will be "ServerToClFIFOtemplate" + "client's pid" (e.g ServerToClFIFO46785).
char* ServerToClFIFOtemplate = "ServerToClFIFO";


//flag(true-false) for knowing if SIGUSR1 was already sent by Server, before reaching the pause()
volatile sig_atomic_t signal_send = 0;                      //volatile: for not optimizing actions over it, sig_atomic_t: to be read/written atomicly

void SIGUSR1_handler(/*int signo*/) {
    //Ignore the default action of the signal SIGUSR1

    // const char *message = "CLIENT: inside handler of SIGUSR1\n";
    // if (write(1, message, strlen(message)) == -1) { perror("CLIENT: write() to stdout failed"); exit(2); }   //write to stdout

    signal_send = 1;                                        //SIGUSR1 was just captured, flag set to 1
}


//////////////////////////////////////////////////////////////////// Main Program ////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {

    //Set the handler for the SIGUSR1 signal
    static struct sigaction act1;
    act1.sa_handler = SIGUSR1_handler;
    sigfillset(&(act1.sa_mask));                            //fillset with all signals, so that handler is safe (not interrupted) while operating
    act1.sa_flags = SA_RESTART;                             //set SA_RESTART flag, so that interrupted syscalls to restart automatically
    if (sigaction(SIGUSR1, &act1, NULL) == -1) { perror("Client: sigaction(SIGUSR1) failed"); exit(10); }

    if (argc == 1) {
        printf("CLIENT %d: Usage with one the following commands/ways:\n\
        %s issueJob <job>\n\
        %s setConcurrency <N>\n\
        %s stop <jobID>\n\
        %s poll [running,queued]\n\
        %s exit\n", getpid(), argv[0], argv[0], argv[0], argv[0], argv[0]);
        exit(-1);
    }

    pid_t mypid = getpid();                                 //keep client's pid, it will be useful below in the program

    //Produce the name of the ServerToClFIFO, as ServerToClFIFOxxxx.., where xxxx.. is the pid of respective client
    int max_size_of_ascii_pid = sizeof(pid_t)*8;                //max size a pid_t can be in ascii representation
    char str_client_pid[max_size_of_ascii_pid];             
    int curr_size_of_ascii_pid = myitoa(mypid, str_client_pid); //transform client's pid into string
    if (curr_size_of_ascii_pid == -1) { perror("CLIENT: myitoa function failed"); exit(11); }
    char ServerToClFIFO[strlen(ServerToClFIFOtemplate)+1+curr_size_of_ascii_pid];
    strcpy(ServerToClFIFO, ServerToClFIFOtemplate);
    strcat(ServerToClFIFO, str_client_pid);                     //create the name as "ServerToClFIFOtemplate" + "client's pid"

    //Create the unique ServerToClFIFO
    if (mkfifo(ServerToClFIFO, 0666) == -1) { perror("CLIENT: mkfifo(ServerToClFIFO) failed"); exit(7); }

    pid_t serverpid;                                        //it will keep the server's pid
    int server_fd = open("jobExecutorServer.txt", O_RDONLY);
    if (server_fd == -1 && errno == ENOENT) {               //file jobExecutorServer.txt does not exist, jobExecutorServer must be created
        // printf("CLIENT %d: file jobExecutorServer.txt does not exist, jobExecutorServer must be created\n", getpid());
        serverpid = fork();
        if (serverpid == -1) {
            perror("CLIENT: fork failed");
            exit(4);
        } else if (serverpid == 0) {                        //child-Server
            //Execute the jobExecutorServer
            execl("jobExecutorServer", "jobExecutorServer", ClientsToSerFIFO, ServerToClFIFOtemplate, NULL);
            perror("execl of jobExecutorServer returned-failed");
            exit(5);
        } else {                                            //parent-Client
            //Pause the client the first time server starts, so that server can initialize its handlers, and wait until server sends a signal
            printf("CLIENT %d: I'm going to pause a little bit in order for the server to be initialized\n", getpid());
            if (signal_send == 0) {
                pause(); 
            }
            //Sleep the first client a little bit, so that server has time to be initialized
            sleep(1);
        }

    } else if (server_fd == -1) {
        perror("CLIENT: open(jobExecutorServer.txt) failed");
        exit(1);

    } else {                                                //file jobExecutorServer.txt exists, jobExecutorServer is active
        //Read the server's pid from the jobExecutorServer.txt file
        if (myread(server_fd, &serverpid, sizeof(pid_t)) == -1) { perror("CLIENT: myread(serverpid) function failed"); exit(3); }
        // printf("CLIENT %d: file jobExecutorServer.txt exists, and child-Server's pid is:%d\n", getpid(), serverpid);
    }

////////////////////////// SEND command TO SERVER //////////////////////////
    //Inform server that client is ready to open and write things in the pipe
    if (kill(serverpid, SIGUSR1) == -1) { perror("CLIENT: kill(serverpid, SIGUSR1) failed"); exit(6); }

    //Open the ClientsToSerFIFO
    int ClientToSerFD = open(ClientsToSerFIFO, O_WRONLY);
    if (ClientToSerFD == -1) { perror("CLIENT: open(ClientsToSerFIFO) failed"); exit(1); }

    //Compute the size of the command (with spaces as delimeters)
    int command_size = 0;
    for (int i = 1; i < argc; i++) {
        command_size += strlen(argv[i]);
        if (i != argc-1) {                                  //in order to count a space after each arg, except last arg
            command_size++;                                 //+1 for each space delimeter
        }
    }
    command_size++;                                         //+1 for the, last, '\0'

    //Produce the whole command as string
    char concatenated_args[command_size];
    concatenated_args[0] = '\0';                            //initialize it
    for (int i = 1; i < argc; i++) {
        strcat(concatenated_args, argv[i]);
        if (i != argc-1) {                                  //put a space after each arg, except last arg
            strcat(concatenated_args, " ");
        }
    } 

    //Compute the total size of the message to be sent
    int complete_message_size = sizeof(pid_t) + sizeof(int) + command_size;
    //And produce the whole message as one string (clientpid + commandsize + command)
    char message[complete_message_size];
    memcpy(message, &mypid, sizeof(pid_t));
    memcpy(message + sizeof(pid_t), &command_size, sizeof(int));
    memcpy(message + sizeof(pid_t) + sizeof(int), concatenated_args, command_size);

    //Write the whole message with an, atomic, write into the FIFO
    if (write(ClientToSerFD, message, complete_message_size) == -1) { perror("CLIENT: write message to ClientToSerFD failed"); exit(2); }

////////////////////////// RECEIVE response FROM SERVER //////////////////////////
    //Open the ServerToClFIFO
    int ServerToClFD = open(ServerToClFIFO, O_RDONLY);
    if (ServerToClFD == -1) { perror("CLIENT: open(ServerToClFIFO) failed"); exit(1); }

    //Now it is definitely safe to close ClientsToSerFIFO
    if (close(ClientToSerFD) == -1) { perror("CLIENT: close(ClientToSerFD) failed"); exit(8); }

    //Read the size of the response to be read the FIFO
    int size_to_receive;
    if (myread(ServerToClFD, &size_to_receive, sizeof(int)) == -1) { perror("CLIENT: myread(size_to_receive) function failed"); exit(3); }
    // printf("CLIENT %d: size_to_receive is:%d\n", getpid(), size_to_receive);

    //Read the actual response from the FIFO
    char response[size_to_receive];
    if (myread(ServerToClFD, response, size_to_receive) == -1) { perror("CLIENT: myread(response) function failed"); exit(3); }
    // printf("CLIENT %d: Response from Server is:\n%s\n", getpid(), response);
    printf("%s\n", response);

    //Inform server that the whole response has been received
    if (kill(serverpid, SIGUSR2) == -1) { perror("CLIENT: kill(serverpid, SIGUSR2) failed"); exit(6); }
    // printf("CLIENT %d: just send a SIGUSR2 to serverpid: %d\n", getpid(), serverpid);

    //Now it is safe to close ServerToClFD (as bytes were received from client)
    if (close(ServerToClFD) == -1) { perror("CLIENT: close(ServerToClFD) failed"); exit(8); }

    //Unlink, being the one and only hard-link instance of the FIFO, the FIFO gets deleted
    if (unlink(ServerToClFIFO) == -1) { perror("unlink"); exit(9); }



    // printf("CLIENT %d: I just finished my operation\n", getpid());

    exit(0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////