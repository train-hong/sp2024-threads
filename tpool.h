#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

/* Definitions for matrices and vectors */
typedef int** Matrix;
typedef int* Vector;

/* Work structure for backend threads */
typedef struct work {
    Matrix a;          // Pointer to Matrix a
    Matrix b;          // Pointer to Matrix b (transposed)
    Matrix c;          // Pointer to Matrix c (result matrix)
    int start;         // Starting index of work
    int end;           // Ending index of work
    struct work* next; // Pointer to the next work in the queue
} work_t;

/* Queue for frontend and backend */
typedef struct queue {
    work_t* head;   // Front of the queue
    work_t* tail;   // End of the queue
    int size;       // Number of items in the queue
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} queue_t;

/* Thread pool structure */
struct tpool {
    int num_threads;          // Number of backend threads
    int n;                    // Size of matrices (n x n)
    pthread_t* threads;       // Array of backend thread IDs
    pthread_t frontend_thread;// Frontend thread ID
    queue_t frontend_queue;   // Queue for frontend requests
    queue_t worker_queue;     // Queue for backend workers
    bool stop;                // Flag to stop the threads
    pthread_mutex_t sync_mutex;
    pthread_cond_t sync_cond;
    int unfinished_works;
};

/* Thread pool interface */
struct tpool* tpool_init(int num_threads, int n);
void tpool_request(struct tpool*, Matrix a, Matrix b, Matrix c, int num_works);
void tpool_synchronize(struct tpool*);
void tpool_destroy(struct tpool*);
int calculation(int n, Vector, Vector);  // Already implemented
