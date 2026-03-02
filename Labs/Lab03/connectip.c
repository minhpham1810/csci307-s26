/*
** connectip.c -- resolve a host and attempt a connection
**
** based on the example from Beej's Guide to Network Programming
** https://beej.us/guide/bgnet/source/examples/showip.c
** by Marchiori 2025
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res, *p;
    int status;

    // IPV6 addresses are longer than IPV4 addresses
    // so this will be big enough for either.
    char ipstr[INET6_ADDRSTRLEN];

    if (argc != 3) {
        fprintf(stderr,"usage: showip hostname port\n");
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
    if ((status = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    /*** IMPORTANT
     * if you see code using gethostbyname() or gethostbyaddr()
     * it's outdated. getaddrinfo() is the new way to do it!!!!!!
     * those will not work for IPv6 and you will be stuck in the past!
     * You do not want to be stuck in the past!
     */

    printf("IP addresses for %s:%s\n", argv[1], argv[2]);

    // the response is a LINKED LIST of addreses (just like slist)!
    for(p = res;p != NULL; p = p->ai_next) {
        void *addr;     // pointer to the address (ipv4 or ipv6 format!)
        char *ipver;    // string to hold the ip version

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
        // NtoP = Network to Presentation 
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("  %s: %s\n", ipver, ipstr);

        // create a socket using the address family, socket type and protocol
        int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        
        // check if socket is valid
        if (sockfd < 0) {
            perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
        {
            printf("Connected to %s:%s\n", ipstr, argv[2]);
            close(sockfd);
            
        }
        else
        {
            printf("Failed to connect to %s:%s\n", ipstr, argv[2]);
        }
    }

    // free the linked list (dealocate memory)
    freeaddrinfo(res); // free the linked list

    return 0;
}