#define _GNU_SOURCE
/* jobExecutorServer.c */
#include <stdio.h>
#include <unistd.h>         //getpid(),write(),read(),close()
#include <sys/wait.h>	    //sockets
#include <sys/types.h>	    //sockets
#include <sys/socket.h>	    //sockets
#include <netdb.h>	        //sockets
#include <netinet/in.h>	    //internet sockets
#include <stdlib.h>	        //free(),exit(),atoi(),malloc(),
#include <signal.h>         //signals
#include <pthread.h>        //threads
#include <string.h>         //strlen()
#include <stdbool.h>        //bool type(true/false)

#include "../include/CommonInterface.h"
#include "../include/ServerInterface.h"


/* Exit Codes on Errors:
general: -1
open()/close()/dup2()/creat()/unlink(): 1
write(): 2
read(): 3
fork()/exec*()/waitpid(): 4
socket()/connect()/setsockopt()/bind()/listen()/accept()/shutdown(): 5
sigaction(): 6
pthread_mutex_init()/pthread_mutex_lock()/pthread_mutex_unlock(): 7
pthread_cond_init()/pthread_cond_wait()/pthread_cond_broadcast(): 8
pthread_create()/pthread_detach(): 9

gethostbyname(): 11
write()/SIGPIPE/BROKENPIPE: 12
strchr(): 13
*/

//Define the shared memory objects
/******************************** Shared-Global Memory *********************************/
int concurrencyLevel;           //multiprogramming level
int jobID_count;                //counter of total jobs that have been received
int running_jobs;               //counter of current running jobs
bool exit_commanded;            //flag of exit command received
int controllers_running;        //counter of active controllers
bool entered;                   //just a small flag to let only one controller exit the program

JobBuffer_t *jobBuffer;         //buffer for storing waiting jobs

pthread_mutex_t mutex_shm;      //mutex for all the shared memory
pthread_cond_t cond_full;       //condition for full buffer
pthread_cond_t cond_empty;      //condition for empty buffer (and concurrency)
pthread_cond_t cond_exit;       //just a small condition for the controllers_running, used in the exit_program() function

int threadPoolSize;             //number of worker threads created
pthread_t* wrk_id;              //worker-threads-ids array

/***************************************************************************************/


//Signal handler for SIGPIPE (for the server to be safeguarded)
void SIGPIPE_handler(/*int signo*/) {
    //Ignore the default action of the signal SIGPIPE
    char *message = "SERVER: SIGPIPE error handled, not quiting\n";
    if (write(1, message, strlen(message)) == -1) { error_exit("SERVER: write() to stdout failed", 2); } //write to stdout
    fflush(stdout);
}


///////////////////////////////////////////////////////////////* THREADS start routines */////////////////////////////////////////////////////////////////
//WORKER thread start routine
void* worker_thread(void* my_arg) {
    (void)my_arg;                                           //to ignore unused parameter warning
    //Call the actual Worker
    worker();
    //Terminate the thread properly (it will be joined by the last controller who will exit the program)
    pthread_exit(NULL);
}

//CONTROLLER thread start routine
void* controller_thread(void* my_arg) {
    //LOCK mutex_shm
    if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

        //Keep track of how many controller run (do a "useful" thing)
        controllers_running++;

    //UNLOCK mutex_shm
    if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

    struct Arg *ctrl_arg = (struct Arg*) my_arg;            //define an arguments struct and set it equal to the one given
    //Call the actual Controller
    controller(ctrl_arg->mysock);
    int initial_socket = ctrl_arg->initialsock;             //keep locally the initial socket of the connection (the one which accepts) 
    free(my_arg);                                           //free the arguments struct given

    //LOCK mutex_shm
    if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

        //Decrease the number of controllers running
        controllers_running--;

        //Check whether the controller was commanded an "exit"
        if (exit_commanded == true) {
            if (entered == false) {
                //If so, call the following function to exit the program altogether
                entered = true; //so that only one controller will exit the program
                exit_program(initial_socket);
            } else {    //if a controller has already entered the exit function
                //Broadcast (or signal here) the one possible controller waiting for exiting the program
                if (pthread_cond_broadcast(&cond_exit) == -1) { error_exit("CONTROLLER-thread: pthread_cond_broadcast(cond_exit) failed", 7); }
            }
        }

    //UNLOCK mutex_shm
    if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

    //Terminate the thread properly (it is detached by main, will not be joined)
    pthread_exit(NULL);
}

//MAIN thread start routine
int main(int argc, char *argv[]) {

    if (argc < 4) {
        printf("SERVER %d: Usage: %s [portNum] [bufferSize] [threadPoolSize]\n", getpid(), argv[0]);
        exit(-1);
    }

    /* Any signal handlers need to be set here (before threads creations)*/
    //Safeguard server to continue working (waiting for new connections), even if a client closes in wrong time the socket that server tries to write on
    //(in my Commander implementation, this is not going to happen, so the server will not get a SIGPIPE. But, as a server, it is a good practice to be safeguarded)
    static struct sigaction act;
    act.sa_handler = SIGPIPE_handler;                       //set the handler for the SIGPIPE signal
    sigfillset(&(act.sa_mask));                             //fillset with all signals, so that handler is safe (not interrupted) while operating
    act.sa_flags = SA_RESTART;                              //set SA_RESTART flag, so that interrupted syscalls to restart automatically
    if (sigaction(SIGPIPE, &act, NULL) == -1) { error_exit("SERVER: sigaction(SIGPIPE) failed", 6); }


    /* Initialize shared memory*/
    if (pthread_mutex_init(&mutex_shm, NULL) == -1) { error_exit("SERVER: pthread_mutex_init(mutex_shm) failed", 7); }
    if (pthread_cond_init(&cond_full, NULL) == -1) { error_exit("SERVER: pthread_cond_init(cond_full) failed", 8); }
    if (pthread_cond_init(&cond_empty, NULL) == -1) { error_exit("SERVER: pthread_cond_init(cond_empty) failed", 8); }
    if (pthread_cond_init(&cond_exit, NULL) == -1) { error_exit("SERVER: pthread_cond_init(cond_exit) failed", 8); }

    int bufferSize = atoi(argv[2]);    //number of jobs to create
    if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("SERVER: pthread_mutex_lock(mutex_shm) failed", 7); }        //LOCK mutex_shm
        jobBuffer = malloc(sizeof(JobBuffer_t));            //create shared buffer
        initialize_jobBuffer(jobBuffer, bufferSize);        //and initialize it 
        concurrencyLevel = 1;                               //initialize multiprogramming level to 1
        jobID_count = 0;                                    //initialize counter of the jobs jobExecutorServer has received to execute (totally)
        running_jobs = 0;                                   //initialize running_jobs
        exit_commanded = false;                             //exit has not been commanded yet
        controllers_running = 0;                            //initialize count of active controllers
        entered = false;                                    //no controller has entered to exit_program function yet
    if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("SERVER: pthread_mutex_unlock(mutex_shm) failed", 7); }    //UNLOCK mutex_shm


    /* Create the threadPool worker threads */
    typedef void* (*funptr) (void*);    //typedef the pointer to function which takes a void* and returns a void*
    if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("SERVER: pthread_mutex_lock(mutex_shm) failed", 7); }        //LOCK mutex_shm
        threadPoolSize = atoi(argv[3]);
        wrk_id = malloc(threadPoolSize * sizeof(pthread_t));
        for (int i = 0; i < threadPoolSize; i++) {
            funptr wrk_start_routine = &worker_thread;
            if (pthread_create(&(wrk_id[i]), 0, wrk_start_routine, NULL) == -1) { error_exit("SERVER: pthread_create(worker_thread) failed", 9); }

            //Workers will be joined by a controller thread, so no detach needed here
        }
    if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("SERVER: pthread_mutex_unlock(mutex_shm) failed", 7); }    //UNLOCK mutex_shm

    /* Create the connection */
    short unsigned int port;
    int sock, newsock;
    struct sockaddr_in server, client;
    socklen_t clientlen = sizeof(client);
    struct sockaddr *serverptr = (struct sockaddr*) &server;
    struct sockaddr *clientptr = (struct sockaddr*) &client;

    port = (short unsigned int) atoi(argv[1]);

    //Create socket (TCP over Internet)
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) { error_exit("MAIN-thread: socket() failed", 5); }

    //When a server quits, the listening port remains busy (state TIME WAIT) for a while
    //Restarting the server fails in bind with “Bind: Address Already in Use”
    //To override this, I use setsockopt() to enable SO_REUSEADDR
    int opt = 1;    //1 in order to "enable" the SO_REUSEADDR option for socket sock in socket-level (SOL_SOCKET)
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) { error_exit("MAIN-thread: setsockopt(SO_REUSEADDR) failed", 5); }

    server.sin_family = AF_INET;                            //internet domain
    server.sin_addr.s_addr = htonl(INADDR_ANY);             //accept from any address
    server.sin_port = htons(port);                          //the given port in which will listen
    //Bind socket to address/port of server
    if (bind(sock, serverptr, sizeof(server)) == -1) { error_exit("MAIN-thread: bind(sock, serverptr) failed", 5); }
    //Listen for connections (up to a waiting-for-accept backlog of 10)
    if (listen(sock, 10) == -1) { error_exit("MAIN-thread: listen(sock) failed", 5); }
    printf("MAIN-thread %ld: Listening for connections to port %d, on socket %d.\n", pthread_self(), port, sock);

    while (1) {
        //Accept connections on initial socket-port
    	if ((newsock = accept(sock, clientptr, &clientlen)) < 0) { error_exit("MAIN-thread: accept(sock) failed", 5); }
        printf("MAIN-thread %ld: Accepted connection on new socket %d.\n", pthread_self(), newsock);

        /* Create controller thread */
        pthread_t ctrl_id;
        funptr ctrl_start_routine = &controller_thread;
        struct Arg *ctrl_arg = malloc(sizeof(struct Arg));
        ctrl_arg->mysock = newsock;
        ctrl_arg->initialsock = sock;
        if (pthread_create(&ctrl_id, 0, ctrl_start_routine, ctrl_arg) == -1) { error_exit("MAIN-thread: pthread_create(controller_thread) failed", 9); }

        //Detach it, as it will not be joined
        if (pthread_detach(ctrl_id) == -1) { error_exit("MAIN-thread: pthread_detach(controller_thread) failed", 9); }
    }
    
    //This following-last segment will never be executed, as a controller will be responsible for exiting the program altogether
    return -1;  //return an exit code, just in case the main-thread exits the whole program unexpectedly
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////* EXIT-program procedure */////////////////////////////////////////////////////////////////
void exit_program(int initial_socket) {

    //LOCKED mutex_shm already before call
    
        //Wait for all the remaining running controller threads to finish
        while(controllers_running > 0) {
	    	if (pthread_cond_wait(&cond_exit, &mutex_shm) == -1) { error_exit("WORKER-thread: pthread_cond_wait(cond_empty) failed", 7); }
        }

        if (jobBuffer->count > 0) {                 //if jobBuffer is not empty
            //Traverse the whole jobBuffer
            for (int i = 0; i < jobBuffer->bufferSize; i++) {
                //For every existing element-job in the buffer
                if (jobBuffer->buffer[i] != NULL) {
                    //Send a new 2nd response to the corresponding commander that commanded this specific job
                    char *another_issueJob_response = "SERVER TERMINATED BEFORE EXECUTION\n";
                    size_t size_to_send_another = strlen(another_issueJob_response)+1;
                    if (write(jobBuffer->buffer[i]->clientSocket, another_issueJob_response, size_to_send_another) == -1) { error_exit("CONTROLLER-thread: write(another_issueJob_response) failed", 12); }

                    //Shutdown the write side of the socket, in order to close correctly
                    if (shutdown(jobBuffer->buffer[i]->clientSocket, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(jobBuffer->buffer[i]->clientSocket) failed", 5); }
                    //Close the socket with the specific commander
                    if (close(jobBuffer->buffer[i]->clientSocket) == -1) { error_exit("CONTROLLER-thread: close(jobBuffer->buffer[i]->clientSocket) failed", 1); }
                }
            }
        }

    //UNLOCK mutex_shm (so that workers can take it and exit themselves)
    if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

    //Join all the exited worker threads (and wait the ones running jobs to finish)
    for (int i = 0; i < threadPoolSize; i++) {
        if (pthread_join(wrk_id[i], NULL) == -1) { error_exit("CONTROLLER-thread: pthread_join(wrk_id[i]) failed", 9); }
    }

    //LOCK mutex_shm (again so that free of shared data is done correctly)
    if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

        //Free the worker-thread_ids array
        free(wrk_id);

        //Free the whole jobBuffer
        free_jobBuffer(jobBuffer, jobBuffer->bufferSize);
        free(jobBuffer);

        //Destroy condition variables
	    if (pthread_cond_destroy(&cond_empty) == -1) { error_exit("CONTROLLER-thread: pthread_cond_destroy(cond_empty) failed", 9); }
	    if (pthread_cond_destroy(&cond_full) == -1) { error_exit("CONTROLLER-thread: pthread_cond_destroy(cond_full) failed", 9); }
	    if (pthread_cond_destroy(&cond_exit) == -1) { error_exit("CONTROLLER-thread: pthread_cond_destroy(cond_exit) failed", 9); }

    //Destroy mutex (auto UNLOCK)
	if (pthread_mutex_destroy(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_destroy(mutex_shm) failed", 9); }

    printf("CONTROLLER-thread %ld: Closing listening socket %d, and terminating the server\n", pthread_self(), initial_socket);
    //Close the initial socket (the socket on accept)
    if (close(initial_socket) == -1) { error_exit("CONTROLLER-thread: close(initial_socket) failed", 1); }

    //Exit the program altogether
    exit(0);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////* jobBuffer Init&Free procedures */////////////////////////////////////////////////////////////
//Initialize the job buffer
void initialize_jobBuffer(JobBuffer_t *jobBuffer, int bufferSize) {
     //Set buffer's size
    jobBuffer->bufferSize = bufferSize;
    jobBuffer->buffer = (Job_t **)malloc(bufferSize * sizeof(Job_t*));
    //Initialize each pointer in jobBuffer->buffer to NULL
    for (int i = 0; i < bufferSize; i++) {
        jobBuffer->buffer[i] = NULL;
    }
    //Initialize buffer's start, end, count
    jobBuffer->start = 0;
    jobBuffer->end = -1;
    jobBuffer->count = 0;
    return;
}
//Free the job buffer
void free_jobBuffer(JobBuffer_t *jobBuffer, int bufferSize) {
    //Free each pointer in jobBuffer->buffer != NULL
    for (int i = 0; i < bufferSize; i++) {
        if (jobBuffer->buffer[i] != NULL) {
            free(jobBuffer->buffer[i]);
        }
    }
    //Free the buffer itself
    free(jobBuffer->buffer);
    return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////