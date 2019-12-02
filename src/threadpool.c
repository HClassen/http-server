#include <stdlib.h>
#include <pthread.h>

#include "threadpool.h"
#include "connection.h"
#include "http_server.h"

static pthread_mutex_t mutex;
static pthread_cond_t cond;
static pthread_t threads[4];
static unsigned int idle_threads = 0;
static unsigned int nthreads = 0;
static int running = 0;
static queue wq;
static queue exit_sig;

int tpool_running(){
    return running;
}

int tpool_add_work(queue *q){
    pthread_mutex_lock(&mutex);
    QUEUE_ADD_TAIL(&wq, q);
    if(idle_threads > 0){
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutex);
    return HTTPSERVER_OK;
}

/* core function of every thread */
static void *thread_routine(){
    pthread_mutex_lock(&mutex);
    while(1){
        while(QUEUE_EMPTY(&wq)){
            idle_threads += 1;
            pthread_cond_wait(&cond, &mutex);
            idle_threads -= 1;
        }

        queue *q = QUEUE_NEXT(&wq);
        if(q == &exit_sig){
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mutex);
            break;
        }

        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        pthread_mutex_unlock(&mutex);
        connection *c = container_of(q, connection, q);
        c->cfunc(c);

        loop_add_active(c->l, &c->q);
        DEBUG_MSG("worker\n");

        pthread_mutex_lock(&mutex);
    }
    return NULL;
}

int tpool_start(){
    if(nthreads == 0){
        return HTTPSERVER_ETPOOL;
    }
    
    pthread_mutex_lock(&mutex);
    for(unsigned int i = 0; i<nthreads; i++){
        if(pthread_create(&threads[i], NULL, &thread_routine, NULL) != 0){
            DEBUG_MSG("failed to create worker thread nr. %d, exiting\n", i);
            return HTTPSERVER_ETPOOL;
        }
    }
    running = 1;
    pthread_mutex_unlock(&mutex);
    return HTTPSERVER_OK;
}

int tpool_init(){
    nthreads = 4;
    idle_threads = 4;

    if(pthread_mutex_init(&mutex, NULL) != 0){
        DEBUG_MSG("failed to initialize mutex\n");
        return HTTPSERVER_ETPOOL;
    }

    if(pthread_cond_init(&cond, NULL) != 0){
        DEBUG_MSG("failed to initialize condition\n");
        return HTTPSERVER_ETPOOL;
    }

    QUEUE_INIT(&wq);
    QUEUE_INIT(&exit_sig);
    return HTTPSERVER_OK;
}

int tpool_cleanup(){
    QUEUE_ADD_TAIL(&wq, &exit_sig);
    
    /* wait for all threads to finish up */
    pthread_mutex_lock(&mutex);
    while(idle_threads != nthreads){
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    /* join and clean up each thread */
    for(unsigned int i = 0; i<nthreads; i++){
        pthread_join(threads[i], NULL);
    }

    /* clean up mutexes and conditons */
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return HTTPSERVER_OK;
}