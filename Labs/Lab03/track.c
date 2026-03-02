# include "track.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE_LENGTH 1024 // Maximum characters in a line from the input file.

// Global buffers for reading and processing file lines.
char line[MAX_LINE_LENGTH];


/**
 * @brief A helper function to safely get the next token from a string.
 *
 * This is a wrapper around `wcstok` for robust tokenizing of strings.
 * It exits on failure, as a missing token indicates a malformed input file.
 *
 * @param str The string to tokenize on the first call, or NULL on subsequent calls.
 * @param sep A string containing the delimiter characters.
 * @param msg An error message to display if tokenizing fails.
 * @return A pointer to the next token.
 */
char* get_token(char** str, const char* sep, char* msg){
    // strsep is similar to strtok but a more modern replacement
    // it is thread safe and also properly handles empty tokens
    char* token = strsep(str, sep);
    if (token == NULL) {
        // If NULL, it means no more tokens could be found.
        // For this program, that implies a malformed line in the CSV file.
        // We print an error and exit to prevent further processing errors.
        fprintf(stderr, "Error parsing line, missing token for: %s\n", msg);
        exit(EXIT_FAILURE);
    }
    return token;
}

Track* parse_track(char* line, const char* sep, Track* track) {
    // the pointer to the line is advanced by strsep
    char **lptr = &line;

    // id is already allocated in the struct, convert it to byte string    
    strncpy(track->id, get_token(lptr, sep, "id"), sizeof(track->id));
    track->id[ID_LEN]= '\0';
    
    track->name = strdup(get_token(lptr, sep, "name"));
    track->artist = strdup(get_token(lptr, sep, "artist"));

    track->popularity = strtol(get_token(lptr, sep, "popularity"), NULL, 10);    

    strncpy(track->album_id, get_token(lptr, sep, "id"), ID_LEN);
    track->album_id[ID_LEN] = '\0';

    track->danceability = strtof(get_token(lptr, sep, "danceability"), NULL);
    track->duration_ms = strtol(get_token(lptr, sep, "duration_ms"), NULL, 10);
    return track;

}

struct tracklist_node * load_csv(const char *filename, const char* sep, long int* num_tracks){    
    FILE *file;
    struct tracklist_node *head = NULL;
    struct tracklist_node *current = NULL;
    
    // Open the file
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return NULL; 
    }
    
    // Skip first line(headers)
    fgets(line, sizeof(line), file);

    *num_tracks = 0;

    // Read the file line by line
    while (fgets(line, sizeof(line), file) != NULL) {
                
        struct tracklist_node *new_node = (struct tracklist_node*)calloc(1, sizeof(struct tracklist_node));
       
        new_node->t = (Track*)calloc(1, sizeof(Track));

        if (new_node == NULL || new_node->t == NULL) {
            // If memory allocation fails, free any allocated memory and return NULL.
            perror("Memory allocation failed\n");
            continue;            
        }

        parse_track(line, sep, new_node->t);
        
        if (head == NULL) {
            head = new_node;
            current = head;
        } else {
            current->next = new_node;
            current = new_node;
        }

        (*num_tracks)++;
    }
    
    printf("Read %ld tracks\n", *num_tracks);    

    // Close the file
    fclose(file);

    return head;
}


void free_tracklist(struct tracklist_node *head) {
    struct tracklist_node *current = head;
    struct tracklist_node *next;

    while (current != NULL) {
        next = current->next;
        
        free(current->t->name);
        free(current->t->artist);
        
        free(current->t);

        free(current);

        // Move to the next node to continue the process.
        current = next;
    }
}