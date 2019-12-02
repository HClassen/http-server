#include <stdlib.h>

#include "loop.h"
#include "http_server_internal.h"
#include "http_server.h"
#include "connection.h"

int loop_create(loop *l){
    if(l == NULL){
        return HTTPSERVER_EINPUT;
    }

    l->active_size = l->idle_size = l->closing_size = l->pending_size = 0;
	QUEUE_INIT(&l->active);
	QUEUE_INIT(&l->idle);
	QUEUE_INIT(&l->closing);
	heap_init(&l->timerheap, timer_cmp);

    if(pthread_mutex_init(&l->mutex, NULL) != 0){
        DEBUG_MSG("failed to init loop mutex\n");
		return HTTPSERVER_ERROR;
    }

	if(tpool_init() != HTTPSERVER_OK){
		DEBUG_MSG("failed to init threadpool\n");
		return HTTPSERVER_ETPOOL;
	}

	if(tpool_start() != HTTPSERVER_OK){
		tpool_cleanup();
		DEBUG_MSG("failed to start threadpool\n");
		return HTTPSERVER_ETPOOL;
	}

    return HTTPSERVER_OK;
}

int loop_destroy(loop *l){
    if(l == NULL){
        return HTTPSERVER_EINPUT;
    }

    tpool_cleanup();
    pthread_mutex_destroy(&l->mutex);
    return HTTPSERVER_OK;
}

int loop_add_active(loop *l, queue *q){
    if(l == NULL || q == NULL){
        return HTTPSERVER_EINPUT;
    }

    // DEBUG_MSG("add active\n");
    QUEUE_INIT(q);
    pthread_mutex_lock(&l->mutex);
    l->active_size += 1;
    QUEUE_ADD_TAIL(&l->active, q);
    pthread_mutex_unlock(&l->mutex);
    return HTTPSERVER_OK;
}

int loop_add_idle(loop *l, queue *q){
    if(l == NULL || q == NULL){
        return HTTPSERVER_EINPUT;
    }

    // DEBUG_MSG("add idle\n");
    QUEUE_INIT(q);
    l->idle_size += 1;
    QUEUE_ADD_TAIL(&l->idle, q);
    return HTTPSERVER_OK;
}

int loop_add_closing(loop *l, queue *q){
    if(l == NULL || q == NULL){
        return HTTPSERVER_EINPUT;
    }

    // DEBUG_MSG("add closing\n");
    QUEUE_INIT(q);
    l->closing_size += 1;
    QUEUE_ADD_TAIL(&l->closing, q);
    return HTTPSERVER_OK;
}

int loop_walk_active(loop *l){
    // DEBUG_MSG("walk active\n");
    queue tmp, *q;
	pthread_mutex_lock(&l->mutex);
    l->active_size = 0;
	QUEUE_MOVE(&l->active, &tmp);
	pthread_mutex_unlock(&l->mutex);
	while(!QUEUE_EMPTY(&tmp)){
		q = QUEUE_NEXT(&tmp);
		QUEUE_REMOVE(q);
		connection *c = container_of(q, connection, q);
		c->cfunc(c);
	}
    return HTTPSERVER_OK;
}

int loop_walk_closing(loop *l){
    // DEBUG_MSG("walk closing\n");
	queue tmp, *q;
	QUEUE_MOVE(&l->closing, &tmp);
	while(!QUEUE_EMPTY(&tmp)){
		q = QUEUE_NEXT(&tmp);
		QUEUE_REMOVE(q);
		connection *c = container_of(q, connection, q);
		l->closing_size -= 1;
		connection_destroy(&c->s->conn[c->client]);
	}
    return HTTPSERVER_OK;
}

int loop_get_timeout(loop *l){
    // DEBUG_MSG("timeout\n");
	if(l->active_size > 0 || l->closing_size > 0 || l->pending_size > 0){
		return 0;
	}
	
	if(l->idle_size > 0){
		heap_node *n = heap_peek_head(&l->timerheap);
		timer *t = NULL;
		while(n != NULL && timer_check(n, l->now)){
			t = container_of(n, timer, node);
			connection *c = container_of(t, connection, timer);
			QUEUE_REMOVE(&c->q);
			QUEUE_ADD_TAIL(&l->closing, &c->q);
			l->idle_size -= 1;
			l->closing_size += 1;
			c->state = closing;
			heap_dequeue_head(&l->timerheap);
			n = heap_peek_head(&l->timerheap);
		}
		if(n == NULL){
			return -1;
		}
		t = container_of(n, timer, node);
		return l->now - t->end;
	}
	return -1;
}

int loop_load_file(loop *l, queue *q){
    if(l == NULL || q == NULL){
		return HTTPSERVER_EINPUT;
	}

    connection *c = container_of(q, connection, q);
    c->state = pending;
    l->pending_size += 1;
    if(tpool_running()){
		DEBUG_MSG("thread\n");
        tpool_add_work(&c->q);
    }else{
        response_load_file(c);
    }
    return HTTPSERVER_OK;
}