// =================================================================================
// CSCI307: C Programming and Unix
// Lab: Web Server for Tracks
//
// This program acts as a simple "radio station" server. It reads track data
// from a CSV file, and for each incoming client connection, it sends the
// next track in the playlist, cycling through the list.
//
// This program demonstrates:
//  - File I/O and CSV parsing.
//  - Dynamic memory allocation for a list of tracks.
//  - TCP/IP socket programming for creating a simple web server.
// =================================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <stdint.h>

#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

// Define global constants for readability and maintenance.
#define MAX_LINE_LENGTH 1024 // Maximum characters in a line from the input file.
#define ID_LEN 23            // Length for track/album IDs.

// Global buffers for reading and processing file lines.
char line[MAX_LINE_LENGTH];
wchar_t wline[MAX_LINE_LENGTH];

// Structure to hold track information.
typedef struct Track {
    char id[ID_LEN];
    wchar_t *name;
    wchar_t *artist;
    long int popularity;
    char album_id[ID_LEN];
    float danceability;
    long int duration_ms;
} Track;

/**
 * @brief A helper function to safely get the next token from a wide string.
 *
 * This is a wrapper around `wcstok` for robust tokenizing of wide strings.
 * It exits on failure, as a missing token indicates a malformed input file.
 *
 * @param str The string to tokenize on the first call, or NULL on subsequent calls.
 * @param sep A wide string containing the delimiter characters.
 * @param state A pointer used by wcstok to maintain its state.
 * @param msg An error message to display if tokenizing fails.
 * @return A pointer to the next token.
 */
wchar_t* get_token(wchar_t* str, const wchar_t* sep, wchar_t** state, const char* msg) {
    wchar_t* token = wcstok(str, sep, state);
    if (token == NULL) {
        fwprintf(stderr, L"Error parsing line, missing token for: %s\n", msg);
        exit(EXIT_FAILURE);
    }
    return token;
}

/**
 * @brief Prints a wide-character string followed by the system error message.
 *
 * A wide-character equivalent for perror(). It prints the given wide string `s`,
 * a colon, a space, the string corresponding to the current `errno` value,
 * and a newline, all to stderr.
 *
 * @param s The wide-character string to prepend to the error message.
 */
void wperror(const wchar_t *s) {
    char err_buf[256]; // Buffer for the error string
    strerror_r(errno, err_buf, sizeof(err_buf)); // Thread-safe version of strerror
    fwprintf(stderr, L"%ls: %s\n", s, err_buf);
}

/**
 * @brief Parses a single line of wide-character text into a Track struct.
 *
 * @param line The wide-character string to parse.
 * @param sep The delimiter string.
 * @param track A pointer to a Track struct to store the parsed data.
 */
void parse_track(wchar_t* line, const wchar_t* sep, Track* track) {
    wchar_t *state;

    // The last field in the CSV often has a newline, which we need to remove.
    size_t len = wcslen(line);
    if (len > 0 && line[len - 1] == L'\n') {
        line[len - 1] = L'\0';
    }

    // Parse each token and populate the track struct.
    wcstombs(track->id, get_token(line, sep, &state, "id"), sizeof(track->id));
    track->name = wcsdup(get_token(NULL, sep, &state, "name"));
    track->artist = wcsdup(get_token(NULL, sep, &state, "artist"));
    track->popularity = wcstol(get_token(NULL, sep, &state, "popularity"), NULL, 10);
    wcstombs(track->album_id, get_token(NULL, sep, &state, "album_id"), sizeof(track->album_id));
    track->danceability = wcstof(get_token(NULL, sep, &state, "danceability"), NULL);
    track->duration_ms = wcstol(get_token(NULL, sep, &state, "duration_ms"), NULL, 10);
}

/**
 * @brief Loads track data from a CSV file into a dynamically allocated array.
 *
 * @param filename The path to the CSV file.
 * @param tracks_array A pointer to an array of Track pointers that will be allocated.
 * @return The number of tracks loaded.
 */
int load_tracks_from_csv(const char *filename, Track ***tracks_array) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read and discard the header line.
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    int capacity = 100;
    int count = 0;
    *tracks_array = malloc(capacity * sizeof(Track*));

    while (fgets(line, sizeof(line), file) != NULL) {
        if (mbstowcs(wline, line, MAX_LINE_LENGTH) <= 0) {
            wprintf(L"Skipping invalid string: %s\n", line);
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            *tracks_array = realloc(*tracks_array, capacity * sizeof(Track*));
        }

        (*tracks_array)[count] = malloc(sizeof(Track));
        parse_track(wline, L"|", (*tracks_array)[count]);
        count++;
    }

    fclose(file);
    wprintf(L"Successfully loaded %d tracks from %s.\n", count, filename);
    return count;
}

/**
 * @brief Frees all dynamically allocated memory for the tracks.
 *
 * @param tracks The array of Track pointers.
 * @param num_tracks The number of tracks in the array.
 */
void free_tracks(Track **tracks, int num_tracks) {
    for (int i = 0; i < num_tracks; i++) {
        free(tracks[i]->name);
        free(tracks[i]->artist);
        free(tracks[i]);
    }
    free(tracks);
}

/**
 * @brief Sets up a server to listen for connections and serve tracks.
 *
 * @param tracks The array of tracks to serve.
 * @param num_tracks The total number of tracks.
 * @param port The port number for the server to listen on.
 */
void serve_tracks(Track **tracks, int num_tracks, int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    uint32_t current_track_index = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    wprintf(L"Server listening on port %d\n", port);

    while (1) {
        wprintf(L"Waiting for a connection...\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept failed");
            continue; // Continue to the next iteration to accept new connections
        }

        // Read the browser's HTTP request to determine what is being asked for.
        char request_buffer[MAX_LINE_LENGTH];
        int bytes_received = recv(new_socket, request_buffer, MAX_LINE_LENGTH - 1, 0);
        if (bytes_received > 0) {
            request_buffer[bytes_received] = '\0'; // Null-terminate the request
        } else {
            close(new_socket);
            continue; // Ignore empty or failed requests
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        wprintf(L"Connection accepted from %s:%d\n", client_ip, ntohs(address.sin_port));


        // Check if the browser is requesting the root, and nothing else!
        if (strstr(request_buffer, "GET / HTTP/1.1") == request_buffer) {
            // TODO
            // Track *track_to_send = tracks[current_track_index];
            // send_track(current_track_index, new_socket, track_to_send);            
            close(new_socket);
            current_track_index = (current_track_index + 1) % num_tracks;
        } else {
            wprintf(L"--> Ignoring request\n");
            // terminate at the first newline
            request_buffer [strstr(request_buffer, "\r\n") - request_buffer] = '\0';
            wprintf(L"%s\n", request_buffer);
            const char *response_204 = "HTTP/1.0 204 No Content\r\nConnection: close\r\n\r\n";
            send(new_socket, response_204, strlen(response_204), 0);
            close(new_socket);
            continue; // Skip to the next connection
        }

        Track *track_to_send = tracks[current_track_index];
        wprintf(L"--> Sending track %d: %ls\n", current_track_index + 1, track_to_send->name);

        if (send(new_socket, track_to_send, sizeof(Track), 0) < 0) {
            wperror(L"send failed");
        }

        close(new_socket);
        current_track_index = (current_track_index + 1) % num_tracks;
    }

    close(server_fd);
}

/**
 * @brief The main entry point of the program.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings for command-line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <csv_filename> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int port = atoi(argv[2]);

    Track **tracks = NULL;
    int num_tracks = load_tracks_from_csv(filename, &tracks);

    if (num_tracks > 0) {
        serve_tracks(tracks, num_tracks, port);
        free_tracks(tracks, num_tracks);
    }

    return EXIT_SUCCESS;
}

