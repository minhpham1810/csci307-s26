// =================================================================================
// CSCI307: Computer Networks & Securty
// Lab 01: Review C - Structures and Memory Management
// Starter code (c) 2025 Marchiori
//
// NAME: 
// DATE:
//
// =================================================================================
// The #include directive is used to import functionality from C's standard
// libraries.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
#define ID_LEN 22

// Define the Track struct
typedef struct Track {
    char id[ID_LEN + 1];        // store the null terminator as well!
    char *name;
    char *artist;
    long int popularity;
    char album_id[ID_LEN + 1];  // store the null terminator as well!
    float danceability;
    long int duration_ms;
} Track;

// To construct a linked list of Track nodes
struct tracklist_node {
  Track * t;
  struct tracklist_node *next;
};

char* get_token(char** str, char* sep, char* msg){
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

/**
 * @brief Parses a single line of wide-character text into a Track struct.
 *
 * @param line The character string to parse.
 * @param sep The delimiter string (e.g., "|").
 * @param track A pointer to a Track struct where the parsed data will be stored.
 * @return A pointer to the populated Track struct.
 */
Track* parse_track(char* line, char* sep, Track* track) {
    // TODO
    return track;
}

/**
 * @brief Loads a CSV file and creates a linked list of Track structs.
 *
 * @param filename The path to the CSV file.
 * @param sep The delimiter string (e.g., "|").
 * @param num_tracks A pointer to a long integer that will store the number of tracks loaded.
 * @return The head of a linked list of Track structs, or NULL if an error occurs.
 */
struct tracklist_node * load_csv(const char *filename, char* sep, long int* num_tracks){    
    // TODO, read the file and create track structures
    // while (gets) { parse_track() ... }

    // Return the head of a linked list of tracks (struct tracklist_node)
    return NULL;
}

/**
 * @brief Prints the entire list of tracks in a formatted table.
 *
 * @param list A pointer to the head of the track list.
 * @param num_tracks The maximum number of tracks to print.
 */
void print_tracklist(struct tracklist_node *list, long int num_tracks) {
    printf("%-22s | %-30s | %-20s | %-22s | %-3s | %-8s | %-5s\n", "Id", "Name", "Artist", "Album", "Pop", "Dance", "Dur");
    printf("-----------------------|--------------------------------|----------------------|------------------------|-----|----------|-------\n");

    while((list != NULL) && (num_tracks-- > 0)){
        printf("%22s | %30.30s | %20.20s | %22s | %3ld | %8.4f | %5ld\n", 
            list->t->id,

            list->t->name,
            list->t->artist,

            list->t->album_id,

            list->t->popularity,
            list->t->danceability,
            list->t->duration_ms);
        list = list->next;
    }
}

/**
 * @brief Frees all dynamically allocated memory for the track list.
 *
 * This is a critical function to prevent memory leaks. A memory leak occurs
 * when a program allocates memory on the heap but loses all pointers to it,
 * making it impossible to free. For every `malloc`, `calloc`, or `wcsdup`,
 * there must be a corresponding `free`.
 *
 * @param head A pointer to the head of the list to be freed.
 */
void free_tracklist(struct tracklist_node *head) {
    // TODO
}

/**
 * @brief Sorts the track list based on a given comparison function.
 * @param list A pointer to the head of the list.
 * @param num_songs The number of songs in the list.
 * @param compare A pointer to the comparison function. 
 * @returns A pointer to the head of the sorted list.
 */
struct tracklist_node * sort_tracklist(
    struct tracklist_node *list,
    long int num_songs,
    int (*compare)(const void *, const void *)){

    return NULL;
}

/**
 * @brief The main entry point of the program.
 *
 * @param argc The number of command-line arguments passed to the program.
 * @param argv An array of strings, where each string is a command-line argument.
 *             argv[0] is always the name of the program itself.
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE on error.
 */
int main(int argc, char** argv) {
    // check CLI args
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }
    long int num_tracks = 0;    
    struct tracklist_node * list = load_csv(argv[1], "|", &num_tracks);

    // TODO print the tracklist sorted various ways.    
    printf("Top 5 tracks by popularity\n");
    //print_tracklist(sort_tracklist(list, num_tracks, compare_track_popularity), 5);
    printf("\n");
    
    printf("Top 5 tracks by duration\n");
    //print_tracklist(sort_tracklist(list, num_tracks, compare_track_duration), 5);
    printf("\n");

    printf("Top 5 tracks by danceability then popularity\n");
    //print_tracklist(sort_tracklist(list, num_tracks, compare_track_danceability_then_popularity), 5);
    printf("\n");    

    free_tracklist(list);

    return EXIT_SUCCESS;
}
