/**
 * Listens on specified TCP ports. On connect, close the connection.
 * Created Marchiori 2025 with GPT-4o
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>

// Global variable to indicate if the program should stop (semaphore)
volatile sig_atomic_t stop;

// set stop to 1 when SIGINT is received
void handle_sigint(int sig) {
    (void)sig; // unused
    stop = 1;
}

// install the SIGINT signal handler
void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // Check for correct usage
    if (argc < 2) {
        printf("Usage: %s <port1> <port2> ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Block SIGINT initially (good practice)
    sigset_t mask, orig_mask;
    sigemptyset(&mask);
    if (pthread_sigmask(SIG_BLOCK, &mask, &orig_mask) != 0) { // More portable than sigprocmask
        perror("pthread_sigmask");
        return 1;
    }

    // set mask for pselect to ignore SIGINT later on
    sigaddset(&mask, SIGINT);

    // Create an array of ports and sockets
    int ports[argc - 1];
    int sockets[argc - 1];
    fd_set readfds;
    struct timespec timeout;
    int max_sd = 0;

    // Create sockets and bind to ports
    for (int i = 1; i < argc; i++) {
        ports[i - 1] = atoi(argv[i]);
        sockets[i - 1] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets[i - 1] == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;

        // Convert port to network byte order!!!!
        addr.sin_port = htons(ports[i - 1]);

        // bind the socket to the port
        if (bind(sockets[i - 1], (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        // tell the socket to listen for incoming connections
        if (listen(sockets[i - 1], 1) == -1) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // Keep track of the maximum file descriptor
        if (sockets[i - 1] > max_sd) {
            max_sd = sockets[i - 1];
        }
    }
    
    setup_signal_handler();
    time_t start = time(NULL);
   
    // set timeout to 50ms
    timeout.tv_sec = 0;    
    timeout.tv_nsec = .05 * 1e9;

    while (!stop && (difftime(time(NULL), start) < 10)) {
        
        FD_ZERO(&readfds);
        for (int i = 0; i < argc - 1; i++) {
            FD_SET(sockets[i], &readfds);
        }
        
        // Wait for activity on any of the sockets
        // from the man page:
        //     nfds   This argument should be set to the highest-numbered file descriptor in any of the three sets, plus 1.  The indicated file descriptors in each set are
        //              checked, up to this limit (but see BUGS).
        int activity = pselect(max_sd + 1, &readfds, NULL, NULL, &timeout, &mask);
        if (activity < 0 && errno != EINTR) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Check each socket for activity and accept the connection
        for (int i = 0; i < argc - 1; i++) {
            // check if the socket is ready to read
            if (FD_ISSET(sockets[i], &readfds)) {
                int new_socket;
                struct sockaddr_in addr;
                socklen_t addrlen = sizeof(addr);
                if ((new_socket = accept(sockets[i], (struct sockaddr *)&addr, &addrlen)) < 0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                printf("Connection from %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                close(new_socket);
            }
        }
    }

    // Restore original signal mask (important!)
    if (pthread_sigmask(SIG_SETMASK, &orig_mask, NULL) != 0) {
        perror("pthread_sigmask");
        return 1;
    }

    // Check if the loop was exited due to SIGINT or timeout
    if (stop) {
        printf("ctrl-c, Exit\n");
    } else {
        printf("Timeout reached, exiting...\n");
    }

    // Close all listening sockets
    for (int i = 0; i < argc - 1; i++) {
        close(sockets[i]);
    }

    return 0;
}