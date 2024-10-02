#define _GNU_SOURCE
/* Worker.c */
#include <stdio.h>
#include <unistd.h>         //write(),read(),close()
#include <sys/wait.h>	    //sockets
#include <sys/types.h>	    //sockets
#include <sys/socket.h>	    //sockets
#include <netdb.h>	        //sockets
#include <netinet/in.h>	    //internet sockets
#include <pthread.h>        //threads
#include <unistd.h>	        //fork(),execvp()
#include <stdlib.h>	        //malloc(),free()
#include <string.h>         //memcpy(),strcpy(),strtok(),strlen()
#include <fcntl.h>          //creat(),open(),
#include <stdbool.h>        //bool type(true/false)

#include "../include/CommonInterface.h"
#include "../include/ServerInterface.h"

/////////////////////////////////////////////////////////////* WORKER-thread procedure */////////////////////////////////////////////////////////////
void worker(void) {

    bool break_while = false;

    while(1) {
        Job_t *job_selected = malloc(sizeof(Job_t));

        /*************************************************Worker-thread as Consumer******************************************************/

        //LOCK mutex_shm
        if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("WORKER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

            //Wait on condition empty, if buffer is empty or running jobs are already as many as the concurrency level
	        while ((jobBuffer->count <= 0 || running_jobs >= concurrencyLevel) && exit_commanded == false) {
		        if (pthread_cond_wait(&cond_empty, &mutex_shm) == -1) { error_exit("WORKER-thread: pthread_cond_wait(cond_empty) failed", 7); }
		    }
            if (exit_commanded == false) {
                //Woke up, so worker can run a job from the buffer
                memcpy(job_selected, jobBuffer->buffer[jobBuffer->start], sizeof(Job_t)); //get the job indicated by "start"
                free(jobBuffer->buffer[jobBuffer->start]);
                jobBuffer->buffer[jobBuffer->start] = NULL;

                // printf("WORKER-thread %ld: Job obtained to run is: %s %s %d\n", pthread_self(), job_selected->jobID, job_selected->job, job_selected->clientSocket);

                jobBuffer->start = (jobBuffer->start + 1) % jobBuffer->bufferSize;  //start = (start + 1) % N
                jobBuffer->count--;                                                 //buffer has one less job now             
                running_jobs++;                                                     //running jobs are plus one

                //Signal (broadcast) the condition in which possible controllers are waiting
                if (pthread_cond_broadcast(&cond_full) == -1) { error_exit("WORKER-thread: pthread_cond_broadcast(cond_full) failed", 7); }
            } else {
                break_while = true;
                //Signal (broadcast) the condition in which possible workers are waiting (so that everybody is awake-to-exit)
                if (pthread_cond_broadcast(&cond_empty) == -1) { error_exit("WORKER-thread: pthread_cond_broadcast(cond_empty) failed", 7); }
            }

        //UNLOCK mutex_shm
        if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("WORKER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }

        /********************************************************************************************************************************/

        if (break_while == true) {
            free(job_selected);
            break;
        }

        //Parse and create the arguments, given a whole job-command as one string
        char job_to_exec[DFLBUFSIZE];                       //same max size as struct Job's job field
        strcpy(job_to_exec, job_selected->job);             //copy the struct Job's job field into the new string
        // printf("WORKER-thread %ld: Job given to be executed: %s\n", pthread_self(), job_to_exec);
        sanitize(job_to_exec);                              //sanitize command before executing it, in order to be safe to run
        // printf("WORKER-thread %ld: Job to be executed after sanitize: %s\n", pthread_self(), job_to_exec);
        char* job_argv[DFLBUFSIZE];                         //array-vector of pointers to strings (for keeping each argument)
        int i = 0;
        job_argv[i] = strtok(job_to_exec, " ");             //in the first position of the array, put the first argument of the job
        while (job_argv[i] !=  NULL) {                      //NULL will be included in the array's last position
            i++;
            job_argv[i] = strtok(NULL, " ");                //in each next position of the array, put the next argument of the job
        }

        //Fork the child-job to be executed
        pid_t jobpid = fork();
        if (jobpid == -1) {
            error_exit("WORKER-thread: fork() failed", 4);

        //Child-job
        } else if (jobpid == 0) {


            //Create the name of the file "pid.output"
            char output_filename[DFLBUFSIZE];
            if (snprintf(output_filename, sizeof(output_filename), "%d.output", getpid()) == -1) { error_exit("CHILD-of-WORKER-thread: snprintf(pid.output) failed", 13); }
            //Create the file "pid.output" (with default flags O_WRONLY | O_CREAT | O_TRUNC)
            int fd_output;
            if ((fd_output = creat(output_filename, 0644)) == -1) { error_exit("CHILD-of-WORKER-thread: creat(pid.output) failed", 1); }

            //Duplicate STDOUT of child to file "pid.output"
            if (dup2(fd_output, STDOUT_FILENO) == -1) { error_exit("CHILD-of-WORKER-thread: dup2() failed", 1); }   //STDOUT_FILENO is like STDOUT but for low-level sycalls (it is the file descriptor 1)

            //File descriptor of the "pid.output" no longer needed, close it
            if (close(fd_output) == -1) { error_exit("CHILD-of-WORKER-thread: close(pid.output) failed", 1); }

            //Execute the job
            execvp(job_argv[0], job_argv);                  //execvp (v as I created a vector-array with arguments, p to support sysprograms etc)
            //If exec returned, error occured. Exit immediately to avoid other errors
            error_exit("CHILD-of-WORKER-thread: execvp(job) returned-failed", 4);

        //Parent-server
        } else {
            // printf("WORKER-thread %ld: I just forked a job\n", pthread_self());

            //Wait for the child process to finish
            int status;
            pid_t child_pid;
            if ((child_pid = waitpid(jobpid, &status, 0)) == -1) { error_exit("WORKER-thread: waitpid() failed", 4); }
            //Check if the child process exited normally
            // if (WIFEXITED(status)) { printf("WORKER-thread %ld: child process %d exited with status %d\n", pthread_self(), child_pid, WEXITSTATUS(status)); } 
            // else { printf("WORKER-thread %ld: child process %d did not exit normally\n", pthread_self(), child_pid); }

            //LOCK mutex_shm
            if (pthread_mutex_lock(&mutex_shm) == -1) { error_exit("WORKER-thread: pthread_mutex_lock(mutex_shm) failed", 7); }

                running_jobs--;                         //running jobs are minus one

                //Signal (broadcast) the condition in which possible workers are waiting
                if (pthread_cond_broadcast(&cond_empty) == -1) { error_exit("WORKER-thread: pthread_cond_broadcast(cond_empty) failed", 7); }

            //UNLOCK mutex_shm
            if (pthread_mutex_unlock(&mutex_shm) == -1) { error_exit("WORKER-thread: pthread_mutex_unlock(mutex_shm) failed", 7); }
        }
        //Parent-server continues here, to return the child-job's response to commander

        //Write into the socket the first line of the response
        char buffer[DFLBUFSIZE];
        ssize_t bytes_read;
        if (snprintf(buffer, sizeof(buffer), "-----%s output start-----\n", job_selected->jobID) == -1) { error_exit("WORKER-thread: snprintf(-----jobID output start-----) failed", 13); }
        if (write(job_selected->clientSocket, buffer, strlen(buffer)) == -1) { error_exit("WORKER-thread: write() first line of response to clientSocket failed", 12); }

        //Create the name of the file "pid.output"
        char output_filename[DFLBUFSIZE];
        if (snprintf(output_filename, sizeof(output_filename), "%d.output", jobpid) == -1) { error_exit("WORKER-thread: snprintf(pid.output) failed", 13); }
        //Open the file (already created by child, and filled with his executed job's output)
        int fd_output; 
        if ((fd_output = open(output_filename, O_RDONLY)) == -1) { error_exit("WORKER-thread: open(pid.output) failed", 1); }

        //Read from the "pid.output" file and write into the socket the whole response chunk-by-chunk
        while ((bytes_read = read(fd_output, buffer, sizeof(buffer))) > 0) {
            if (write(job_selected->clientSocket, buffer, bytes_read) == -1) { error_exit("WORKER-thread: write() chunk of response to clientSocket failed", 12); }
        }

        //File descriptor of the "pid.output" no longer needed, close it
        if (close(fd_output) == -1) { error_exit("WORKER-thread: close(pid.output) failed", 1); }

        //Write into the socket the last line of the response
        if (snprintf(buffer, sizeof(buffer), "-----%s output end-----\n", job_selected->jobID) == -1) { error_exit("WORKER-thread: snprintf(-----jobID output end-----) failed", 13); }
        if (write(job_selected->clientSocket, buffer, strlen(buffer)) == -1) { error_exit("WORKER-thread: write() last line of response to clientSocket failed", 12); }
        
        //Delete the file "pid.output", as it is no longer needed
        if (unlink(output_filename) == -1) { error_exit("WORKER-thread: unlink(pid.output) failed", 1); }

        printf("WORKER-thread %ld: Closing connection on socket %d.\n", pthread_self(), job_selected->clientSocket);

        //Shutdown the write side of the socket, in order to close correctly
        if (shutdown(job_selected->clientSocket, SHUT_WR) == -1) { error_exit("WORKER-thread: shutdown(clientSocket) failed", 5); }
        //Close the socket with the specific client
        if (close(job_selected->clientSocket) == -1) { error_exit("WORKER-thread: close(clientSocket) failed", 1); }

        free(job_selected); //free the job that was malloced into the worker
    }

    return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////