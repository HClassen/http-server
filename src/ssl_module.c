#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ssl_module.h"

static volatile int instances = 0;

static void init_openssl(){
	if(instances == 0){
		SSL_load_error_strings();
		OpenSSL_add_ssl_algorithms();
	}
}

static void cleanup_openssl(){
	if(instances == 0){
		EVP_cleanup();
	}
}

static void configure_context(SSL_CTX *ctx){
	SSL_CTX_set_ecdh_auto(ctx, 1);

	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0){
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0){
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
}

SSL_CTX *ssl_create_context(){
	init_openssl();
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	method = SSLv23_server_method();

	ctx = SSL_CTX_new(method);
	if (!ctx){
		printf("Unable to create SSL context\n");
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
	configure_context(ctx);
	instances += 1;
	return ctx;
}

void ssl_destroy_context(SSL_CTX *ctx){
	SSL_CTX_free(ctx);
	instances -= 1;
	cleanup_openssl();
}
	
SSL *ssl_create_ssl(SSL_CTX *ctx, int socket){
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stdout);
        SSL_free(ssl);
        return NULL;
    }
	printf("ssl established");
    return ssl;
}

void ssl_destroy_ssl(SSL *ssl){
    SSL_free(ssl);
}

int ssl_read(SSL *ssl, void *buf, int num){
    int ret = SSL_read(ssl,buf,num);
	if(ret <= 0){
		int err = SSL_get_error(ssl, ret);
		switch(err){
			case SSL_ERROR_ZERO_RETURN: return 0;
			case SSL_ERROR_WANT_READ: return -1;
			default: return -2;
		}
	}
	return ret;
}

int ssl_write(SSL *ssl, void *buf, int num){
    int ret = SSL_write(ssl, buf, num);
	if(ret <= 0){
		int err = SSL_get_error(ssl, ret);
		switch(err){
			case SSL_ERROR_ZERO_RETURN: return 0;
			case SSL_ERROR_WANT_WRITE: return -1;
			default: return -2;
		}
	}
	return ret;
}