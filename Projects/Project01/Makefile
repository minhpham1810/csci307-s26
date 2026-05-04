# Compiles secure_server.c and secure_client.c into server and client executables

CC = gcc
CFLAGS = -Wall -Wextra -pedantic

all: server client

server: secure_server.c protocol.h
	$(CC) $(CFLAGS) -o server secure_server.c

client: secure_client.c protocol.h
	$(CC) $(CFLAGS) -o client secure_client.c

clean:
	rm -f server client
