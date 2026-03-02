/*
** scanip.c -- scan common ports on a host.
** based on:
** https://beej.us/guide/bgnet/source/examples/showip.c
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "common_port.h"

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res, *p;
    int status;

    // IPV6 addresses are longer than IPV4 addresses
    // so this will be big enough for either.
    char ipstr[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr,"usage: showip hostname\n");
        return 1;
    }

    // be sure that the struct is empty!
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC; // AF_INET (IPv4) or AF_INET6 (IPv6) to force version

    // we specify a socket type to get only the addresses that can be used with that type
    hints.ai_socktype = SOCK_STREAM;   // SOCK_STREAM (TCP) or SOCK_DGRAM (UDP)

    // this is the application interface to the OS's DNS resolver
    // we could implement DNS resolution ourselves, but it's better to use the OS's resolver
    // and that would be a different lab!
    if ((status = getaddrinfo(argv[1], NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    /*** IMPORTANT
     * if you see code using gethostbyname() or gethostbyaddr()
     * it's outdated. getaddrinfo() is the new way to do it!!!!!!
     * those will not work for IPv6 and you will be stuck in the past!
     * You do not want to be stuck in the past!
     */

    printf("IP addresses for %s\n", argv[1]);

    // the response is a LINKED LIST of addresses (just like slist)!
    for(p = res;p != NULL; p = p->ai_next) {
        void *addr;     // pointer to the address (ipv4 or ipv6 format!)
        char *ipver;    // string to hold the ip version
        int open_ports = 0;  // counter for open ports

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6
        // and they point to different sturctures types!
        if (p->ai_family == AF_INET) { // IPv4

            // this is an ipv4 address
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else if(p->ai_family == AF_INET6) { // IPv6        

            // this is an ipv6 address
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        } else {
            // we are not handling other address families
            printf("Unknown family: %d\n", p->ai_family);
            continue;
        }

        // convert the IP to a string and print it:
        // ntop = Network to Presentation 
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("Scanning %s: %s\n", ipver, ipstr);

        // Scan common ports for this IP address
        for (int i = 0; common_tcp_ports[i].port != 0; i++) {
            int port = common_tcp_ports[i].port;
            const char *service = common_tcp_ports[i].service;

            // Create a socket for this connection attempt
            int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            
            if (sockfd < 0) {
                perror("socket");
                continue;
            }

            // Create a copy of the address structure to avoid modifying the original
            struct sockaddr_storage addr_copy;
            memcpy(&addr_copy, p->ai_addr, p->ai_addrlen);

            // Set the port in the copied address structure
            if (p->ai_family == AF_INET) {
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)&addr_copy;
                ipv4->sin_port = htons(port);
            } else if (p->ai_family == AF_INET6) {
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&addr_copy;
                ipv6->sin6_port = htons(port);
            }

            // Attempt to connect to the port using the copied address
            if (connect(sockfd, (struct sockaddr *)&addr_copy, p->ai_addrlen) == 0) {
                printf("Connected to %s on port %5d (%s)\n", ipstr, port, service);
                open_ports++;
                close(sockfd);
            } else {
                // Port is closed or filtered, don't print anything
                close(sockfd);
            }
        }
        
        // Print summary of open ports for this IP
        printf("Found %d open ports on %s\n", open_ports, ipstr);
    }

    // free the linked list (dealocate memory)
    freeaddrinfo(res); // free the linked list

    return 0;
}