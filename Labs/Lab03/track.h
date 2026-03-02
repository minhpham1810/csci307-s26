#ifndef TRACK_H
#define TRACK_H

#define ID_LEN 22


// Define the Track struct
typedef struct Track {
    char id[ID_LEN+1];
    char *name;
    char *artist;
    long int popularity;
    char album_id[ID_LEN+1];
    float danceability;
    long int duration_ms;
    
} Track;

// To construct a linked list of Track nodes
struct tracklist_node {
  Track * t;
  struct tracklist_node *next;
};

struct tracklist_node * load_csv(const char *filename, const char* sep, long int* num_tracks);
void free_tracklist(struct tracklist_node *head);

#endif // TRACK_H