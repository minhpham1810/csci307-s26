CC = gcc
CFLAGS = -std=gnu17 -pedantic -Wall -g
EXECS = load_tracks load_contacts

VALGRINDFLAGS = -s --leak-check=full --track-origins=yes

all: $(EXECS)

debug: CFLAGS += -DDEBUG -g -Wextra -Werror
debug: $(EXECS)

load_tracks: load_tracks.c
	$(CC) $(CFLAGS) load_tracks.c  -o load_tracks

load_contacts: load_contacts.c
	$(CC) $(CFLAGS) load_contacts.c  -o load_contacts 
	
valgrind_contacts: load_contacts
	valgrind $(VALGRINDFLAGS) ./load_contacts contacts.csv
	
valgrind_tracks: load_tracks
	valgrind $(VALGRINDFLAGS) ./load_tracks tracks.csv
	
.PHONY: clean
clean:
	/bin/rm -rf $(EXECS) *.o *~

