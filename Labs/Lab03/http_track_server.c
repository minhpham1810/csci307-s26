// =================================================================================
// Simple HTTP Server Example
//
// This program acts as a minimal, single-threaded, iterative HTTP/1.0 server.
//
// This program demonstrates:
//  - TCP/IP socket programming following modern best practices (getaddrinfo).
//  - Basic HTTP/1.0 response formatting.
//  - File I/O for serving a static document.
//  - Comprehensive error checking and resource management.
//
// =================================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#include "http/httputils.h"
#include "track.h"

#define PORT "8080"       // The default port to listen on.
#define BACKLOG 10        // How many pending connections queue will hold.
#define MAX_BUFFER 4*1024   // Max buffer size for receiving requests.

char request_buffer[MAX_BUFFER];

// Global flag to signal server shutdown.
// `volatile` tells the compiler that this variable can be changed by external
// means (like a signal handler) and prevents certain optimizations.
// `sig_atomic_t` ensures that reads/writes to this variable are atomic.
volatile sig_atomic_t g_stop_server = 0;

/**
 * @brief Signal handler for SIGINT (Ctrl+C). Sets the global stop flag.
 */
void handle_sigint(int sig) { g_stop_server = 1; }

void send_track(int new_socket, Track *track_to_send) {            
    // --- HTTP Response Crafting ---
    // We will format the contact data into an HTML string and send it as an HTTP response.
    
    printf("--> Sending track: %s (%22s)\n", track_to_send->name, track_to_send->id);

    // 2. Create the HTML body.
    static char html_body[2048];
    int body_len = snprintf(html_body, sizeof(html_body),
        "<!DOCTYPE html><html><head><title>Contact</title><meta charset=\"UTF-8\"></head>"
        "<body style=\"font-family: sans-serif;\">"
        "<h1>Track Information</h1>"
        "<p><b>id:</b> %22s</p>"
        "<p><b>Name:</b> %s</p>"
        "<p><b>Artist:</b> %s</p>"
        "<p><b>Album id:</b> %22s</p>"
        "<p><b>Popularity:</b> %3ld</p>"
        "<p><b>Danceability:</b> %8.4f</p>"
        "<p><b>Duration (ms):</b> %5ld</p>"
        "</body></html>",
        track_to_send->id, track_to_send->name, track_to_send->artist, track_to_send->album_id, track_to_send->popularity,
        track_to_send->danceability, track_to_send->duration_ms);

    // Check if the body was truncated
    if (body_len >= (int)sizeof(html_body)) {
        fprintf(stderr, "Warning: HTML body truncated\n");
        body_len = sizeof(html_body) - 1;
    }

    // 3. Construct the full HTTP response.
    static char http_response[4096];
    int response_len = snprintf(http_response, sizeof(http_response),
        "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Length: %d\r\nContent-Type: text/html; charset=utf-8\r\n\r\n%s",
        body_len, html_body);
    
    // Check if the response was truncated
    if (response_len >= (int)sizeof(http_response)) {
        fprintf(stderr, "Warning: HTTP response truncated\n");
    }

    // 4. Send the response to the client.
    if (send(new_socket, http_response, strlen(http_response), 0) < 0) {
        perror("send");
    }
}

/**
 * @brief Handles a single client connection.
 *
 * This function reads an HTTP request from a client, and sends back either
 * the contents of the file specified by FILENAME or a 404 Not Found error.
 *
 * @param client_fd The file descriptor for the connected client socket.
 * @return one if the contat was sent.
 */
int handle_connection(int client_fd, struct Track* track) {
    // --- HTTP Request Handling ---
    // read the HTTP request, if it is for the root path, send the current Track.
    
    int bytes_received = recv(client_fd, request_buffer, MAX_BUFFER - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        return -3;
    }
    
    if (bytes_received == 0) {
        // Connection closed by client
        return -4;
    }
            
    request_buffer[bytes_received] = '\0';

    // end the request at the first newline, to print just the request info
    char *crlf = strstr(request_buffer, "\r\n");
    if (crlf != NULL) {
        *crlf = '\0';
    }

    printf("Received Request: %s ", request_buffer);

    if (strstr(request_buffer, "GET / HTTP") == request_buffer) {                
        // --- HTTP Response Crafting ---
        if (track == NULL) {
            printf("--> Sending 404 track!\n");
            // If the file doesn't exist, send a 404 Not Found error.
            const char *response_404 =
                "HTTP/1.0 404 Not Found\r\n"
                "Connection: close\r\n"
                "Content-Type: text/plain\r\n\r\n"
                "404 Not Found: No tracks loaded!";
            if (send(client_fd, response_404, strlen(response_404), 0) == -1) {
                perror("send 404");
                return -2;
            }
        } else {
            // If the track exists, send a 200 OK response.        
            send_track(client_fd, track);
            return 0;
        }
    } else {
        printf("--> Ignoring request!\n");
        const char *response_204 = "HTTP/1.0 204 No Content\r\nConnection: close\r\n\r\n";
        send(client_fd, response_204, strlen(response_204), 0);        
        return -1;
    }

    return -5;
}

/**
 * @brief The main entry point of the program.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings for command-line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <csv_filename> [port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Determine the port to use from command-line arguments or use the default.
    const char *port = (argc > 2) ? argv[2] : PORT;

    // Install signal handler for graceful shutdown.
    // --------------------------------------------------------------------------
    struct sigaction sa = {.sa_handler = handle_sigint};

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Don't restart syscalls like accept()
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    // Load the tracks
    long int num_tracks;
    struct tracklist_node *list = load_csv(argv[1], "|", &num_tracks);
    if (list == NULL) {
        fprintf(stderr, "Failed to load tracks from file.\n");
        return EXIT_FAILURE;
    }

    // Verify the list is not empty and has valid data
    if (num_tracks == 0 || list->t == NULL) {
        fprintf(stderr, "No valid tracks loaded from file.\n");
        free_tracklist(list);
        return EXIT_FAILURE;
    }

    // SETUP: Create a listening socket.
    // --------------------------------------------------------------------------
    int sockfd = setup_listening_socket(port);
    if (sockfd == -1) {
        fprintf(stderr, "server: failed to setup listening socket\n");
        return EXIT_FAILURE;
    }

    // The BACKLOG constant defines the queue size for pending connections.
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server is listening on port %s...\n", port);

    // MAIN LOOP: Accept and handle incoming connections.
    // --------------------------------------------------------------------------
    int new_fd;  // New connection socket
    struct sockaddr_storage their_addr; // Connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    
    struct tracklist_node *current = list;
    while (!g_stop_server) {
        sin_size = sizeof their_addr;
        
        // accept() is a blocking call. It waits for a connection to arrive.
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("server: accept");
            continue; // Log the error and continue to the next connection.
        }

        // Convert the client's IP address to a string for printing.
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("Server: got connection from %s\n", s);

        // Ensure current is valid before accessing it
        if (current == NULL) {
            current = list;
        }

        if (handle_connection(new_fd, current->t) == 0){
            // if we sent a track, advance to the next
            current = current->next;
            if (current == NULL) {
                current = list;
            }
        }
        close(new_fd);
    }
    
    printf("\nSIGINT received, shutting down server...\n");

    free_tracklist(list);
    
    // Close the listening socket.
    close(sockfd);

    return EXIT_SUCCESS;
}
