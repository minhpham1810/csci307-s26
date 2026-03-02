/*
 Utility functions for HTTP servers and clients

 */

#include "httputils.h"

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
void *get_in_addr(struct sockaddr *sa) {
    // Check the address family field to determine the IP version.
    if (sa->sa_family == AF_INET) {
        // It's an IPv4 address.
        // 1. Cast the generic `struct sockaddr*` to a `struct sockaddr_in*`.
        // 2. Access the `sin_addr` member, which holds the IPv4 address.
        // 3. Return a pointer to this address.
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    // It's an IPv6 address.
    // 1. Cast the generic `struct sockaddr*` to a `struct sockaddr_in6*`.
    // 2. Access the `sin6_addr` member, which holds the IPv6 address.
    // 3. Return a pointer to this address.
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/**
 * @brief Sets up a listening socket.
 *
 * This function creates a socket, binds it to the specified port, and puts it
 * in listening mode, ready to accept incoming connections.
 *
 * @param port The port number to listen on.
 * @return The file descriptor for the listening socket, or -1 on failure.
 */
int setup_listening_socket(const char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;

    // First, load up address structs with getaddrinfo():
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Use IPv4 or IPv6, whichever is available
    hints.ai_socktype = SOCK_STREAM; // This is a TCP socket
    hints.ai_flags = AI_PASSIVE;     // Use my local IP

    // getaddrinfo() does all the heavy lifting of resolving the host and service (port)
    // and filling out the sockaddr structs for us. It returns a linked list of
    // potential addresses in 'servinfo'.
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "server: getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    // Loop through all the results and bind to the first one we can.
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Create the socket based on the address info.
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue; // Try the next address
        }

        // Set a socket option to allow reusing the port. This is crucial for servers
        // to avoid the "Address already in use" error upon restarting.
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            continue; // Try the next address
        }

        // Bind the socket to the port we passed in to getaddrinfo().
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue; // Try the next address
        }

        // If we got here, we successfully bound the socket.
        break;
    }

    // Free the linked list, as we no longer need it.
    freeaddrinfo(servinfo);

    if (p == NULL) {
        return -1;
    }

    return sockfd;
}

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
int create_connection(const char *hostname, const char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    //char s[INET6_ADDRSTRLEN];

    // --- Address Resolution (getaddrinfo) ---
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // --- Socket Creation and Connection ---
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // inet_ntop(p->ai_family, &(((struct sockaddr_in*)p->ai_addr)->sin_addr), s, sizeof s);
        // printf("Client: trying address %s on port %s\n", s, port);

        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: failed to create socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: failed to connect");
            continue;
        }

        break; // If we get here, we successfully connected.
    }

    // if p is NULL we hit the end of the list of addresses without making a connection.
    if (p == NULL) {        
        freeaddrinfo(servinfo);
        return -1;
    }

    // inet_ntop(p->ai_family, &(((struct sockaddr_in*)p->ai_addr)->sin_addr), s, sizeof s);
    // printf("Client: connected to %s on port %s\n\n", s, port);

    freeaddrinfo(servinfo); // All done with this structure.

    return sockfd;
}

/**
 * @brief Creates and sends a basic HTTP GET request to the server.
 *
 * @param sockfd The connected socket file descriptor.
 * @param hostname The hostname of the server, used for the "Host" header.
 * @param method The HTTP method, eg GET.
 * @param path The Path to request in the HTTP request.
 * @return number of bytes sent or error code.
 */
int send_http_request(int sockfd, const char *hostname, char *method, char *path) {
    static char request[512];
    snprintf(request, sizeof(request),
             "%s %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n",
             method, path,
             hostname);

    return send(sockfd, request, strlen(request), 0);
}

/**
 * @brief Receives data from the server
 *
 * This function loops, calling recv() until the server closes the connection
 * or an error occurs.
 *
 * @param sockfd The connected socket file descriptor.
 * @return number of bytes recieved or -1 on error.
 */
int receive_http_response(int sockfd, char *response_buffer, int buffer_size) {    
    int bytes_received = 0;

    while (1){
        int last_received = recv(sockfd, 
            response_buffer + bytes_received, 
            buffer_size - bytes_received - 1, 0);
        
        if (last_received <= 0){
            // pass error codes out
            bytes_received = last_received;
            break;
        } 
        if (last_received == 0){
            // nothing more to receive
            response_buffer[bytes_received] = '\0';
            break;
        }
        bytes_received += last_received;
    }
    
    return bytes_received;
}