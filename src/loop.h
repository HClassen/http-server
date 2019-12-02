#ifndef LOOP_HEADER
#define LOOP_HEADER

#include <stddef.h>
#include <pthread.h>

#include "util.h"
#include "queue.h"

typedef struct loop_{
	queue active;
	queue idle;
	queue closing;
	heap timerheap;
	time_t now;
	size_t active_size;
	size_t pending_size;
	size_t idle_size;
	size_t closing_size;
	pthread_mutex_t mutex;
}loop;

int loop_create(loop *l);
int loop_destroy(loop *l);

int loop_add_active(loop *l, queue *q);
int loop_add_idle(loop *l, queue *q);
int loop_add_closing(loop *l, queue *q);

int loop_walk_active(loop *l);
int loop_walk_closing(loop *l);
int loop_get_timeout(loop *l);

int loop_load_file(loop *l, queue *q);

#endif