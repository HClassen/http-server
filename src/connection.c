#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#include "header_util.h"
#include "connection.h"
#include "network.h"

#define HEADER_END "\r\n\r\n"

#define GET_CB(c, cb)																\
	do{																				\
		hash_entry *he = hash_lookup(&(c)->s->routes, (c)->req.resource);			\
		(cb) = he != NULL ? (callback) he->value : (callback) (c)->s->routes.data;	\
	}while(0)

static void not_found_response(message *res){
    res->body = strdup("<!DOCTYPE html><html><head><title>404</title><meta charset=\"utf-8\"></head><body>404 requested resource not found</body></html>");
    res->body_len = 127;
    res->code = 404;
    header_set_content_type(&(res->props), html);
}

static int get_header_template(message *req, message *res, server *s){
	char *file = combine(req->resource, s->resource_base_path);
    if(filesize(file, &res->body_len) == FILE_ERROR){
		free(file);
        return -1;
    }
	free(file);
    mime_type type = res_to_mime(req->resource);
    header_set_content_type(&(res->props), type);
    res->code = 200;
    return 0;
}

static void head_response(message *req, message *res, server *s){
    if(get_header_template(req, res, s) == -1){
		not_found_response(res);
    }
}

static void options_response(message *res){
    header_insert(&(res->props), "Allow", "GET, POST, HEAD, OPTIONS");
    res->code = 204;
}

static void get_response(message *req, message *res, server *s){
    if(get_header_template(req, res, s) == -1){
        not_found_response(res);
        return;
    }
	char *file = combine(req->resource, s->resource_base_path);
    res->body = load_file(file, res->body_len);
	free(file);
    if(res->body == NULL){
        not_found_response(res);
        return;
    }
}

void default_response(server *s,  client *c, message *req, message *res){
    res->header = NULL;
    res->body = NULL;
    req_method method = req->method;
    if(method == GET){
        get_response(req, res, s);
    }else if(method == POST){
        //post_response(req, res);
    }else if(method == PUT){
		//put_response(req, res);
	}else if(method == HEAD){
        head_response(req, res, s);
    }else if(method == OPTIONS){
        options_response(res);
    }
}

void message_reset(message *m){
	if(m == NULL){
		return;
	}

	if(m->header != NULL){
		free(m->header);
		m->header = NULL;
	}
	if(m->body != NULL){
		free(m->body);
		m->body = NULL;
	}
	if(m->resource != NULL){
		free(m->resource);
		m->resource = NULL;
	}
	hash_cleanup(&(m->props));
	m->code = 0;
	m->header_len = 0;
	m->body_len = 0;
	//header_cleanup(&(h->props));
}

static void trim(char *to_trim){
	for(unsigned int i = 0; i<strlen(to_trim); i++){
		if(to_trim[i] == ' ' || to_trim[i] == ':'){
			to_trim[i] = 0;
		}
	}
}

static char *find_val(char *line){
	char *val = strchr(line, ':');
	unsigned int i = 0;
	while((val[i] == ':' || val[i] == ' ') && i < strlen(val)){
		i += 1;
	}
	val += i;
	return val;
}

static req_method string_to_method(char *method){
	if(strncmp(method, "GET", 3) == 0){
        return GET;
    }else if(strncmp(method, "PUT", 3) == 0){
		return PUT;
	}else if(strncmp(method, "POST", 4) == 0){
        return POST;
    }else if(strncmp(method, "HEAD", 4) == 0){
        return HEAD;
    }else if(strncmp(method, "OPTIONS", 7) == 0){
        return OPTIONS;
    }
	return MAX;
}

//methode und ressource contrahieren
static void decompose_header(message *req){
	char *prop_ptr = strtok(req->header, "\r\n");
	char *first_line = req->header;

	prop_ptr = strtok(NULL, "\r\n");
	req->props = header_create(16);
	while(prop_ptr != NULL){
		char *val = find_val(prop_ptr);
		*val = tolower(*val);
		char *key = prop_ptr;
		trim(prop_ptr);
		header_insert(&(req->props), key, val);
		prop_ptr = strtok(NULL, "\r\n");
	}

	req->method = string_to_method(strtok(first_line, " "));
	char *tmp_res = strtok(NULL, " ");
	req->resource = strlen(tmp_res) == 1 ? strdup("/index.htm") : strdup(tmp_res);
	req->version = strtok(NULL, " ");
	
	req->body_len = 0;

	if(req->method == POST || req->method == PUT){
		const char *len = NULL;
		int err = header_lookup(&(req->props), "Content-Length", &len);
		if(err == 0 && len != NULL){
			req->body_len = atoi(len);
		}else{
			req->body_len = 1024;
		}
	}
}

static int header_is_done(char *buf, size_t read, size_t max){
	if(read >= max){
		return 1;
	}
	size_t ind = read > 3 ? read - 4 : 0;
	if(strncmp(&buf[ind],HEADER_END, 4) == 0){
		buf[ind] = 0;
		return 1;
	}
	return 0;
}

static int body_is_done(char *buf, size_t read, size_t max){
	(void *) buf;
	if(read >= max){
		return 1;
	}
	return 0;
}

static void connection_remove(connection *c){
	timer_remove(&c->l->timerheap, &c->timer);
	c->state = closing;
	loop_add_closing(c->l, &c->q);
}

void response_send(connection *c){
	header_set_content_length(&c->res.props, c->res.body_len);
	const char *status = http_code_to_str(c->res.code);
	c->res.header_len = header_len(&c->res.props, status);
	char *to_send = malloc((c->res.header_len + c->res.body_len) * sizeof(char));
	if(to_send == NULL){
		DEBUG_MSG("error allocating memory\n");
		connection_remove(c);
		return;
	}

	header_build(&(c->res.props), status, to_send, c->res.header_len);
	memcpy(&to_send[c->res.header_len], c->res.body, c->res.body_len * sizeof(char));

	server_socket_send(c->client, to_send, c->res.header_len + c->res.body_len);
	free(to_send);
	message_reset(&c->req);
	message_reset(&c->res);
	c->cfunc = NULL;
	c->state = idle;
	timer_set(&c->l->timerheap, &c->timer, c->timer.duration);
	loop_add_idle(c->l, &c->q);
}

void response_fill(connection *c){
	int keep_alive = 0;
	if(header_is_set(&c->req.props, "Connection", "keep-alive")){
		keep_alive = 5;
	}

	c->res.props = header_create(8);
	header_set_general(&c->res.props, c->s->name, keep_alive, 0);
	c->timer.duration = keep_alive;
	c->res.code = 200;
	QUEUE_INIT(&c->q);
	callback cb;
	GET_CB(c, cb);
	cb(c->s, NULL, &c->req, &c->res);
    c->cfunc = response_send;
	if(c->state == pending || !QUEUE_EMPTY(&c->q)){
		return;
	}
	loop_add_active(c->l, &c->q);
}

void request_fill(connection *c){
	c->req.header = malloc(1024 * sizeof(char));
	if(c->req.header == NULL){
		DEBUG_MSG("error allocating memory\n");
		connection_remove(c);
		return;
	}

	int err = server_socket_read(c->client/* , c->ssl */, c->req.header, 1024, &c->req.header_len, header_is_done);
	if(err == 0){
		DEBUG_MSG("connection closed\n");
		connection_remove(c);
		return;
	}

	if(err == -1){
		DEBUG_MSG("error recv header\n");
		connection_remove(c);
		return;
	}

	c->req.header[1023] = 0;
	decompose_header(&c->req);

	c->req.body = malloc(c->req.body_len * sizeof(char));
	if(c->req.body == NULL){
		DEBUG_MSG("error allocating memory\n");
		connection_remove(c);
		return;
	}

	err = server_socket_read(c->client/* , c->ssl */, c->req.body, c->req.body_len, &c->req.body_len, body_is_done);;
	if(err == 0){
		DEBUG_MSG("connection closed\n");
		connection_remove(c);
		return;
	}

	if(err == -1){
		DEBUG_MSG("error recv body\n");
		connection_remove(c);
		return;
	}

	// c->req.body[c->req.body_len] = 0;
	c->cfunc = response_fill;
	c->read_count -= 1;
	loop_add_active(c->l, &c->q);
}

static void init_client_connection(connection *c){
	//c->ssl = ssl_create_ssl(c->server->ctx, c->client);
	//c->cfunc = request_fill;
	c->state = idle;
	loop_add_idle(c->l, &c->q);
}

void response_load_file(connection *c){
	DEBUG_MSG("load file\n");
	int err = filesize(c->res.resource, &c->res.body_len);
	if(err != 0){
		return;
	}
	c->res.body = load_file(c->res.resource, c->res.body_len);
	header_set_content_type(&c->res.props, res_to_mime(c->res.resource));
	c->state = active;
	c->cfunc = response_send;
	c->l->pending_size -= 1;
	loop_add_active(c->l, &c->q);
}

connection *connection_create(int new_fd, server *s){
	connection *c = malloc(sizeof(connection));
	if(c == NULL){
		return NULL;
	}
	c->client = new_fd;
	c->read_count = 0;
	c->s = s;
	c->cfunc = init_client_connection;
	c->req = (message) {0};
	c->res = (message) {0};
	return c;
}

void connection_destroy(connection **c){
	message_reset(&(*c)->req);
	message_reset(&(*c)->res);
	//ssl_destroy_ssl((*c)->ssl);
	close((*c)->client);
	free((*c));
	(*c) = NULL;
}