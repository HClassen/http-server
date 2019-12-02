#ifndef CONNECTION_HEADER
#define CONNECTION_HEADER

#include "loop.h"
#include "util.h"
#include "queue.h"
#include "http_server_internal.h"
#include "ssl_module.h"

#define MAX_CON 1024

typedef struct connection_ connection;
typedef struct message_ message;
typedef struct client_ client; 
typedef void (*callback)(server *s,  client *c, message *req, message *res);
typedef void (*con_func)(connection *c);

typedef enum method{
    GET,
    PUT,
    POST,
    HEAD,
    OPTIONS,
    MAX,
}req_method;

typedef struct message_{
	char *header;
	char *body;
	char *resource;
	req_method method;
	char *version;
	hash_table props;
	int code;
	size_t header_len;
	size_t body_len;
}message;

typedef enum cstate_{
    active,
    pending,
    idle,
    closing,
    max_states
}cstate;

struct connection_{
    int client;
    int read_count;
    //my_ssl *ssl;
    cstate state;
    timer timer;
    server *s;
    loop *l;
    queue q;
    con_func cfunc;
	message req;
    message res;
};

void default_response(server *s,  client *c, message *req, message *res);
connection *connection_create(int new_fd, server *s);
void connection_destroy(connection **c);

void response_fill(connection *c);
void request_fill(connection *c);
void message_reset(message *m);
void response_load_file(connection *c);

#endif