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
#include "contact.h"

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

void send_contact(int new_socket, Contact *contact_to_send) {
    // --- HTTP Response Crafting ---
    // We will format the contact data into an HTML string and send it as an HTTP response.

    printf("--> Sending contact: %s %s\n", 
        contact_to_send->first_name, contact_to_send->last_name);

    // 2. Create the HTML body.
    static char html_body[2048];
    int body_len = snprintf(html_body, sizeof(html_body),
        "<!DOCTYPE html><html><head><title>Contact</title><meta charset=\"UTF-8\"></head>"
        "<body style=\"font-family: sans-serif;\">"
        "<h1>Contact Information</h1>"
        "<p><b>Name:</b> %s %s</p>"
        "<p><b>Email:</b> %s</p>"
        "<p><b>Phone:</b> %s</p>"
        "<p><b>Location:</b> %s, %s</p>"
        "</body></html>",
        contact_to_send->first_name, contact_to_send->last_name, 
        contact_to_send->email, contact_to_send->phone, 
        contact_to_send->city, contact_to_send->country);

    // 3. Construct the full HTTP response.
    static char http_response[4096];
    snprintf(http_response, sizeof(http_response),
        "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Length: %d\r\nContent-Type: text/html; charset=utf-8\r\n\r\n%s",
        body_len, html_body);

    // 4. Send the response to the browser.
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
int handle_connection(int client_fd, struct Contact* contact) {
    // --- HTTP Request Handling ---
    // For this simple server, we'll just read the request to clear the buffer
    // but we won't parse it. A real server would parse the method (GET, POST),
    // the path (/index.html), and headers.
    
    int bytes_received = recv(client_fd, request_buffer, MAX_BUFFER - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        return -3;
    }
            
    request_buffer[bytes_received] = '\0';

    // end the request at the first newline, to print just the request info
    request_buffer[ strstr(request_buffer, "\r\n") - request_buffer] = '\0';

    printf("Received Request: %s ", request_buffer);

    if (strstr(request_buffer, "GET / HTTP") == request_buffer) {                
        // --- HTTP Response Crafting ---
        // An HTTP response consists of:
        // 1. A status line (e.g., "HTTP/1.0 200 OK")
        // 2. Headers (e.g., "Content-Type: text/plain")
        // 3. A blank line (\r\n)
        // 4. The response body (the file content)
        if (contact == NULL) {
            printf("--> Sending 404 contact!\n");
            // If the file doesn't exist, send a 404 Not Found error.
            const char *response_404 =
                "HTTP/1.0 404 Not Found\r\n"
                "Connection: close\r\n"
                "Content-Type: text/plain\r\n\r\n"
                "404 Not Found: No contacts loaded!";
            if (send(client_fd, response_404, strlen(response_404), 0) == -1) {
                perror("send 404");
                close(client_fd);
                return -2;
            }
        } else {
            // If the contact exists, send a 200 OK response.        
            send_contact(client_fd, contact);
            // The connection is handled, so we close the new socket descriptor.
            close(client_fd);    
            return 0;
        }
    } else {
        printf("--> Ignoring request!\n");
        const char *response_204 = "HTTP/1.0 204 No Content\r\nConnection: close\r\n\r\n";
        send(client_fd, response_204, strlen(response_204), 0);        
        close(client_fd);    
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
    int sockfd, new_fd;  // Listen on sockfd, new connection on new_fd
    struct sockaddr_storage their_addr; // Connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];    

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <csv_filename> [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
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

    // load the contacts
    int num_contacts;
    struct contactlist_node *list = load_csv(filename, "|", &num_contacts);
    if (list == NULL) {
        fprintf(stderr, "Failed to load contacts from file.\n");
        return EXIT_FAILURE;
    }

    // SETUP: Create a listening socket.
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

    // MAIN LOOP: Accept and handle incoming connections.
    // --------------------------------------------------------------------------
    struct contactlist_node *current = list;
    while (!g_stop_server) {
        sin_size = sizeof their_addr;
        
        // accept() is a blocking call. It waits for a connection to arrive,
        // then creates a *new* socket descriptor for that specific connection.
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("server: accept");
            continue; // Log the error and continue to the next connection.
        }

        // Convert the client's IP address to a string for printing.
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("Server: got connection from %s\n", s);

        if (handle_connection(new_fd, current->c) == 0){
            // if we sent a contact, advance to the next
            current = current->next;
            if (current == NULL) {
                current = list;
            }
        }
        close(new_fd);
    }

    printf("\nSIGINT received, shutting down server...\n");

    free_contactlist(list);
    
    // This part is unreachable in this simple server, but it's good practice
    // to show where the listening socket would be closed.
    close(sockfd);

    return EXIT_SUCCESS;
}
