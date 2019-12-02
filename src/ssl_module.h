#ifndef SSL_MODULE_HEADER
#define SSL_MODULE_HEADER

typedef struct ssl_ctx_st my_ssl_ctx; 
typedef struct ssl_st my_ssl;


my_ssl_ctx *ssl_create_context();
void ssl_destroy_context(my_ssl_ctx *ctx);

my_ssl *ssl_create_ssl(my_ssl_ctx *ctx, int socket);
void ssl_destroy_ssl(my_ssl *ssl);
int ssl_read(my_ssl *ssl, void *buf, int num);
int ssl_write(my_ssl *ssl, void *buf, int num);

#endif