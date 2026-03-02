#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

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



/**
 * @brief Get a pointer to the binary IP address from a generic sockaddr struct.
 *
 * The `struct sockaddr` is a generic container. The actual address information
 * is stored in a more specific structure, either `struct sockaddr_in` for IPv4
 * or `struct sockaddr_in6` for IPv6. This function checks the address family
 * (`sa_family`) to determine which type of address it is, performs the
 * correct typecast, and returns a `void*` pointer to the binary IP address.
 * This is essential for functions like `inet_ntop` which need to work with
 * both IP versions.
 *
 * @param sa A pointer to a `struct sockaddr`.
 * @return A `void*` pointer to the IP address portion of the sockaddr
 *         (the `in_addr` for IPv4 or `in6_addr` for IPv6).
 */

void *get_in_addr(struct sockaddr *sa);

/**
 * @brief Sets up a listening socket.
 *
 * This function creates a socket, binds it to the specified port, and puts it
 * in listening mode, ready to accept incoming connections.
 *
 * @param port The port number to listen on.
 * @return The file descriptor for the listening socket, or -1 on failure.
 */
int setup_listening_socket(const char *port);


/**
 * @brief Establishes a TCP connection to a server.
 *
 * This function handles address resolution, socket creation, and connecting to the
 * server specified by the hostname and port. It iterates through the results
 * from getaddrinfo and connects to the first one that works.
 *
 * @param hostname The server's hostname (e.g., "example.com").
 * @param port The server's port (e.g., "80").
 * @return The socket file descriptor on success, or -1 on failure.
 */
int create_connection(const char *hostname, const char *port);

/**
 * @brief Creates and sends a basic HTTP GET request to the server.
 *
 * @param sockfd The connected socket file descriptor.
 * @param hostname The hostname of the server, used for the "Host" header.
 * @param method The HTTP method, eg GET.
 * @param path The Path to request in the HTTP request.
 */
int send_http_request(int sockfd, const char *hostname, char *method, char *path);

/**
 * @brief Receives data from the server and prints it to the console.
 *
 * This function loops, calling recv() until the server closes the connection
 * or an error occurs.
 *
 * @param sockfd The connected socket file descriptor.
 * @return number of bytes recieved or -1 on error.
 */
int receive_http_response(int sockfd, char *response_buffer, int buffer_size);


#endif // HTTP_UTILS_H  