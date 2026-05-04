CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
LDFLAGS = -lcrypto

all: client server crypto_test

server: server.o common.o crypto_utils.o db_utils.o record.o
	$(CC) $(CFLAGS) -o server server.o common.o crypto_utils.o db_utils.o record.o $(LDFLAGS)

client: client.o common.o crypto_utils.o record.o
	$(CC) $(CFLAGS) -o client client.o common.o crypto_utils.o record.o $(LDFLAGS)

crypto_test: crypto_test.o common.o crypto_utils.o
	$(CC) $(CFLAGS) -o crypto_test crypto_test.o common.o crypto_utils.o $(LDFLAGS)

init_db: init_db.o common.o crypto_utils.o db_utils.o
	$(CC) $(CFLAGS) -o init_db init_db.o common.o crypto_utils.o db_utils.o $(LDFLAGS)

server.o: server.c common.h crypto_utils.h db_utils.h record.h protocol.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c common.h crypto_utils.h record.h
	$(CC) $(CFLAGS) -c client.c

db_utils.o: db_utils.c db_utils.h common.h crypto_utils.h
	$(CC) $(CFLAGS) -c db_utils.c

record.o: record.c record.h common.h crypto_utils.h
	$(CC) $(CFLAGS) -c record.c

init_db.o: init_db.c db_utils.h common.h
	$(CC) $(CFLAGS) -c init_db.c

crypto_test.o: crypto_test.c common.h crypto_utils.h
	$(CC) $(CFLAGS) -c crypto_test.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

crypto_utils.o: crypto_utils.c crypto_utils.h common.h
	$(CC) $(CFLAGS) -c crypto_utils.c

clean:
	rm -f *.o client server crypto_test init_db \
	      client.exe server.exe crypto_test.exe init_db.exe
