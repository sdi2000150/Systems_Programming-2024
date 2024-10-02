#define DFLBUFSIZE 4096

//Struct Job <jobID,job,queuePosition> (+jobPID) (of Queue's Node data)
typedef struct {
    char jobID[256];            //job_XX form
    char job[DFLBUFSIZE];       //any name of an executable + arguments/flags
    int queuePosition;          //current position in queue
    pid_t jobPID;               //used only for the running queue
} Job;

//Struct Node (of Queue)
typedef struct Node {
    Job *jobEntry;              //elements of Node is a Job struct
    struct Node *next;          //pointer to next Node
} Node;

//Struct Queue
typedef struct {
    Node *front;                //pointer to Queue's front Node
    Node *rear;                 //pointer to Queue's rear Node
} Queue;

//Queue's Functions
Node *createNode(Job *);
void initializeQueue(Queue *);
int isQueueEmpty(Queue *);
int insertJob(Queue *, Job *);
Job* removeJob(Queue *);
Job* removePIDSpecificJob(Queue *, pid_t);
Job* removejobIDSpecificJob(Queue *, char*);
void printQueue(Queue *);
void pollQueue(Queue*, char*);
void freeQueue(Queue *);

//Extra Functions
void check_to_run(int* , int, Queue*, Queue*);
int myitoa(pid_t, char*);
int myread(int, void*, int);