#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MyHeader.h"

//Function which creates a new Node (and initializes its jobEntry field)
Node *createNode(Job *new_job) {
    Node *new_node = malloc(sizeof(Node));                  //allocate space for the new Node
    new_node->jobEntry = new_job;                           //jobEntry field becomes the new_job given (which is already allocated and filled by the invoking program)
    new_node->next = NULL;                                  //Node's next entry becomes NULL
    return new_node;
}

//Function which initializes the Queue (to empty-NULL)
void initializeQueue(Queue *q) {
    q->front = NULL;
    q->rear = NULL;
}

//Function to check if Queue is empty or not
int isQueueEmpty(Queue *q) {
    return (q->front == NULL);                              //if queue is empty 1 is returned, otherwise 0 is returned
}

//Function which inserts a new element (job) inside the Queue's rear
int insertJob(Queue *q, Job *new_job) {
    Node *new_node = createNode(new_job);                   //create a new Node to accomodate the new job
    if (isQueueEmpty(q)) {                                  //If Queue is empty, both front and rear will point to this Node
        new_node->jobEntry->queuePosition = 1;              //job's queuePosition is 1 in this case
        q->front = new_node;
        q->rear = new_node;
    } else if (q->front == q->rear) {                       //If queue has only 1 element,
        new_node->jobEntry->queuePosition = (q->rear->jobEntry->queuePosition) + 1; //set the queue position
        q->rear = new_node;                                 //new Node will go to rear
        q->front->next = q->rear;                           //front's next Node is now the rear Node
    } else {                                                //In any other case,
        new_node->jobEntry->queuePosition = (q->rear->jobEntry->queuePosition) + 1; //set the queue position
        q->rear->next = new_node;                           //new NOde will go to Queue's rearest 
        q->rear = new_node;                                 //Node's position is the new rear
    }
    return new_node->jobEntry->queuePosition;
}

//Function which removes an element (job) from the Queue's front
Job* removeJob(Queue *q) {
    if (isQueueEmpty(q)) {                                  //If attempting to remove from an empty Queue,
        // printf("Queue is empty!\n");
        exit(1);                                            //exit immediately to avoid other errors
    } else if (q->front == q->rear) {                       //If queue has only 1 element,
        Job *job_entry = q->front->jobEntry;                //remove that one element
        q->front = NULL;
        q->rear = NULL;
        free(q->front);                                     //free its Node space
        return job_entry;                                   //and return it
    } else {                                                //In any other case,
        Job *job_entry = q->front->jobEntry;                //remove the job from the front of the Queue
        Node *temp = q->front;
        q->front = q->front->next;                          //set the new front
        Node *temp_next = q->front;
        while (temp_next != NULL) {
            (temp_next->jobEntry->queuePosition)--;         //update the rest elements' queue positions
            temp_next = temp_next->next;                    //for traversing the rest elements
        }
        free(temp);                                         //free its (old front's) Node space
        return job_entry;                                   //and return it
    }
}

//Function which removes a specific element (job) from the Queue based on PID
Job* removePIDSpecificJob(Queue *q, pid_t zombie_pid) {
    if (isQueueEmpty(q)) {                                  //If attempting to remove from an empty Queue,
        // printf("Queue is empty!\n");
        return NULL;
    } else if (q->front == q->rear) {                       //If queue has only 1 element,
        if(q->front->jobEntry->jobPID == zombie_pid) {      //if it matches with the desired to remove
            Job *job_entry = q->front->jobEntry;            //remove that one element
            q->front = NULL;
            q->rear = NULL;
            free(q->front);                                 //free its Node space
            return job_entry;                               //and return it
        } else {
            return NULL;                                    //otherwise, specific PID was not found, return NULL
        }
    } else {                                                //In any other case,
        Node *temp = q->front;
        Node *temp_prev = q->front;
        while (temp->jobEntry->jobPID != zombie_pid || temp == NULL) {      //traverse the whole Queue, looking for the specific job
            temp_prev = temp;
            temp = temp->next;
        }
        if (temp != NULL) {                                 //specific job was found
            Job *job_entry = temp->jobEntry;                //when found, save it
            temp_prev->next = temp->next;                   //update the respective previous and next Node, so they to be connected now
            Node *temp_forward = temp->next;
            while (temp_forward != NULL) {
                (temp_forward->jobEntry->queuePosition)--;  //update the rest elements' queue positions
                temp_forward = temp_forward->next;          //for traversing the rest elements
            }
            free(temp);                                     //free its Node space
            return job_entry;                               //and return it
        } else {
            return NULL;                                    //otherwise, specific PID was not found, return NULL
        }
    }
}

//Function which removes a specific element (job) from the Queue based on jobID
Job* removejobIDSpecificJob(Queue *q, char* jobID_remove) {
    if (isQueueEmpty(q)) {                                  //If attempting to remove from an empty Queue,
        // printf("Queue is empty!\n");
        return NULL;
    } else if (q->front == q->rear) {                       //If queue has only 1 element,
        if(strcmp(q->front->jobEntry->jobID, jobID_remove) == 0) {  //if it matches with the desired to remove
            Job *job_entry = q->front->jobEntry;            //remove that one element
            q->front = NULL;
            q->rear = NULL;
            free(q->front);                                 //free its Node space
            return job_entry;                               //and return it
        } else {
            return NULL;                                    //otherwise, specific jobID was not found, return NULL
        }
    } else {                                                //In any other case,
        Node *temp = q->front;
        Node *temp_prev = q->front;
        while (strcmp(temp->jobEntry->jobID, jobID_remove) != 0 || temp == NULL) {      //traverse the whole Queue, looking for the specific job
            temp_prev = temp;
            temp = temp->next;
        }
        if (temp != NULL) {                                 //specific job was found
            Job *job_entry = temp->jobEntry;                //when found, save it
            temp_prev->next = temp->next;                   //update the respective previous and next Node, so they to be connected now
            Node *temp_forward = temp->next;
            while (temp_forward != NULL) {
                (temp_forward->jobEntry->queuePosition)--;  //update the rest elements' queue positions
                temp_forward = temp_forward->next;          //for traversing the rest elements
            }
            free(temp);                                     //free its Node space
            return job_entry;                               //and return it
        } else {
            return NULL;                                    //otherwise, specific jobID was not found, return NULL
        }
    }
}

//Function to display the Queue
void printQueue(Queue *q) {
    printf("Queue is:\n");
    if (isQueueEmpty(q)) {
        printf("Queue is empty!\n");
    } else if (q->front == q->rear) {                       //if Queue has only one element, print it
        printf("%s ", q->front->jobEntry->job);
        printf("%s ", q->front->jobEntry->jobID);
        printf("%d ", q->front->jobEntry->queuePosition);
        if (q->front->jobEntry->jobPID != 0) {              //print pid field too, (if it is about the Running Queue)
            printf("%d ", q->front->jobEntry->jobPID);
        }
        printf("\n");
    } else {                                                //else, traverse the whole Queue and print all its elements
        Node *current = q->front;
        while (current != NULL) {
            printf("%s ", current->jobEntry->job);
            printf("%s ", current->jobEntry->jobID);
            printf("%d ", current->jobEntry->queuePosition);
            if (current->jobEntry->jobPID != 0) {           //print pid field too, (if it is about the Running Queue)
                printf("%d ", current->jobEntry->jobPID);
            }
            printf("\n");
            current = current->next;
        }
    }
}

void pollQueue(Queue* queue, char* poll_response) {
    if (isQueueEmpty(queue)) {
        strcpy(poll_response, "");
    } else if (queue->front == queue->rear) {                       //if Queue has only one element, print it
        strcpy(poll_response, "<");
        strcat(poll_response, queue->front->jobEntry->jobID);
        strcat(poll_response, ",");
        strcat(poll_response, queue->front->jobEntry->job);
        strcat(poll_response, ",");
        int max_size_of_ascii_int = sizeof(int)*8;                      //max size a integer can be in ascii representation
        char str_queuePosition_response[max_size_of_ascii_int];
        int curr_size_of_ascii_int = myitoa(queue->front->jobEntry->queuePosition, str_queuePosition_response);    //transform queue_position into string
        if (curr_size_of_ascii_int == -1) { perror("SERVER: myitoa function failed"); exit(11);}
        strcat(poll_response, str_queuePosition_response);
        strcat(poll_response, ">");
        strcat(poll_response, "\n");
    } else {                                                //else, traverse the whole Queue and print all its elements
        Node *current = queue->front;
        strcpy(poll_response, "");
        while (current != NULL) {
            strcat(poll_response, "<");
            strcat(poll_response, current->jobEntry->jobID);
            strcat(poll_response, ",");
            strcat(poll_response, current->jobEntry->job);
            strcat(poll_response, ",");
            int max_size_of_ascii_int = sizeof(int)*8;                      //max size a integer can be in ascii representation
            char str_queuePosition_response[max_size_of_ascii_int];
            int curr_size_of_ascii_int = myitoa(current->jobEntry->queuePosition, str_queuePosition_response);    //transform queue_position into string
            if (curr_size_of_ascii_int == -1) { perror("SERVER: myitoa function failed"); exit(11);}
            strcat(poll_response, str_queuePosition_response);
            strcat(poll_response, ">");
            strcat(poll_response, "\n");
            current = current->next;
        }
    }
}

//Function to free all memory allocated for the queue
void freeQueue(Queue *q) {
    while (!isQueueEmpty(q)) {                              //while Queue is not empty
        Job* job_entry = removeJob(q);                      //remove one element
        free(job_entry);                                    //and free Job space too
    }                                                       //(until Queue becomes empty, so that all elements have been removed and freed)
}

