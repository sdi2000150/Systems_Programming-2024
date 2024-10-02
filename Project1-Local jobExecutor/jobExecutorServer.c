#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

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

///////////////////////////////////////////////////////////// Signal Handlers definition /////////////////////////////////////////////////////////////

//flag(true-false) for knowing if server is operating (running) or not (suspended)
volatile sig_atomic_t server_operating = 0;                 //volatile: for not optimizing actions over it, sig_atomic_t: to be read/written atomicly
//counter-flag(0,1,2,...) for knowing how many SIGUSR1 signals have been received at a certain point
volatile sig_atomic_t new_request = 0;                      //volatile: for not optimizing actions over it, sig_atomic_t: to be read/written atomicly
void SIGUSR1_handler(/*int signo*/) {
    //Ignore the default action of the signal SIGUSR1

    // char *message = "SERVER: inside handler of SIGUSR1\n";
    // if (write(1, message, strlen(message)) == -1) { perror("SERVER: write() to stdout failed"); exit(2); } //write to stdout

    if (server_operating == 1) {                            //if server is operating with another command
        new_request++;                                      //then increase the counter, meaning that a new request (client writing to pipe) has been made
    }
}

//counter-flag(0,1,2,...) for knowing how many SIGUSR2 signals have been received at a certain point
volatile sig_atomic_t bytes_received = 0;                   //volatile: for not optimizing actions over it, sig_atomic_t: to be read/written atomicly
void SIGUSR2_handler(/*int signo*/) {
    //Ignore the default action of the signal SIGUSR2

    // char *message = "SERVER: inside handler of SIGUSR2\n";
    // if (write(1, message, strlen(message)) == -1) { perror("SERVER: write() to stdout failed"); exit(2); } //write to stdout

    bytes_received++;                                       //increase the counter, meaning that a client has read the response sent by server
}

//counter-flag(0,1,2,...) for knowing how many SIGCHLD signals have been received at a certain point
volatile sig_atomic_t child_exited = 0;                     //volatile: for not optimizing actions over it, sig_atomic_t: to be read/written atomicly
void SIGCHLD_handler(/*int signo*/) {
    //Default action of the signal SIGCHLD is Ignore

    // char *message = "SERVER: inside handler of SIGCHLD\n";
    // if (write(1, message, strlen(message)) == -1) { perror("SERVER: write() to stdout failed"); exit(2); } //write to stdout

    child_exited++;                                         //increase the counter, meaning that a child was terminated
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////// Main Program ////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {

    if (argc != 3) {
        printf("SERVER %d: Server should be executed as: %s ClientsToSerFIFO ServerToClFIFO\n", getpid(), argv[0]);
        exit(-1);
    }

    //Create the jobExecutorServer.txt file, as server starts
    int server_fd = creat("jobExecutorServer.txt", 0666);
    if (server_fd == -1) { perror("SERVER: creat(\"jobExecutorServer.txt\") failed"); exit(1); } 
    else { printf("SERVER %d:\"jobExecutorServer.txt\" created successfully\n", getpid()); }

    //And write inside the file the server's pid
    pid_t mypid = getpid();
    if (write(server_fd, &mypid, sizeof(pid_t)) == -1) { perror("SERVER: write() pid to jobExecutorServer.txt failed"); exit(2); }


    //Set the handler for the SIGUSR1 signal
    static struct sigaction act;
    act.sa_handler = SIGUSR1_handler;
    sigfillset(&(act.sa_mask));                             //fillset with all signals, so that handler is safe (not interrupted) while operating
    act.sa_flags = SA_RESTART;                              //set SA_RESTART flag, so that interrupted syscalls to restart automatically
    if (sigaction(SIGUSR1, &act, NULL) == -1) { perror("SERVER: sigaction(SIGUSR1) failed"); exit(10); }

    //Set the handler for the SIGUSR2 signal
    static struct sigaction act2;
    act2.sa_handler = SIGUSR2_handler;
    sigfillset(&(act2.sa_mask));                            //fillset with all signals, so that handler is safe (not interrupted) while operating
    act2.sa_flags = SA_RESTART;                             //set SA_RESTART flag, so that interrupted syscalls to restart automatically
    if (sigaction(SIGUSR2, &act2, NULL) == -1) { perror("SERVER: sigaction(SIGUSR2) failed"); exit(10); }

    //Set the handler for the SIGCHLD signal
    static struct sigaction act3;
    act3.sa_handler = SIGCHLD_handler;
    sigfillset(&(act3.sa_mask));                            //fillset with all signals, so that handler is safe (not interrupted) while operating
    act3.sa_flags = SA_RESTART;                             //set SA_RESTART flag, so that interrupted syscalls to restart automatically
    if (sigaction(SIGCHLD, &act3, NULL) == -1) { perror("SERVER: sigaction(SIGCHLD) failed"); exit(10); }

    //Set the mask for the sigwait(SIGUSR1,SIGCHLD)
    sigset_t maskUSR1_CHLD;
    sigemptyset(&maskUSR1_CHLD);                            //emptyset
    sigaddset(&maskUSR1_CHLD, SIGUSR1);                     //add SIGUSR1
    sigaddset(&maskUSR1_CHLD, SIGCHLD);                     //add SIGCHLD

    //To set the mask for the sigsuspend(SIGUSR2)
    sigset_t maskUSR2;
    //Keep the default signal mask, in order to restore it when needed, after sigsuspend(SIGUSR2)
    sigset_t orig_mask;
    if (sigprocmask(SIG_SETMASK, NULL, &orig_mask) == -1) { perror("SERVER: sigprocmask(save default mask) failed"); exit(10); }


    //Create the ClientsToSerFIFO
    char ClientsToSerFIFO[strlen(argv[1])+1];
    strcpy(ClientsToSerFIFO, argv[1]);
    if (mkfifo(ClientsToSerFIFO, 0666) == -1) { perror("SERVER: mkfifo(ClientsToSerFIFO) failed"); exit(7); }

    //Initialize Waiting Queue
    Queue* waiting_queue = malloc(sizeof(Queue));
    initializeQueue(waiting_queue);

    //Initialize Running Queue
    Queue* running_queue = malloc(sizeof(Queue));
    initializeQueue(running_queue);


    int concurrency_degree = 1;                             //degree of multiprogramming initialized to 1
    int current_mpl = 0;                                    //current running jobs are 0 (current multiprogramming level = 0)
    int jobID_count = 0;                                    //counter of the jobs jobExecutorServer has received to execute (totally)
    int received_chld = 0;                                  //flag for checking if server was unpaused due to a SIGCHLD
    int sig;                                                //for use in sigwait
    int status;                                             //for use in wait
    pid_t zombie_pid;                                       //for use in wait

    //Resume parent-Client (this will happen only for the first Client-Commander, the one that forked the Server)
    if (kill(getppid(), SIGUSR1) == -1) { perror("SERVER: kill(firstclientpid,SIGUSR1) failed"); exit(6); }


    /////////////////////////* Start of Repeat - jobExecutorServer will terminate only when "exit" is commanded */////////////////////////

    while(1) {                                              //repeat until a break occurs

        //While having children-jobs that have exited, repeat (remove them from running queue, and check-to-run other jobs)
        while (child_exited > 0) {
            child_exited--;                                 //decrease the counter of terminated children-jobs

            //Capture its pid and status
            zombie_pid = wait(&status);
            // printf("SERVER %d: I just captured with wait() child-job with pid %d which exited with status: %d\n", getpid(), zombie_pid, WEXITSTATUS(status));

            //Remove it from running queue
            Job* job_entry = removePIDSpecificJob(running_queue, zombie_pid);
            if (job_entry != NULL) {                        //if job_entry existed, and removed
                current_mpl--;                              //current multiprogramming level decreases by 1
                free(job_entry);            

                //Check-to-run other jobs
                // printf("SERVER %d: i should check_to_run here\n", getpid());
                check_to_run(&current_mpl, concurrency_degree, waiting_queue, running_queue);
                // printQueue(running_queue);
            }                                               //otherwise it was already removed by stop command
        }

        //Suspend operation until signals occur
        if (new_request == 0) {                             //if no signal was received while the server operated with another client

            //Wait-suspend, until a SIGUSR or SIGCHLD gets received
            sigwait(&maskUSR1_CHLD, &sig);

            if (sig == SIGUSR1) {
                // printf("SERVER %d: Caught SIGUSR1 with sigwait\n", getpid());
                //okei continue with server operations
            } else if (sig == SIGCHLD) {
                child_exited--;                             //decrease the counter of terminated children-jobs
                // printf("SERVER %d: Caught SIGCHLD with sigwait\n", getpid());

                //Capture its pid and status
                zombie_pid = wait(&status);
                // printf("SERVER %d: I just captured with wait() child-job with pid %d which exited with status: %d\n", getpid(), zombie_pid, WEXITSTATUS(status));

                //Remove it from running queue
                Job* job_entry = removePIDSpecificJob(running_queue, zombie_pid);
                if (job_entry != NULL) {                    //if job_entry existed, and removed
                    current_mpl--;                          //current multiprogramming level decreases by 1
                    free(job_entry);

                    //Check-to-run other jobs
                    // printf("SERVER %d: i should check_to_run here\n", getpid());
                    check_to_run(&current_mpl, concurrency_degree, waiting_queue, running_queue);
                }                                           //otherwise it was already removed by stop command
                
                received_chld = 1;                          //go to while(1) (do not go to server operations)
            }
        //Or do not suspend if signal(s) have already been received
        } else {                                            //else if any SIGUSR1 signal(s) was received while the server operated with another client 
            new_request--;                                  //then do not wait, data from new client are ready to be received
        }
    
        //if server was awaken due to SIGCHLD, then do not do any operation, and return to while(1)
        if (received_chld == 1) {
            received_chld = 0;                              //return back to while(1)
        //Else, server was awaken due to a SIGUSR1, so he have to operate
        } else {
            server_operating = 1;                           //server starts operating

            //Open ClientsToSerFIFO
            int ClientToSerFD = open(ClientsToSerFIFO, O_RDONLY);
            if (ClientToSerFD == -1) { perror("SERVER: open(ClientsToSerFIFO) failed"); exit(1); }

            //Read from ClientsToSerFIFO the first sizeof(pid_t) bytes, so that the server knows the pid of the respective client
            pid_t client_pid;
            if (myread(ClientToSerFD, &client_pid, sizeof(pid_t)) == -1) { perror("SERVER: myread(client_pid) function failed"); exit(3); }
            // printf("SERVER %d: client's pid is:%d\n", getpid(), client_pid);

            //Read from ClientsToSerFIFO the next sizeof(int) bytes, which indicate the number of bytes that the following client's message will be
            int size_to_receive;
            if (myread(ClientToSerFD, &size_to_receive, sizeof(int)) == -1) { perror("SERVER: myread(size_to_receive) function failed"); exit(3); }
            // printf("SERVER %d: size_to_receive is:%d\n", getpid(), size_to_receive);

            //Read from ClientsToSerFIFO the final bytes, that are the command-string
            char command[size_to_receive];
            if (myread(ClientToSerFD, command, size_to_receive) == -1) { perror("SERVER: myread(command) function failed"); exit(3); }
            // printf("SERVER %d: command is:%s\n", getpid(), command);

            //Produce the name of the ServerToClFIFO, as ServerToClFIFOxxxx.., where xxxx.. is the pid of respective client
            int max_size_of_ascii_pid = sizeof(pid_t)*8;
            char str_client_pid[max_size_of_ascii_pid];
            int curr_size_of_ascii_pid = myitoa(client_pid, str_client_pid);
            if (curr_size_of_ascii_pid == -1) { perror("SERVER: myitoa function failed"); exit(11); }
            char ServerToClFIFO[strlen(argv[2]) + curr_size_of_ascii_pid + 1]; //+1 for '\0'
            strcpy(ServerToClFIFO, argv[2]);
            strcat(ServerToClFIFO, str_client_pid);

            //Open the ServerToClFIFO (it has already been created by client at this point)
            int ServerToClFD = open(ServerToClFIFO, O_WRONLY);
            if (ServerToClFD == -1) { perror("SERVER: open(ServerToClFIFO) failed"); exit(1); }

            //Now it is definitely safe to close ClientsToSerFIFO
            if (close(ClientToSerFD) == -1) { perror("SERVER: close(ClientToSerFD) failed"); exit(8); }

            //Parse command
            char command_copy[size_to_receive];
            strcpy(command_copy, command);                  //strcpy the command, because strtok destroys the initial string
            char* arg_token;
            arg_token = strtok(command_copy, " ");

            ////////////////* Beggining of the Server's substantial operations (command execution and response to client) *////////////////

            //if command = "issueJob":
            if (strcmp(arg_token, "issueJob") == 0) {

                //////////* Waiting Queue Insertion *//////////

                //Parse the rest command, after "issueJob"
                char* rest_command;
                rest_command = strchr(command, ' ');
                rest_command++;                             //for skipping space (created at the front of rest command with strchr(command, ' '))

                //Allocate memory for the Job
                Job *new_job = malloc(sizeof(Job));

                //Create the jobID_XX field
                jobID_count++;
                strcpy(new_job->jobID, "job_");
                int max_size_of_ascii_int = sizeof(int)*8;                      //max size a integer can be in ascii representation
                char str_jobID[max_size_of_ascii_int];
                int curr_size_of_ascii_int = myitoa(jobID_count, str_jobID);    //transform jobID_count into string
                if (curr_size_of_ascii_int == -1) { perror("SERVER: myitoa function failed"); exit(11); }
                strcat(new_job->jobID, str_jobID);                              //create the jobID as "jobID_XX", where XX is the server's jobID counter
                char jobID_response[max_size_of_ascii_int+4];                   //keep it localy, to send it in the response (+4 for the "job_")
                strcpy(jobID_response, new_job->jobID);

                //Create the job field
                strcpy(new_job->job, rest_command);                             //rest_command will be sent in the response


                //Field jobPID is 0 for the waiting_queue elements
                new_job->jobPID = 0;

                //Insert in the queue (the queuePosition field will be created during the insertion)
                int queue_position = insertJob(waiting_queue, new_job);
                // printf("Waiting "); 
                // printQueue(waiting_queue);

                //////////* Check if possible to execute a job *//////////

                //Check-to-run
                // printf("SERVER %d: i should check_to_run here\n", getpid());
                check_to_run(&current_mpl, concurrency_degree, waiting_queue, running_queue);

                //////////* Send Responce to Client *//////////

                //Create the response of the form "<jobID,job,queuePosition>"
                char issueJob_response[strlen(jobID_response) + strlen(rest_command) + max_size_of_ascii_int + 4 + 1];   //+4 for the "<" "," "," ">", +1 for the '\0'
                strcpy(issueJob_response, "<");
                strcat(issueJob_response, jobID_response);
                strcat(issueJob_response, ",");
                strcat(issueJob_response, rest_command);
                strcat(issueJob_response, ",");
                char str_queue_position[max_size_of_ascii_int];
                curr_size_of_ascii_int = myitoa(queue_position, str_queue_position);                                    //transform queue_position into string
                strcat(issueJob_response, str_queue_position);
                strcat(issueJob_response, ">");
                int size_to_send = strlen(issueJob_response)+1;
                if (write(ServerToClFD, &size_to_send, sizeof(int)) == -1) { perror("SERVER: write(size_to_send) on issueJob command failed"); exit(2); }
                if (write(ServerToClFD, issueJob_response, size_to_send) == -1) { perror("SERVER: write(issueJob_response) failed"); exit(2); }

            //if command = "setConcurrency":
            } else if (strcmp(arg_token, "setConcurrency") == 0) {

                arg_token = strtok(NULL, " ");              //parse the <N> (integer (as string) after "setConcurrency" command)
                concurrency_degree = atoi(arg_token);       //transform it into integer and set the new Concurrency degree
                // printf("SERVER %d: Concurrency degree is going to change to %s (as string) %d (as int)\n", getpid(), arg_token, concurrency_degree);

                char* setConcurrency_response = "Concurrency degree changed";
                int size_to_send = strlen(setConcurrency_response)+1;
                if (write(ServerToClFD, &size_to_send, sizeof(int)) == -1) { perror("SERVER: write(size_to_send) on setConcurrency command failed"); exit(2); }
                if (write(ServerToClFD, setConcurrency_response, size_to_send) == -1) { perror("SERVER: write(setConcurrency_response) failed"); exit(2); }

            //if command = "stop":
            } else if (strcmp(arg_token, "stop") == 0) {

                arg_token = strtok(NULL, " ");              //parse the <jobID> ("job_XX" after "stop" command)

                char stop_response[256];

                //Check running queue
                Job* job_entry = removejobIDSpecificJob(running_queue, arg_token);
                if (job_entry != NULL) {
                    current_mpl--;                          //current multiprogramming level decreases by 1
                    kill(SIGKILL, job_entry->jobPID);
                    strcpy(stop_response, job_entry->jobID);
                    strcat(stop_response, " terminated");
                    free(job_entry);
                } else { //Check waiting queue
                    job_entry = removejobIDSpecificJob(waiting_queue, arg_token);
                    if (job_entry != NULL) {
                        strcpy(stop_response, job_entry->jobID);
                        strcat(stop_response, " removed");
                        free(job_entry);
                    } else {
                        strcpy(stop_response, "job_ID requested was not found");
                    }
                }

                int size_to_send = strlen(stop_response)+1;
                if (write(ServerToClFD, &size_to_send, sizeof(int)) == -1) { perror("SERVER: write(size_to_send) on stop command failed"); exit(2); }
                if (write(ServerToClFD, stop_response, size_to_send) == -1) { perror("SERVER: write(stop_response) failed"); exit(2); }

            //if command = "poll":
            } else if (strcmp(arg_token, "poll") == 0) {

                arg_token = strtok(NULL, " ");                  //parse the "running" or "queued" (after "poll" command)

                //Create the response of the form "<jobID,job,queuePosition>"
                int max_size_of_ascii_int = sizeof(int)*8;      //max size a integer can be in ascii representation
                char poll_response[jobID_count * (max_size_of_ascii_int+4 + DFLBUFSIZE + max_size_of_ascii_int+4 + 1 + 1)]; //to be safe declare its size big enough

                if (strcmp(arg_token, "running") == 0) {        //"running " was commanded to be polled
                    pollQueue(running_queue, poll_response);

                } else if (strcmp(arg_token, "queued") == 0) {  //"queued" was commanded to be polled
                    pollQueue(waiting_queue, poll_response);

                } else {                                        //wrong input on poll command
                    strcpy(poll_response, "Wrong input on poll command");
                }

                int size_to_send = strlen(poll_response)+1;
                if (write(ServerToClFD, &size_to_send, sizeof(int)) == -1) { perror("SERVER: write(size_to_send) on poll command failed"); exit(2); }
                if (write(ServerToClFD, poll_response, size_to_send) == -1) { perror("SERVER: write(poll_response) failed"); exit(2); }

            //if command = "exit":
            } else if (strcmp(arg_token, "exit") == 0) {

                char* exit_response = "jobExecutorServer terminated";
                int size_to_send = strlen(exit_response)+1;
                if (write(ServerToClFD, &size_to_send, sizeof(int)) == -1) { perror("SERVER: write(size_to_send) on exit command failed"); exit(2); }
                if (write(ServerToClFD, exit_response, size_to_send) == -1) { perror("SERVER: write(exit_response) failed"); exit(2); }

                //Check if client has received the whole message, or wait
                if (bytes_received == 0) {
                    // printf("SERVER %d: I'm going to block on sigsuspend(SIGUSR2)\n", getpid());
                    sigfillset(&maskUSR2);                      //fill with all signals
                    sigdelset(&maskUSR2, SIGUSR2);              //remove SIGUSR1 from the set
                    sigsuspend(&maskUSR2);                      //wait for a SIGUSR2 by client
                    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) == -1) { perror("SERVER: sigprocmask(restore default mask) failed"); exit(10); } //restore the mask
                }
                bytes_received--;                               //decrease the counter-flag, showing that SIGUSR2 was handled

                //Now it is safe to close ServerToClFD (as bytes were received from client)
                if (close(ServerToClFD) == -1) { perror("SERVER: close(ServerToClFD) failed"); exit(8); }

                break;

            //if command is unknown:
            } else {

                char* other_response = "Command passed to Server is unknown";
                int size_to_send = strlen(other_response)+1;
                if (write(ServerToClFD, &size_to_send, sizeof(int)) == -1) { perror("SERVER: write(size_to_send) on unknown command failed"); exit(2); }
                if (write(ServerToClFD, other_response, size_to_send) == -1) { perror("SERVER: write(other_response) failed"); exit(2); }
            }

            //Check if client has received the whole message, or wait
            if (bytes_received == 0) {
                // printf("SERVER %d: I'm going to block on sigsuspend(SIGUSR2)\n", getpid());
                sigfillset(&maskUSR2);                      //fill with all signals
                sigdelset(&maskUSR2, SIGUSR2);              //remove SIGUSR1 from the set
                sigsuspend(&maskUSR2);                      //wait for a SIGUSR2 by client
                if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) == -1) { perror("SERVER: sigprocmask(restore default mask) failed"); exit(10); } //restore the mask
            }
            bytes_received--;                               //decrease the counter-flag, showing that SIGUSR2 was handled

            //Now it is safe to close ServerToClFD (as bytes were received from client)
            if (close(ServerToClFD) == -1) { perror("SERVER: close(ServerToClFD) failed"); exit(8); }

            ////////////////* End of the Server's substantial operations (command execution and response to client) *////////////////

            server_operating = 0;                           //server finished operating on the specific command
        }
    }                                                       //end of current loop, go to while(1) again

    ////////////////////////* End of Repeat - jobExecutorServer received the command "exit", and he will now terminate *////////////////////////


    //Wait for any remaining running children-jobs
    while ((zombie_pid = waitpid(-1, &status, 0)) > 0);

    //Free the mallocs for the waiting_queue
    freeQueue(waiting_queue);
    free(waiting_queue);

    //Free the mallocs for the running_queue
    freeQueue(running_queue);
    free(running_queue);

    //Unlink, being the one and only hard-link instance of the FIFO, the FIFO gets deleted
    if (unlink(ClientsToSerFIFO) == -1) { perror("SERVER: unlink(ClientsToSerFIFO) failed"); exit(9); }

    //Delete the jobExecutorServer.txt file before server exits, 
    //with unlink, being the one and only hard-link instance of the file, the file gets deleted
    if (unlink("jobExecutorServer.txt") == 0) { printf("SERVER %d: \"jobExecutorServer.txt\" deleted successfully\n", getpid()); } 
    else { perror("SERVER: unlink(jobExecutorServer.txt) failed"); exit(9); }


    printf("SERVER %d: I finished my operations, as I received an \"exit\" command\n", getpid());

    exit(0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////