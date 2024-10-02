#ifndef SERVERINTERFACE_H
#define SERVERINTERFACE_H
/* ServerInterface.h */
#include <pthread.h>            //pthread_mutex_t,pthread_cond_t,pthread_t types

#define DFLBUFSIZE 4096         //define default buffer size

//For passing the argument to controller thread
struct Arg {
    int mysock;                 //new socket created for specific connection
    int initialsock;            //initial listening socket
};

//Struct Job <jobID,job,clientSocket>
typedef struct {
    char jobID[256];            //job_XX form
    char job[DFLBUFSIZE];       //any name of an executable + arguments/flags, except these symbols --> (;|&`><$(){}[]*?~)
    int clientSocket;           //socket id with commander who send the specific job
} Job_t;

//Circular buffer to store waiting jobs
typedef struct {
    Job_t **buffer;             //buffer of pointers to jobs
    int bufferSize;             //buffer's size
    int start;                  //current start of the buffer (circular)
    int end;                    //current end of the buffer (circular)
    int count;                  //current count of jobs waiting on buffer
} JobBuffer_t;


//Declare the shared memory objects as extern, to be viewed by other .c files of the program too
/******************************** Shared-Global Memory *********************************/
extern int concurrencyLevel;            //multiprogramming level
extern int jobID_count;                 //counter of total jobs that have been received
extern int running_jobs;                //counter of current running jobs
extern bool exit_commanded;             //flag of exit command received
extern int controllers_running;         //counter of active controllers
extern bool entered;                    //just a small flag to let only one controller exit the program

extern JobBuffer_t *jobBuffer;          //buffer for storing waiting jobs

extern pthread_mutex_t mutex_shm;       //mutex for all the shared memory
extern pthread_cond_t cond_full;        //condition for full buffer
extern pthread_cond_t cond_empty;       //condition for empty buffer (and concurrency)
extern pthread_cond_t cond_exit;        //just a small condition for the controllers_running, used in the exit_program() function

extern int threadPoolSize;              //number of worker threads created
extern pthread_t* wrk_id;               //worker-threads-ids array

/***************************************************************************************/

//Server Functions
void worker(void);                              //inside Worker.c
void controller(int);                           //inside Controller.c
void exit_program(int);                         //inside jobExecutorServer.c
void initialize_jobBuffer(JobBuffer_t *, int);  //inside jobExecutorServer.c
void free_jobBuffer(JobBuffer_t *, int);        //inside jobExecutorServer.c


#endif