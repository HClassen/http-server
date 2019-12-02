#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <time.h>

#include "network.h"
#include "util.h"
#include "connection.h"
#include "ssl_module.h"
#include "threadpool.h"
#include "http_server_internal.h"
#include "http_server.h"
#include "header_util.h"
#include "loop.h"

static int new_connection(server *s, loop *l, int new_fd){
	if (s->max_con <= s->nfds - 1){
		return -1;
	}

	s->nfds += 1;
	connection *c = connection_create(new_fd, s);
	if(c == NULL){
		return -1;
	} 

	QUEUE_INIT(&c->q);
	QUEUE_ADD_TAIL(&l->active, &c->q);
	l->active_size += 1;
	timer_init(&l->timerheap, &c->timer, 5);
	c->state = active;
	c->l = l;
	s->conn[new_fd] = c;
	return 0;
}

//epoll stuff
static int epoll_create_socket(){
	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0){
		return -1;
	}

	return epoll_fd;
}

static int epoll_add_socket(int epoll_fd, int new_fd){
	struct epoll_event new_ev = (struct epoll_event) {0};
	new_ev.events = EPOLLIN | EPOLLET;
	new_ev.data.fd = new_fd;

	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &new_ev)){
		return -1;
	}
	return 0;
}

static void poll_sockets(server *server, loop *l, time_t timeout){
	struct epoll_event events[server->nfds];
	int rfds = epoll_wait(server->poll_fd, events, server->nfds, timeout);
	for (int i = 0; i < rfds; i++){
		int fd = events[i].data.fd;
		if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))){
			DEBUG_MSG("event error\n");
			if(fd == server->socket){
				server_destroy(&server);
				exit(1);
			}
			close(fd);
			continue;
		}

		if(fd == server->socket){
			int new_fd = -1;
			while((new_fd = server_socket_accept(fd)) > -1){
				if (new_fd == -1){
					DEBUG_MSG("failed to accept\n");
					continue;
				}

				if(set_fd_nonblocking(new_fd) == -1){
					DEBUG_MSG("failed to set socket nonblocking\n");
					close(new_fd);
					continue;
				}

				if(epoll_add_socket(server->poll_fd, new_fd) == -1){
					DEBUG_MSG("failed to add socket to epoll\n");
					close(new_fd);
					continue;
				}

				if(new_connection(server, l, new_fd) == -1){
					DEBUG_MSG("max connections reached\n");
					close(new_fd);
					continue;
				}
			}			
		}else{
			connection *c = server->conn[fd];
			if(c->state != closing){
				c->read_count += 1;
			}

			if(c->state == idle){
				l->idle_size -= 1;
				QUEUE_REMOVE(&c->q);
				loop_add_active(l, &c->q);
				c->cfunc = request_fill;
			}			
		}
	}
}

int server_start(server *s){
	if(s == NULL){
		DEBUG_MSG("NULL input\n");
		return HTTPSERVER_EINPUT;
	}
	s->socket = server_socket_create(s->port);
	if (s->socket == -1){
		DEBUG_MSG("failed creating server socket\n");
		return HTTPSERVER_ESOCK;
	}
	if(set_fd_nonblocking(s->socket) == -1){
		DEBUG_MSG("failed to set unblocking\n");
		return HTTPSERVER_ESOCK;
	}
	s->poll_fd = epoll_create_socket();
	if (s->poll_fd == -1){
		DEBUG_MSG("failed creating poll socket\n");
		return HTTPSERVER_ESOCK;
	}
	if(epoll_add_socket(s->poll_fd, s->socket) == -1){
		DEBUG_MSG("failed adding socket\n");
		return HTTPSERVER_ESOCK;
	}
	s->nfds = 1;
	s->conn = calloc(s->max_con, sizeof(connection *));
	if(s->conn == NULL){
		DEBUG_MSG("failed allocating memory\n");
		return HTTPSERVER_ENOMEM;
	}

	loop l;
	int err = loop_create(&l);
	if(err != HTTPSERVER_OK){
		return err;
	}

	if (server_socket_listen(s->socket, s->backlog) == -1){
		DEBUG_MSG("failed to listen\n");
		return HTTPSERVER_ESOCK;
	}

	printf("started server on port %s\n", s->port);
	while (1){
		loop_walk_active(&l);
		l.now = time(NULL);
		time_t timeout = loop_get_timeout(&l);
		poll_sockets(s, &l, timeout);
		//walk_idle(s, &l);
		loop_walk_closing(&l);
	}

	loop_destroy(&l);
}

int server_create(server **s, int type, char *port, char *name, char *path){
	if(s == NULL || port == NULL || name == NULL || path == NULL){
		return HTTPSERVER_EINPUT;
	}

	(*s) = malloc(sizeof(server));
	if(s == NULL){
		(*s) = NULL;
		DEBUG_MSG("failed allocating memory\n");
		return HTTPSERVER_ENOMEM;
	}

	(*s)->socket = -1;
	(*s)->poll_fd = -1;
	(*s)->backlog = 50;
	(*s)->port = port;
	(*s)->name = name;
	(*s)->resource_base_path = path;
	(*s)->type = type;
	(*s)->max_con = MAX_CON;
	(*s)->conn = NULL;
	(*s)->ctx = NULL;
	if (type == HTTPS){
		(*s)->ctx = ssl_create_context();
	}
	if(hash_init(&(*s)->routes, 16, default_response) != HASH_OK){
		DEBUG_MSG("failed initalising route table\n");
		return HTTPSERVER_EHASH;
	}
	return HTTPSERVER_OK;
}

void server_destroy(server **s){
	if((*s)->socket > -1){
		close((*s)->socket);
	}
	if((*s)->poll_fd > -1){
		close((*s)->poll_fd);
	}
	ssl_destroy_context((*s)->ctx);
	hash_cleanup(&((*s)->routes));
	if((*s)->conn != NULL){
		for(int i = 0; i<(*s)->max_con; i++){
			if((*s)->conn[i] != NULL){
				connection_destroy(&(*s)->conn[i]);
			}
		}
	}
	free((*s)->conn);
	free((*s));
	(*s) = NULL;
}

int server_add_route(server *server, char *route, callback c){
	if(server == NULL || route == NULL || c == NULL){
		DEBUG_MSG("NULL input\n");
		return HTTPSERVER_EINPUT;
	}

	if (hash_insert(&(server->routes), strdup(route), (void *) c, HASH_FREE_KEY) != HASH_OK){
		DEBUG_MSG("route error\n");
		return HTTPSERVER_EHASH;
	}
	return HTTPSERVER_OK;
}

void res_code(message *res, int code){
	res->code = code;
}

int res_header(message *res, char *array[][2], size_t size){
	return header_insert_mult(&res->props, array, size);
}

int res_body(message *res, char *body, size_t size){
	res->body = strdup(body);
	res->body_len = size;
	return HTTPSERVER_OK;
}

int res_redirect(message *res, char *path){
	res->code = 303;
	header_insert(&res->props, "Location", path);
	return HTTPSERVER_OK;
}

int res_redirect_status(message *res, int status, char *path){
	if(status < 300 || status > 308){
		return HTTPSERVER_EINPUT;
	}

	if(status == 302){
		return HTTPSERVER_EUNSUPPORTED;
	}

	res->code = status;
	return header_insert(&res->props, "Location", path);
}

int res_send_file(message *res, char *path){
	res->resource = strdup(path);
	connection *c = container_of(res, connection, res);
	return loop_load_file(c->l, &c->q);
}

int req_header_get(message *req, char *key, const char **result){
	return header_lookup(&req->props, key, result);
}

int req_header_is_set(message *req, char *key, char *val){
	return header_is_set(&req->props, key, val);
}