#include <stdio.h>
#include <time.h>

#include "http_server.h"

/* void test_callback(server *s, client *c, message *req, message *res){
	char *header[3][2] = {{"Content-Type", "application/json"}, {"X-test", "mult test"}, {"X-more", "test"}};
	res_header(res, header, 3);
	res_body(res, "[test]", 6);
	res_redirect(res, "/");
} */

void file_callback(server *s, client *c, message *req, message *res){
	clock_t t; 
    t = clock();
	res_send_file(res, "resources/R6Rekrut.mp4");
	t = clock() - t; 
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
	printf("time: %lf\n", time_taken);
}

int main(){
	server *server;
    server_create(&server, HTTP, "8080", "Test Server", "resources");
	server_add_route(server, "/file", file_callback);
	server_start(server);
	server_destroy(&server);
	return 0;
}