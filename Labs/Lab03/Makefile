CC = gcc
CFLAGS = -std=gnu17 -pedantic -Wall
EXECS = showip scanip connectip listen_ports http_server http_client serve_tracks http_contact_server http_track_server

VALGRINDFLAGS = -s --leak-check=full --track-origins=yes

# If the DEBUG variable is set (e.g., `make DEBUG=1`), add debug flags.
ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG -g -Wextra -Werror
endif

all: $(EXECS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
http_server: http_server.c http/httputils.o
	$(CC) $(CFLAGS) http_server.c http/httputils.o -o http_server

http_client: http_client.c http/httputils.o
	$(CC) $(CFLAGS) http_client.c http/httputils.o -o http_client

http_contact_server: http_contact_server.c http/httputils.o contact.o
	$(CC) $(CFLAGS) http_contact_server.c http/httputils.o contact.o -o http_contact_server

http_track_server: http_track_server.c http/httputils.o track.o
	$(CC) $(CFLAGS) http_track_server.c http/httputils.o track.o -o http_track_server

serve_contacts: serve_contacts.c
	$(CC) $(CFLAGS) serve_contacts.c  -o serve_contacts

serve_tracks: serve_tracks.c
	$(CC) $(CFLAGS) serve_tracks.c  -o serve_tracks

.PHONY: clean
clean:
	/bin/rm -rf $(EXECS) http/*.o *.o *~