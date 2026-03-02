// =================================================================================
// Gemini Code Assist: Simple HTTP Server Example
//
// This program acts as a minimal, single-threaded, iterative HTTP/1.0 server.
// It serves a static text file to any client that connects.
//
// This program demonstrates:
//  - TCP/IP socket programming following modern best practices (getaddrinfo).
//  - Basic HTTP/1.0 response formatting.
//  - File I/O for serving a static document.
//  - Comprehensive error checking and resource management.
//
// To Compile:
//   gcc -std=gnu17 -Wall -o http_server http_server.c
//
// To Run:
//   1. Create a file named 'hello.txt' in the same directory.
//      echo "Hello, world! This is a static text document." > hello.txt
//   2. Run the server, providing a port number:
//      ./http_server 8080
//   3. Open a web browser and navigate to http://localhost:8080 or use curl:
//      curl http://localhost:8080
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

#define PORT "8080"       // The default port to listen on.
#define BACKLOG 10        // How many pending connections queue will hold.
#define MAX_BUFFER 4*1024   // Max buffer size for receiving requests.
#define FILENAME "hello.txt" // The static file to serve.


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

/**
 * @brief Handles a single client connection.
 *
 * This function reads an HTTP request from a client, and sends back either
 * the contents of the file specified by FILENAME or a 404 Not Found error.
 *
 * @param client_fd The file descriptor for the connected client socket.
 */
void handle_connection(int client_fd) {
    // --- HTTP Request Handling ---
    // For this simple server, we'll just read the request to clear the buffer
    // but we won't parse it. A real server would parse the method (GET, POST),
    // the path (/index.html), and headers.
    

    int bytes_received = recv(client_fd, request_buffer, MAX_BUFFER - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        return;
    }
            
    request_buffer[bytes_received] = '\0';
    printf("Received Request:\n---\n%s---\n", request_buffer);    

    // --- HTTP Response Crafting ---
    // An HTTP response consists of:
    // 1. A status line (e.g., "HTTP/1.0 200 OK")
    // 2. Headers (e.g., "Content-Type: text/plain")
    // 3. A blank line (\r\n)
    // 4. The response body (the file content)

    // Try to open the static file.
    FILE *file = fopen(FILENAME, "r");
    if (file == NULL) {
        // If the file doesn't exist, send a 404 Not Found error.
        const char *response_404 =
            "HTTP/1.0 404 Not Found\r\n"
            "Connection: close\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "404 Not Found: The requested file was not found on this server.";
        if (send(client_fd, response_404, strlen(response_404), 0) == -1) {
            perror("send 404");
            close(client_fd);
            return;
        }
    } else {
        // If the file exists, send a 200 OK response.
        printf("Server: Sending '%s'\n", FILENAME);

        // Get the file size to set the Content-Length header.
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Allocate memory for the file content.
        char *file_content = malloc(file_size + 1);
        if (file_content == NULL) {
            perror("malloc for file content");
        } else {
            fread(file_content, 1, file_size, file);
            file_content[file_size] = '\0';

            // Allocate memory for the full HTTP response.
            char *http_response = malloc(512 + file_size);
            if (http_response != NULL) {
                // Construct the HTTP response headers and body.
                sprintf(http_response,
                        "HTTP/1.0 200 OK\r\n"
                        "Connection: close\r\n"
                        "Content-Length: %ld\r\n"
                        "Content-Type: text/plain; charset=utf-8\r\n\r\n"
                        "%s",
                        file_size, file_content);

                // Send the complete response.
                if (send(client_fd, http_response, strlen(http_response), 0) == -1) {
                    perror("send 200");
                }
                free(http_response);
            } else {
                perror("malloc for response");
            }
            free(file_content);
        }
        fclose(file);
    }

    // The connection is handled, so we close the new socket descriptor.
    close(client_fd);
    printf("Server: Connection closed.\n\n");
}

/**
 * @brief The main entry point of the program.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings for command-line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char *argv[]) {
    int sockfd, new_fd;  // Listen on sockfd, new connection on new_fd
    struct sockaddr_storage their_addr; // Connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    struct sigaction sa;

    // Determine the port to use from command-line arguments or use the default.
    const char *port = (argc > 1) ? argv[1] : PORT;

    // 1. SETUP: Install signal handler for graceful shutdown.
    // --------------------------------------------------------------------------
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Don't restart syscalls like accept()
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    // 2. SETUP: Create a listening socket.
    // --------------------------------------------------------------------------
    sockfd = setup_listening_socket(port);
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

    // 3. MAIN LOOP: Accept and handle incoming connections.
    // --------------------------------------------------------------------------
    while (!g_stop_server) {
        sin_size = sizeof their_addr;

        // accept() is a blocking call. It waits for a connection to arrive,
        // then creates a *new* socket descriptor for that specific connection.
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            // If accept() was interrupted by our signal handler, g_stop_server will be set.
            if (errno == EINTR) {
                break; // Exit the loop to shut down.
            }
            perror("server: accept");
            continue; // Log the error and continue to the next connection.
        }

        // Convert the client's IP address to a string for printing.
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("Server: got connection from %s\n", s);
        handle_connection(new_fd);
    }

    printf("\nSIGINT received, shutting down server...\n");
    close(sockfd);

    return EXIT_SUCCESS;
}
