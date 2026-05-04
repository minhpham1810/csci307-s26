CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
LDFLAGS = -lcrypto

COMMON_OBJS = common.o crypto_utils.o

all: client server

server: server.o db_utils.o record.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: client.o record.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

init_db: init_db.o db_utils.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o:     server.c     common.h crypto_utils.h db_utils.h record.h protocol.h
client.o:     client.c     common.h crypto_utils.h record.h
init_db.o:    init_db.c    common.h db_utils.h
db_utils.o:   db_utils.c   common.h crypto_utils.h db_utils.h
record.o:     record.c     common.h crypto_utils.h record.h
common.o:     common.c     common.h
crypto_utils.o: crypto_utils.c common.h crypto_utils.h

clean:
	rm -f *.o client server init_db client.exe server.exe init_db.exe
