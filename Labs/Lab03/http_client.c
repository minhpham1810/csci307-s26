// =================================================================================
// Gemini Code Assist: Simple HTTP Client Example (Modular)
//
// This program acts as a minimal HTTP/1.0 client. It connects to a specified
// server and port, sends a GET request for the root document, and prints the
// server's response to the console.
//
// This program demonstrates:
//  - TCP/IP client socket programming using modern best practices (getaddrinfo).
//  - Basic HTTP/1.0 GET request formatting.
//  - Looping to receive a complete response from a server.
//  - Comprehensive error checking and resource management.
//
// To Compile:
//   gcc -std=gnu17 -Wall -o http_client http_client.c
//
// To Run:
//   Provide a hostname and a port number as command-line arguments.
//
//   Example 1: Connect to a local server (like the http_server.c example)
//      ./http_client localhost 8080
//
//   Example 2: Connect to a public web server on the standard HTTP port (80)
//      ./http_client example.com 80
// =================================================================================

#include <stdio.h>      // For standard I/O functions like printf, fprintf
#include <stdlib.h>     // For exit(), malloc(), free()
#include <string.h>     // For string manipulation functions like memset, strlen
#include <unistd.h>     // For close()
#include <sys/types.h>  // For standard system data types
#include <sys/socket.h> // For socket programming functions (socket, connect, send, recv)
#include <netdb.h>      // For network database operations (getaddrinfo)
#include <arpa/inet.h>  // For functions to manipulate IP addresses (inet_ntop)
#include <errno.h>      // For the 'errno' variable to get error numbers

#include "http/httputils.h"

#define MAX_BUFFER 16*1024 // The size of our buffer for receiving data (16 KiB)

char response_buffer [MAX_BUFFER];

int main(int argc, char *argv[]) {
    // --- 1. Argument Parsing ---
    // A robust program always validates its input. We need exactly two arguments
    // from the user: the hostname and the port.
    if (argc != 3) {
        // fprintf sends formatted output to a specified stream. stderr is the
        // standard error stream, the conventional place for error messages.
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(EXIT_FAILURE); // Exit with a failure status
    }

    const char *hostname = argv[1];
    const char *port = argv[2];

    // --- 2. Create Connection ---
    // This function handles all the details of DNS lookup, socket creation,
    // and connecting to the server.
    int sockfd = create_connection(hostname, port);
    if (sockfd == -1) {
        fprintf(stderr, "client: failed to create connection\n");
        exit(EXIT_FAILURE);
    }

    // --- 3. Send HTTP Request ---
    printf("--- Sending Request ---\n%s-------------------------\n\n", "GET / HTTP/1.0");

    if (send_http_request(sockfd, hostname, "GET", "/") < 0) {
        perror("send failed");
        close(sockfd);
        exit(EXIT_FAILURE);   
    }

    // --- 4. Receive and Print Server's Response ---

    if (receive_http_response(sockfd, response_buffer, MAX_BUFFER) < 0){
        perror("receive error");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("--- Received Response ---\n");
    printf("%s-------------------------\n\n", response_buffer);

    // --- 5. Clean Up ---
    printf("Client: Connection closed.\n");
    // Always close your sockets when you're done with them to release the
    // system resources.
    close(sockfd);

    return EXIT_SUCCESS; // Indicate successful execution
}
