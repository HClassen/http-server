#ifndef HTTP_SERVER_INTERNAL_HEADER
#define HTTP_SERVER_INTERNAL_HEADER

#include <stdio.h>
#include <stddef.h>
#include <pthread.h>

#include "util.h"
#include "queue.h"
#include "ssl_module.h"
#include "threadpool.h"

#ifdef DEBUG
#define DEBUG_MSG(msg, ...) do{fprintf(stdout,"%s:%d:%s:(): " msg, __FILE__, __LINE__, __func__, ##__VA_ARGS__);}while(0)
#else 
#define DEBUG_MSG(msg, ...)
#endif

#define container_of(ptr, type, member) ((type *)  ((char *) ptr - offsetof(type, member)))

typedef struct connection_ connection;

typedef struct server_{
	int backlog;
	int socket;
	int poll_fd;
	char *port;
	char *name;
	char *resource_base_path;
	int type;
	int nfds;
	int max_con;
	connection **conn;
	my_ssl_ctx *ctx;
	hash_table routes;
}server;

#endif