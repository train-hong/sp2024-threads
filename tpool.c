#include "tpool.h"

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void queue_init(queue_t* queue) {
    queue->head = queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void queue_enqueue(queue_t* queue, work_t* work) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->tail) {
        queue->tail->next = work;
        queue->tail = work;
    } else {
        queue->head = queue->tail = work;
    }
    queue->size++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

work_t* queue_dequeue(queue_t* queue, struct tpool *pool) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0 && !pool->stop) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    if (pool->stop) {
        pthread_mutex_unlock(&queue->mutex);
        // printf("    stop in queue_dequeue\n");
        return NULL;
    }
    work_t* work = queue->head;
    queue->head = queue->head->next;
    if (!queue->head) queue->tail = NULL;
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);
    return work;
}

void* frontend_thread_func(void* arg) {
    // printf("    start in frontend\n");
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    struct tpool* pool = (struct tpool*)arg;
    while (1) {
        // printf("    start in frontend thread func while\n");
        work_t* request = queue_dequeue(&pool->frontend_queue, pool);
        if (!request) break; // Stop signal received

        // Transpose matrix b
        for (int i = 0; i < pool->n; i++)
            for (int j = i + 1; j < pool->n; j++) {
                int temp = request->b[i][j];
                request->b[i][j] = request->b[j][i];
                request->b[j][i] = temp;
            }

        // Divide tasks into smaller works
        int total_tasks = pool->n * pool->n;
        int tasks_per_work = (total_tasks + request->end - 1) / request->end;
        pthread_mutex_lock(&pool->worker_queue.mutex);
        for (int i = 0; i < request->end; i++) {
            int start = i * tasks_per_work;
            int end = start + tasks_per_work;
            if (end > total_tasks) end = total_tasks;

            work_t* work = calloc(1, sizeof(work_t));
            *work = (work_t){request->a, request->b, request->c, start, end, NULL};
            // queue_enqueue(&pool->worker_queue, work);
            if (pool->worker_queue.tail) {
                pool->worker_queue.tail->next = work;
                pool->worker_queue.tail = work;
            } else {
                pool->worker_queue.head = pool->worker_queue.tail = work;
            }
            pool->worker_queue.size++;
            pthread_cond_signal(&pool->worker_queue.cond);
        }
        pthread_mutex_unlock(&pool->worker_queue.mutex);
        // printf("    enqueued in frontend, numworks = %d\n", request->end);

        free(request);
    }
    pthread_exit(NULL);
}

void* backend_thread_func(void* arg) {
    // printf("    start in backend\n");
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    struct tpool* pool = (struct tpool*)arg;
    int i = 1;
    while (1) {
        work_t* work = queue_dequeue(&pool->worker_queue, pool);
        if (!work) {
            // printf("    stop in backend\n");
            break; // Stop signal received
        }

        // Perform calculations
        int n = pool->n;
        for (int index = work->start; index < work->end; index++) {
            int row = index / n;
            int col = index % n;
            int ret = calculation(n, work->a[row], work->b[col]);
            work->c[row][col] = ret;
        }
        free(work);
        pthread_mutex_lock(&pool->sync_mutex);
        pool->unfinished_works--;
        if (pool->unfinished_works == 0) {
            // printf("    unfinished works = 0, signal\n");
            pthread_cond_signal(&pool->sync_cond);
        }
        pthread_mutex_unlock(&pool->sync_mutex);
        // printf("    work %d done\n", i);
        i++;
    }
    // printf("    this thread did %d works\n", i);
    pthread_exit(NULL);
    return NULL;
}

struct tpool* tpool_init(int num_threads, int n) {
    struct tpool* pool = calloc(1, sizeof(struct tpool));
    pool->num_threads = num_threads;
    pool->n = n;
    pool->threads = calloc(num_threads, sizeof(pthread_t));
    queue_init(&pool->frontend_queue);
    queue_init(&pool->worker_queue);
    pthread_mutex_init(&pool->sync_mutex, NULL);
    pthread_cond_init(&pool->sync_cond, NULL);
    pool->stop = false;

    // Create frontend thread
    pthread_create(&pool->frontend_thread, NULL, frontend_thread_func, pool);

    // Create backend threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, backend_thread_func, pool);
    }
    return pool;
}

void tpool_request(struct tpool* pool, Matrix a, Matrix b, Matrix c, int num_works) {
    // printf("    start in request\n");
    work_t* request = calloc(1, sizeof(work_t));
    *request = (work_t){a, b, c, 0, num_works, NULL};
    pthread_mutex_lock(&pool->sync_mutex);
    pool->unfinished_works += num_works;
    pthread_mutex_unlock(&pool->sync_mutex);
    queue_enqueue(&pool->frontend_queue, request);
    // printf("    end in request\n");
}

void tpool_synchronize(struct tpool* pool) {
    pthread_mutex_lock(&pool->sync_mutex);
    while (pool->unfinished_works != 0) {
        // printf("    unfinished works = %d, wait\n", pool->unfinished_works);
        pthread_cond_wait(&pool->sync_cond, &pool->sync_mutex);
    }
    // printf("    all works done\n");
    pthread_mutex_unlock(&pool->sync_mutex);
    // printf("    end in synchronize\n");
}

void tpool_destroy(struct tpool* pool) {
    // printf("    start in destroy\n");
    // Stop frontend thread
    pool->stop = true;
    queue_enqueue(&pool->frontend_queue, NULL);
    
    // pthread_cancel(pool->frontend_thread);
    // for (int i = 0; i < pool->num_threads; i++)
    //     pthread_cancel(pool->threads[i]);

    pthread_join(pool->frontend_thread, NULL);

    // Stop backend threads
    for (int i = 0; i < pool->num_threads; i++) {
        queue_enqueue(&pool->worker_queue, NULL);
    }
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // Clean up resources
    free(pool->threads);
    pthread_mutex_destroy(&pool->sync_mutex);
    pthread_cond_destroy(&pool->sync_cond);
    free(pool);
    // printf("    end in destroy\n");
}
