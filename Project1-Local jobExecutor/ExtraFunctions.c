#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "MyHeader.h"

//Check-to-Run function, which checks whether waiting job(s) can be executed, and executes it(them)
void check_to_run(int* current_mpl, int concurrency_degree, Queue* waiting_queue, Queue* running_queue) {
    while ((*current_mpl) < concurrency_degree && !isQueueEmpty(waiting_queue)) {
        // printf("SERVER %d: Current jobs running are less that the concurrency degree, and there are jobs waiting to be executed\n", getpid());
        Job* job_entry = removeJob(waiting_queue);          //remove the first (front) job from the Waiting Queue
        // printf("SERVER %d: This job just removed from waiting_queue, and it is going to be executed: ", getpid());
        // printf("%s ", job_entry->job);
        // printf("%s ", job_entry->jobID);
        // printf("%d ", job_entry->queuePosition);
        // printf("\n");

        //Parse and create the arguments, given a whole job-command as one string
        char job_to_exec[DFLBUFSIZE];                       //same max size as struct Job's job field
        strcpy(job_to_exec, job_entry->job);                //copy the struct Job's job field into the new string
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
            perror("fork failed");
        //Child-job
        } else if (jobpid == 0) {
            // printf("JOB %d: I'm going to exec: %s\n", getpid(), job_argv[0]);
            execvp(job_argv[0], job_argv);                  //execvp (v as i created a vector-array with arguments, p to support sysprograms etc)
            perror("exec returned");                        //if exec returned, error occured
            exit(5);                                        //exit immediately to avoid other errors
        //Parent-server
        } else {
            // printf("SERVER %d: I just forked a job\n", getpid());
            job_entry->jobPID = jobpid;                     //job executed now has a PID
            insertJob(running_queue, job_entry);            //insert it into the Running Queue
            (*current_mpl)++;                               //increase the current multiprogramming level
        }
    }
}

//Integer to Ascii conversion (only for positive numbers (pids in particular))
int myitoa(pid_t number, char* str) {

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

//My Read function, using a loop for the correct reading
int myread(int fd, void* buf, int msg_size) {
    int to_read = msg_size;
    int rsize = 0;
    do {
        rsize = read(fd, buf, to_read);                     //read
        if (rsize == -1) {  
            perror("myread function: read() failed");
            return -1;
        }
        to_read = to_read - rsize;                          //decrease the bytes_to_be_read with current_read_size
    } while (to_read > 0);                                  //repeat until have read all bytes asked (until bytes_to_be_read becomes 0)

    return 0;                                               //return 0, meaning myread was successful
}