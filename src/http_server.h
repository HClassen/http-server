#ifndef HTTP_SERVER_HEADER
#define HTTP_SERVER_HEADER

#include <stddef.h>

#define HTTP 1
#define HTTPS  2

#define HTTPSERVER_OK 0
#define HTTPSERVER_EINPUT        -1
#define HTTPSERVER_ENOMEM        -2
#define HTTPSERVER_ETPOOL        -4
#define HTTPSERVER_ESOCK         -8
#define HTTPSERVER_EHASH        -16
#define HTTPSERVER_ERROR        -32
#define HTTPSERVER_EUNSUPPORTED -64

typedef struct server_ server;
typedef struct message_ message;
typedef struct client_ client;
typedef void (*callback)(server *s,  client *c, message *req, message *res);

void server_destroy(server **s);
int server_start(server *s);
int server_create(server **s, int type, char *port, char *name, char *path);
int server_add_route(server *server, char *route, callback c);

void res_code(message *res, int code);
int res_header(message *res, char *array[][2], size_t size);
int res_body(message *res, char *body, size_t size);
int res_redirect(message *res, char *path);
int res_redirect_status(message *res, int status, char *path);
int res_send_file(message *res, char *path);

int req_header_get(message *req, char *key, const char **result);
int req_header_is_set(message *req, char *key, char *val);

#endif