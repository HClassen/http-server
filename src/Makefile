CC= gcc
CFLAGS= -W -Wextra -Wall -std=c99
POSIX= -D_POSIX_C_SOURCE=200809L#-D_POSIX_C_SOURCE=200112L
SSL= -lssl -lcrypto
PT= -lpthread
DEBUG= -DDEBUG
FILES= main.c http_server.c network.c header_util.c util.c ssl_module.c connection.c loop.c threadpool.c

debug: $(FILES)
	$(CC) $(CFLAGS) -o hs $(FILES) $(POSIX) $(DEBUG) $(SSL) $(PT) -g

server: $(FILES)
	$(CC) $(CFLAGS) -o hs $(FILES) $(POSIX) $(SSL) $(PT)

clean:
	rm hs