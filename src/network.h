#ifndef NETWORK_HEADER
#define NETWORK_HEADER

#include "ssl_module.h"

typedef int (*read_is_done)(char *buf, size_t read, size_t max); /* return 1 if everything is read, 0 else */

/* creates and binds a new socket on port 
 * @param port - char array containing the port number
 * @return socket
 */
int server_socket_create(char *port);

/* set socket to listen for connections 
 * @param socket - the socket
 * @param backlog - how many connections get queued up
 * @return -1 on error
 */
int server_socket_listen(int socket, int backlog);

/* accepts a new connection 
 * @param socket - the socket
 * @return the new socket for the connection or -1 on error
 */
int server_socket_accept(int socket);

/* reads the http header from socket
 * @param socket - the socket
 * @param buf - char pointer to memory where the header gets written to
 * @param buf_size - the size of buf
 * @param len - int pointer where the actual number of written bytes get stored to
 * @return 0 => connection got closed, 1 => all data was read, -1 => error
 */
int server_socket_read(int socket/* , my_ssl *ssl */, char *buf, size_t buf_size, size_t *len, read_is_done func);

/* reads the body from socket
 * @param socket - the socket
 * @param buf - char pointer to memory where the body gets written to
 * @param buf_size - the size of buf
 * @return 0 => connection got closed, 1 => all data was read, -1 => error
 */
int server_socket_read_body(int socket/* my_ssl *ssl */, char *buf, size_t buf_size/* , size_t *len */);

/* sends a response
 * @param socket - the socket
 * @param res - char pointer to memory where the header gets written to
 * @param res_size - the size of buf
 * @return 0 => all data was send, -1 => error
 */
int server_socket_send(int socket/* , my_ssl *ssl */, char *res, size_t res_len);

/* sets socket to non-blocking mode
 * @param socket - the socket
 * @return -1 on error
 */
int set_fd_nonblocking(int fd);

#endif