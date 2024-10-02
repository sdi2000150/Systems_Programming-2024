#define _GNU_SOURCE
/* Controller.c */
#include <stdio.h>
#include <unistd.h>         //write(),read(),close()
#include <sys/wait.h>	    //sockets
#include <sys/types.h>	    //sockets
#include <sys/socket.h>	    //sockets
#include <netdb.h>	        //sockets
#include <netinet/in.h>	    //internet sockets
#include <pthread.h>        //threads
#include <stdlib.h>	        //malloc(),free(),atoi()
#include <string.h>         //strcpy(),strtok(),strcmp(),strchr(),strcat(),memcpy(),strlen()
#include <stdbool.h>        //bool type(true/false)

#include "../include/CommonInterface.h"
#include "../include/ServerInterface.h"

/////////////////////////////////////////////////////////////* CONTROLLER-thread procedure *//////////////////////////////////////////////////////////////
void controller(int newsock) {

    //Read the first sizeof(uint32_t) bytes, which indicate the number of bytes that the following client's message will be
    uint32_t size_to_receive_net;
    if (myread(newsock, &size_to_receive_net, sizeof(uint32_t)) == -1) { error_exit("CONTROLLER-thread: myread(size_to_receive) function failed", 3); }
    // printf("CONTROLLER-thread %ld: size_to_receive is:%d\n", pthread_self(), (int)ntohl(size_to_receive_net));

    //Read the final bytes, that are the actual command-string
    int size_to_receive = (int) ntohl(size_to_receive_net);  //convert the size from network byte order to host byte order
    char command[size_to_receive];
    if (myread(newsock, command, size_to_receive) == -1) { error_exit("CONTROLLER-thread: myread(command) function failed", 3); }
    // printf("CONTROLLER-thread %ld: command is:%s\n", pthread_self(), command);

    //Parse command
    char command_copy[size_to_receive];
    strcpy(command_copy, command);                          //strcpy the command, because strtok destroys the initial string
    char* arg_token;
    arg_token = strtok(command_copy, " ");

    //if command = "issueJob":
    if (strcmp(arg_token, "issueJob") == 0) {

        //Parse the rest command, after "issueJob"
        char* rest_command;
        if ((rest_command = strchr(command, ' ')) == NULL) { error_exit("CONTROLLER-thread: strchr(issueJob_restcommand) failed", 13); }
        rest_command++;                             //for skipping space (created at the front of rest command with strchr(command, ' '))

        //Allocate memory for the Job
        Job_t *new_job = malloc(sizeof(Job_t));

        //LOCK mutex_shm
        if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

            jobID_count++;                          //for creating the jobID_XX field
            int jobID_count_local = jobID_count;

        //UNLOCK mutex_shm
        if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

        //Create the "job_ID" format
        strcpy(new_job->jobID, "job_");
        int max_size_of_ascii_int = sizeof(int)*8;                      //max size an integer can be in ascii representation
        char str_jobID[max_size_of_ascii_int];
        int curr_size_of_ascii_int = myitoa(jobID_count_local, str_jobID);    //transform jobID_count into string
        if (curr_size_of_ascii_int == -1) { error_exit("CONTROLLER-thread: myitoa() function failed", -1); }
        strcat(new_job->jobID, str_jobID);                              //create the jobID as "jobID_XX", where XX is the server's jobID counter
        char jobID_response[max_size_of_ascii_int+4];                   //keep it localy, to send it in the response (+4 for the "job_")
        strcpy(jobID_response, new_job->jobID);

        //Create the job field
        strcpy(new_job->job, rest_command);                             //rest_command will be sent in the response

        //Create the clientSocket field 
        new_job->clientSocket = newsock;

        /************************************************Controller-thread as Producer*****************************************************/

        //LOCK mutex_shm
        if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

            //Wait on condition full, if buffer is full
	        while (jobBuffer->count >= jobBuffer->bufferSize && exit_commanded == false) {
		        if (pthread_cond_wait(&cond_full, &mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_cond_wait(cond_full) failed", 7); }
		    }
            if (exit_commanded == false) {

                jobBuffer->end = (jobBuffer->end + 1) % jobBuffer->bufferSize;
                jobBuffer->buffer[jobBuffer->end] = malloc(sizeof(Job_t));
                memcpy(jobBuffer->buffer[jobBuffer->end], new_job, sizeof(Job_t));
                free(new_job);
                jobBuffer->count++;

                // printf("CONTROLLER-thread %ld: Job placed on buffer is: %s %s %d\n", pthread_self(), jobBuffer->buffer[jobBuffer->end]->jobID, jobBuffer->buffer[jobBuffer->end]->job, jobBuffer->buffer[jobBuffer->end]->clientSocket);

                //Send response to commander (it is inside the mutex lock, to ensure that the controller's first response will be read first from the client)
                //Create the response of the form "<jobID,job>"
                char issueJob_response[strlen("JOB <,> SUBMITTED") + strlen(jobID_response) + strlen(rest_command) + 1];   //+1 for the '\0'
                strcpy(issueJob_response, "JOB <");
                strcat(issueJob_response, jobID_response);
                strcat(issueJob_response, ",");
                strcat(issueJob_response, rest_command);
                strcat(issueJob_response, "> SUBMITTED");
                size_t size_to_send = strlen(issueJob_response)+1;
                uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
                if (write(newsock, &size_to_send_net, sizeof(uint32_t)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on issueJob_response command failed", 12); }
                if (write(newsock, issueJob_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(issueJob_response) failed", 12); }

                //Signal (broadcast) the condition in which possible workers are waiting
                if (pthread_cond_broadcast(&cond_empty) == -1) { error_exit("CONTROLLER-thread: pthread_cond_broadcast(cond_empty) failed", 7); }

            } else {    //exit_commanded == true
                free(new_job);
                //Send response to commander (it is inside the mutex lock, to ensure that the controller's first response will be read first from the client)
                //Create the response of the form "<jobID,job>"
                char issueJob_response[strlen("JOB <,> SUBMITTED") + strlen(jobID_response) + strlen(rest_command) + 1];   //+1 for the '\0'
                strcpy(issueJob_response, "JOB <");
                strcat(issueJob_response, jobID_response);
                strcat(issueJob_response, ",");
                strcat(issueJob_response, rest_command);
                strcat(issueJob_response, "> NOT SUBMITTED");
                size_t size_to_send = strlen(issueJob_response)+1;
                uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
                if (write(newsock, &size_to_send_net, sizeof(uint32_t)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on issueJob_response command failed", 12); }
                if (write(newsock, issueJob_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(issueJob_response) failed", 12); }

                //Send a new 2nd response to the other commander that commanded this specific job
                char *another_issueJob_response = "SERVER TERMINATED BEFORE SUBMITTION/EXECUTION\n";
                size_t size_to_send_another = strlen(another_issueJob_response)+1;
                if (write(newsock, another_issueJob_response, size_to_send_another) == -1) { error_exit("CONTROLLER-thread: write(another_issueJob_response) failed", 12); }
                //Shutdown the write side of the socket, in order to close correctly
                if (shutdown(newsock, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(newsock) failed", 5); }
                //Close the socket with the specific client
                if (close(newsock) == -1) { error_exit("CONTROLLER-thread: close(newsock) failed", 1); }

                //Signal (broadcast) the condition in which possible controllers are waiting (so that everybody is awake-to-exit)
                if (pthread_cond_broadcast(&cond_full) == -1) { error_exit("CONTROLLER-thread: pthread_cond_broadcast(cond_full) failed", 7); }
            }

        //UNLOCK mutex_shm
        if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

        /**********************************************************************************************************************************/

    //if command = "setConcurrency":
    } else if (strcmp(arg_token, "setConcurrency") == 0) {

        arg_token = strtok(NULL, " ");              //parse the <N> (integer (as string) after "setConcurrency" command)
        int new_mpl;
        if (are_all_digits(arg_token) > 0) {        //if number was given
            new_mpl = atoi(arg_token);              //transform it into integer
            //LOCK mutex_shm
            if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

                concurrencyLevel =  new_mpl;        //and set the new Concurrency level
                //Signal (broadcast) the condition in which possible workers are waiting
                if (pthread_cond_broadcast(&cond_empty) == -1) { error_exit("CONTROLLER-thread: pthread_cond_broadcast(cond_empty) failed", 7); }
            
            //UNLOCK mutex_shm
            if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }
        } else {                                    // if no N given, or non-number was given
            //LOCK mutex_shm
            if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

                new_mpl =  concurrencyLevel;        //do not change the Concurrency level

            //UNLOCK mutex_shm
            if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }
        }
        // printf("CONTROLLER-thread %ld: Concurrency degree changed to %d\n", pthread_self(), new_mpl);

        //Create the response and send it back to commander
        char* init_response_string = "CONCURRENCY SET AT ";
        int max_size_of_ascii_int = sizeof(int)*8;                          //max size a integer can be in ascii representation
        char setConcurrency_response[strlen(init_response_string) + max_size_of_ascii_int + 1]; //+1 for the '\0'
        strcpy(setConcurrency_response, init_response_string);
        char str_concurrencyLevel[max_size_of_ascii_int];
        int curr_size_of_ascii_int = myitoa(new_mpl, str_concurrencyLevel); //transform concurrencyLevel into string
        if (curr_size_of_ascii_int == -1) { error_exit("CONTROLLER-thread: myitoa function failed", 11); }
        strcat(setConcurrency_response, str_concurrencyLevel);
        size_t size_to_send = strlen(setConcurrency_response)+1;
        uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
        if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on setConcurrency command failed", 12); }
        if (write(newsock, setConcurrency_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(setConcurrency_response) failed", 12); }

        printf("CONTROLLER-thread %ld: Closing connection on socket %d.\n", pthread_self(), newsock);

        //Shutdown the write side of the socket, in order to close correctly
        if (shutdown(newsock, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket) failed", 5); }
        //Close the socket with the specific client
        if (close(newsock) == -1) { error_exit("CONTROLLER-thread: close(clientSocket) failed", 1); }

    //if command = "stop":
    } else if (strcmp(arg_token, "stop") == 0) {

        arg_token = strtok(NULL, " ");              //parse the <jobID> ("job_XX" after "stop" command)

        //LOCK mutex_shm
        if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

            if ((jobBuffer->count > 0) && (arg_token != NULL)) {        //if jobBuffer has elements and arg_token was given
                int i = jobBuffer->start;
                bool found = false;
                int found_position;
                //Traverse the jobBuffer (from start to end)
                while ((i != jobBuffer->end) && (found == false)) {
                    if (strcmp(jobBuffer->buffer[i]->jobID, arg_token) == 0) {  //element (job) found

                        found_position = i;                                     //element's (job's) index

                        //Send a new 2nd response to the other commander that commanded this specific job
                        int sock_another = jobBuffer->buffer[found_position]->clientSocket;
                        char *another_issueJob_response = "JOB WAS STOPPED BEFORE EXECUTION\n";
                        size_t size_to_send = strlen(another_issueJob_response)+1;
                        if (write(sock_another, another_issueJob_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(another_issueJob_response) failed", 12); }
                        //Shutdown the write side of the socket, in order to close correctly
                        if (shutdown(sock_another, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket_another) failed", 5); }
                        //Close the socket with the specific client
                        if (close(sock_another) == -1) { error_exit("CONTROLLER-thread: close(clientSocket_another) failed", 1); }

                        //Delete the specific job found
                        free(jobBuffer->buffer[found_position]);
                        jobBuffer->buffer[found_position] = NULL;

                    
                        // printf("CONTROLLER-thread %ld: found %s inside buffer, and it is going to be stopped\n", pthread_self(), arg_token);
                        found = true;
                    }
                    i = (i + 1) % jobBuffer->bufferSize;
                }
                //Check the element at jobBuffer->end too
                if ((found == false) && (strcmp(jobBuffer->buffer[i]->jobID, arg_token) == 0)) {

                    found_position = i;                                     //element's (job's) index

                    //Send a new 2nd response to the other commander that commanded this specific job
                    int sock_another = jobBuffer->buffer[found_position]->clientSocket;
                    char *another_issueJob_response = "JOB WAS STOPPED BEFORE EXECUTION\n";
                    size_t size_to_send = strlen(another_issueJob_response)+1;
                    if (write(sock_another, another_issueJob_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(another_issueJob_response) failed", 12); }
                    //Shutdown the write side of the socket, in order to close correctly
                    if (shutdown(sock_another, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket_another) failed", 5); }
                    //Close the socket with the specific client
                    if (close(sock_another) == -1) { error_exit("CONTROLLER-thread: close(clientSocket_another) failed", 1); }

                    //Delete the specific job found
                    free(jobBuffer->buffer[found_position]);
                    jobBuffer->buffer[found_position] = NULL;

                    // printf("CONTROLLER-thread %ld: found %s inside buffer, and it is going to be stopped\n", pthread_self(), arg_token);
                    found = true;
                }

                if (found == true) {
                    //Shift elements to fill the gap of the deleted job
                    int j = found_position;
                    int nextIndex;
                    // if (j == jobBuffer->start) {
                    //     nextIndex = (j + 1) % jobBuffer->bufferSize;
                    //     jobBuffer->start = nextIndex;
                    //     j = nextIndex;
                    // }
                    while (j != jobBuffer->end) {
                        nextIndex = (j + 1) % jobBuffer->bufferSize;
                        jobBuffer->buffer[j] = jobBuffer->buffer[nextIndex];
                        j = nextIndex;
                    }

                    //Update jobBuffer->end and jobBuffer->count
                    jobBuffer->end = (jobBuffer->end - 1 + jobBuffer->bufferSize) % jobBuffer->bufferSize;
                    jobBuffer->count--;
                    
                    //Signal (broadcast) the condition in which possible controllers are waiting
                    if (pthread_cond_broadcast(&cond_full) == -1) { error_exit("WORKER-thread: pthread_cond_broadcast(cond_full) failed", 7); }
                }

                char stop_response[DFLBUFSIZE];
                if (found == true) {
                    strcpy(stop_response, "JOB <");
                    strcat(stop_response, arg_token);
                    strcat(stop_response, "> REMOVED");
                } else {
                    strcpy(stop_response, "JOB <");
                    strcat(stop_response, arg_token);
                    strcat(stop_response, "> NOTFOUND");
                }
                size_t size_to_send = strlen(stop_response)+1;
                uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
                if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on stop command failed", 12); }
                if (write(newsock, stop_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(stop_response) failed", 12); }

            } else {        //if jobBuffer is empty or arg_token wasn't given, return "NOTFOUND"
                char stop_response[DFLBUFSIZE];
                strcpy(stop_response, "JOB <");
                if (arg_token == NULL) {    
                    strcat(stop_response, "");  //to avoid error on strcat(stop_response, NULL) when arg_token isn't given
                } else {
                    strcat(stop_response, arg_token);
                }
                strcat(stop_response, "> NOTFOUND");
                size_t size_to_send = strlen(stop_response)+1;
                uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
                if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on stop command failed", 12); }
                if (write(newsock, stop_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(stop_response) failed", 12); }
            }

        //UNLOCK mutex_shm
        if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

        printf("CONTROLLER-thread %ld: Closing connection on socket %d.\n", pthread_self(), newsock);

        //Shutdown the write side of the socket, in order to close correctly
        if (shutdown(newsock, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket) failed", 5); }
        //Close the socket with the specific client
        if (close(newsock) == -1) { error_exit("CONTROLLER-thread: close(clientSocket) failed", 1); }

    //if command = "poll":
    } else if (strcmp(arg_token, "poll") == 0) {

        //LOCK mutex_shm
        if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

            if (jobBuffer->count > 0) {                 //if jobBuffer is not empty
                char poll_response[DFLBUFSIZE];
                strcpy(poll_response, "");
                int i = jobBuffer->start;
                //Traverse the jobBuffer (from start to end)
                while (i != jobBuffer->end) {
                    if (jobBuffer->buffer[i] != NULL) {  //NULL indicates absence or deleted element
                        strcat(poll_response, "<");
                        strcat(poll_response, jobBuffer->buffer[i]->jobID);
                        strcat(poll_response, ",");
                        strcat(poll_response, jobBuffer->buffer[i]->job);
                        strcat(poll_response, ">\n");
                    }
                    i = (i + 1) % jobBuffer->bufferSize;
                }
                //Check the element at jobBuffer->end too
                if (jobBuffer->buffer[i] != NULL) {
                    strcat(poll_response, "<");
                    strcat(poll_response, jobBuffer->buffer[i]->jobID);
                    strcat(poll_response, ",");
                    strcat(poll_response, jobBuffer->buffer[i]->job);
                    strcat(poll_response, ">\n");
                }
                //Send the whole response as one message
                size_t size_to_send = strlen(poll_response)+1;
                uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
                if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on poll command failed", 12); }
                if (write(newsock, poll_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(poll_response) failed", 12); }

            } else {                                //if jobBuffer is empty return a specific message
                char *poll_response = "NO JOB FOUND TO BE POLLED";
                size_t size_to_send = strlen(poll_response)+1;
                uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
                if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on poll command failed", 12); }
                if (write(newsock, poll_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(poll_response) failed", 12); }
            }

        //UNLOCK mutex_shm
        if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

        printf("CONTROLLER-thread %ld: Closing connection on socket %d.\n", pthread_self(), newsock);

        //Shutdown the write side of the socket, in order to close correctly
        if (shutdown(newsock, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket) failed", 5); }
        //Close the socket with the specific client
        if (close(newsock) == -1) { error_exit("CONTROLLER-thread: close(clientSocket) failed", 1); }

    //if command = "exit":
    } else if (strcmp(arg_token, "exit") == 0) {

        //LOCK mutex_shm
        if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

            exit_commanded = true;

            //Signal (broadcast) the condition in which possible controllers are waiting (1 time, and the woken controller will do it again, etc...)
            if (pthread_cond_broadcast(&cond_full) == -1) { error_exit("WORKER-thread: pthread_cond_broadcast(cond_full) failed", 7); }

            //Signal (broadcast) the condition in which possible workers are waiting (1 time, and the woken worker will do it again, etc...)
            if (pthread_cond_broadcast(&cond_empty) == -1) { error_exit("WORKER-thread: pthread_cond_broadcast(cond_empty) failed", 7); }

        //UNLOCK mutex_shm
        if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("CONTROLLER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

        char* exit_response = "SERVER TERMINATED";
        size_t size_to_send = strlen(exit_response)+1;
        uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
        if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on exit command failed", 12); }
        if (write(newsock, exit_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(exit_response) failed", 12); }

        printf("CONTROLLER-thread %ld: Closing connection on socket %d.\n", pthread_self(), newsock);

        //Shutdown the write side of the socket, in order to close correctly
        if (shutdown(newsock, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket) failed", 5); }
        //Close the socket with the specific client
        if (close(newsock) == -1) { error_exit("CONTROLLER-thread: close(clientSocket) failed", 1); }

    //if command is unknown:
    } else {

        char* other_response = "COMMAND SENT TO SERVER IS UNKNOWN";
        size_t size_to_send = strlen(other_response)+1;
        uint32_t size_to_send_net = htonl((uint32_t)size_to_send);
        if (write(newsock, &size_to_send_net, sizeof(int)) == -1) { error_exit("CONTROLLER-thread: write(size_to_send) on unknown command failed", 12); }
        if (write(newsock, other_response, size_to_send) == -1) { error_exit("CONTROLLER-thread: write(other_response) failed", 12); }

        printf("CONTROLLER-thread %ld: Closing connection on socket %d.\n", pthread_self(), newsock);

        //Shutdown the write side of the socket, in order to close correctly
        if (shutdown(newsock, SHUT_WR) == -1) { error_exit("CONTROLLER-thread: shutdown(clientSocket) failed", 5); }
        //Close the socket with the specific client
        if (close(newsock) == -1) { error_exit("CONTROLLER-thread: close(clientSocket) failed", 1); }
    }

    return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////