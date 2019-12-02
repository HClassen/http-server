#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include "network.h"
#include "util.h"
typedef struct _client{
    int socket;
    my_ssl *ssl;
	struct sockaddr addr;
}client;


int set_fd_nonblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1){
        return -1;
    }

    flags |= O_NONBLOCK;
    flags = fcntl(fd, F_SETFL, flags);
    if(flags == -1){
        return -1;
    } 
    return 0;
}

int server_socket_create(char *port){
	struct addrinfo hints, *res, *p;
	int yes = 1, status;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((status = getaddrinfo(NULL,port,&hints,&res)) != 0){
		fprintf(stderr,"getaddrinfo: %s, %d\n", gai_strerror(status), status);
		return -1;
	}

	int sock;
	for(p = res; p != NULL; p = p->ai_next){
		
		if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			continue;
		}
		
		if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) == -1){
			exit(1);
		}
	
		if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
			close(sock);
			continue;
		}
		
		break;
	}

	if(p == NULL){
		sock = -1;
	}

	freeaddrinfo(res);
	return sock;
}

int server_socket_listen(int socket, int backlog){
	return listen(socket, backlog);
}

//Convert a struct sockaddr address to a string, IPv4 and IPv6:
static char *get_ip_str(const struct sockaddr *sa){
	char *s = NULL;
	size_t maxlen;
    switch(sa->sa_family) {
        case AF_INET:
			maxlen = INET_ADDRSTRLEN;
			s = calloc(sizeof(char), maxlen);
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),s, maxlen);
            break;

        case AF_INET6:
			maxlen = INET6_ADDRSTRLEN;
			s = calloc(sizeof(char), maxlen);
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),s, maxlen);
            break;

		default:
			maxlen = 8;
			s = calloc(sizeof(char), maxlen);
			snprintf(s, maxlen, "%s", "Unknown");
			break;
    }

    return s;
}

int server_socket_accept(int socket){
	struct sockaddr_storage addrStorage;
	socklen_t addrSize = sizeof(addrStorage);
	int client = accept(socket, (struct sockaddr *) &addrStorage, &addrSize);

	if(client == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
		return -2;
	}

	if(client != -1){
		char *ip = get_ip_str((struct sockaddr *) &addrStorage);
		printf("accepted new connection from [%s]\n", ip);
		free(ip);
	}

	return client;
}

static int socket_read(int socket/* , my_ssl *ssl */, char *buf, int s){
	int recbytes = 0;
	/* if(ssl == NULL){
		recbytes = recv(socket,&buf[c],1,0);
		if(recbytes == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				recbytes = -1;
			}
			recbytes = -2;
		}
	}else{
		recbytes = ssl_read(ssl, &buf[c], 1);
	} */

	recbytes = recv(socket,buf,s,0);
	if(recbytes == -1){
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			recbytes = -1;
		}else{
			recbytes = -2;
		}
	}
	return recbytes;
}

//Einlesen des Headers
int server_socket_read(int socket/* , my_ssl *ssl */, char *buf, size_t buf_size, size_t *len, read_is_done func){
	if(buf == NULL || buf_size == 0 || len == NULL){
		*len = 0;
		return 1;
	}

	size_t c = 0;
	int recbytes = 0;
	buf[c] = 0;
	while(!func(buf, c, buf_size)){
		recbytes = socket_read(socket, &buf[c], 1);
		if(recbytes == 0){
			*len = c;
			return 0;
		}else if(recbytes == -1){
			*len = c;
			return 1;
		}else if(recbytes == -2){
			*len = 0;
			return -1;
		}
		c += recbytes;
		buf[c] = 0;
	}
	*len = c;
	return 1;
}

int server_socket_send(int socket/* , my_ssl *ssl */, char *res, size_t res_len){
	if(res == NULL || res_len <= 0){
		return -1;
	}

	int sendbytes = 0;
	/* if(ssl == NULL){
		sendbytes = send(socket, res, res_len, 0);
	}else{
		sendbytes = ssl_write(ssl, res, res_len);
	} */

	sendbytes = send(socket, res, res_len, 0);
	
	if(sendbytes == -1){
		return -1;
	}
	return 0;
}