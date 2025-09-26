#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */
/* test */
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
// Included to get the support library
#include <calcLib.h>
#include <errno.h>

#include "calcLib.h"
// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG

#define BACKLOG 5 

using namespace std;


// ... all includes and setup as in your original code ...

int main(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(stderr, "Usage: %s host:port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *input = argv[1];
    char *sep = strchr(input, ':');
    if (!sep) {
        fprintf(stderr, "Error: input must be in host:port format\n");
        return 1;
    }

    char hoststring[256];
    char portstring[64];
    size_t hostlen = sep - input;
    strncpy(hoststring, input, hostlen);
    hoststring[hostlen] = '\0';
    strncpy(portstring, sep + 1, sizeof(portstring) - 1);
    portstring[sizeof(portstring) - 1] = '\0';

    printf("TCP server on: %s:%s\n", hoststring, portstring);

    struct addrinfo hints, *res, *p;
    int status;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(hoststring, portstring, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    int sockfd;
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        int yes = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;

        close(sockfd);
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to bind socket\n");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) {
        fprintf(stderr, "listen error: %s\n", strerror(errno));
        exit(1);
    }

    printf("Server is listening...\n");

    while (1) {
        struct sockaddr_storage cli_addr;
        socklen_t clilen = sizeof(cli_addr);

        int newsock = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsock == -1) {
            fprintf(stderr, "accept error: %s\n", strerror(errno));
            continue; // parent continues
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork error: %s\n", strerror(errno));
            close(newsock);
            continue;
        }

        if (pid == 0) {
            // Child process
            close(sockfd); // child doesn't need listening socket

            printf("Client connected!\n");

            const char *msg = "TEXT TCP 1.1\n";
            send(newsock, msg, strlen(msg), 0);

            char buffer[256];
            int n = recv(newsock, buffer, sizeof(buffer) - 1, 0);
            if (n < 0) {
                fprintf(stderr, "recv error: %s\n", strerror(errno));
                close(newsock);
                exit(1);
            }
            buffer[n] = '\0';
            printf("Got message: %s\n", buffer);

            initCalcLib();
            char* operation = randomType();
            int value1 = randomInt();
            int value2 = randomInt();

            int correct_result;
            if (strcmp(operation, "add") == 0) correct_result = value1 + value2;
            else if (strcmp(operation, "sub") == 0) correct_result = value1 - value2;
            else if (strcmp(operation, "mul") == 0) correct_result = value1 * value2;
            else if (strcmp(operation, "div") == 0) correct_result = value1 / value2;
            else correct_result = 0;

            snprintf(buffer, sizeof(buffer), "%s %d %d\n", operation, value1, value2);
            send(newsock, buffer, strlen(buffer), 0);
            printf("Sent assignment: %s %d %d\n", operation, value1, value2);

            n = recv(newsock, buffer, sizeof(buffer) - 1, 0);
            if (n < 0) {
                fprintf(stderr, "recv error: %s\n", strerror(errno));
                close(newsock);
                exit(1);
            }
            buffer[n] = '\0';
            printf("answer: %s\n", buffer);

            int client_result = atoi(buffer);
            if (client_result == correct_result) {
                const char *okmsg = "OK\n";
                send(newsock, okmsg, strlen(okmsg), 0);
                printf("Client was correct! (%d)\n", client_result);
            } else {
                const char *errmsg = "ERROR\n";
                send(newsock, errmsg, strlen(errmsg), 0);
                printf("Client was wrong! Expected %d but got %d\n", correct_result, client_result);
            }

            close(newsock);
            exit(0);
        } else {
            // Parent process
            close(newsock);
        }
    }

    close(sockfd);
    return 0;
}


