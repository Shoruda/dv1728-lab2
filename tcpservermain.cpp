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
#include "protocol.h"
// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG

#define BACKLOG 5 

using namespace std;

void handle_text_client(int sock)
{
    char newbuffer[256];
    initCalcLib();
    char* operation = randomType();
    int value1 = randomInt();
    int value2 = randomInt();

    int correct_result;
    if (strcmp(operation, "add") == 0) 
        correct_result = value1 + value2;
    else if (strcmp(operation, "sub") == 0) 
        correct_result = value1 - value2;
    else if (strcmp(operation, "mul") == 0) 
        correct_result = value1 * value2;
    else if (strcmp(operation, "div") == 0) 
        correct_result = value1 / value2;
    else 
        correct_result = 0;

    snprintf(newbuffer, sizeof(newbuffer), "%s %d %d\n", operation, value1, value2);
    send(sock, newbuffer, strlen(newbuffer), 0);
    printf("Sent assignment: %s %d %d\n", operation, value1, value2);

    int n = recv(sock, newbuffer, sizeof(newbuffer) - 1, 0);
    if (n < 0) {
        fprintf(stderr, "recv error: %s\n", strerror(errno));
        close(sock);
        exit(1);
    }
    newbuffer[n] = '\0';
    printf("answer: %s\n", newbuffer);

    int client_result = atoi(newbuffer);
    if (client_result == correct_result) {
        const char *okmsg = "OK\n";
        send(sock, okmsg, strlen(okmsg), 0);
        printf("Client was correct! (%d)\n", client_result);
    } else {
        const char *errmsg = "ERROR\n";
        send(sock, errmsg, strlen(errmsg), 0);
        printf("Client was wrong! Expected %d but got %d\n", correct_result, client_result);
    }

    close(sock);
    exit(0);
}

void handle_binary_client(int sock)
{
    initCalcLib();
    char* operation = randomType();
    int value1 = randomInt();
    int value2 = randomInt();

    calcProtocol msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = htons(1);
    msg.major_version = htons(1);
    msg.minor_version = htons(1);
    msg.id = htonl(rand());
    if (strcmp(operation,"add")==0) 
        msg.arith = htonl(1);
    else if (strcmp(operation,"sub")==0) 
        msg.arith = htonl(2);
    else if (strcmp(operation,"mul")==0) 
        msg.arith = htonl(3);
    else if (strcmp(operation,"div")==0) 
        msg.arith = htonl(4);
    msg.inValue1 = htonl(value1);
    msg.inValue2 = htonl(value2);

    printf("Sent assignment (BINARY): %s %d %d\n", operation, value1, value2);

    send(sock, &msg, sizeof(msg), 0);

    calcProtocol ans;
    int n = recv(sock, &ans, sizeof(ans), 0);
    if (n < sizeof(ans)) {
        fprintf(stderr, "binary recv failed\n");
        close(sock);
        exit(1);
    }

    int client_result = ntohl(ans.inResult);
    printf("Received answer (BINARY): %d\n", client_result);

    int correct_result;
    if (strcmp(operation, "add") == 0) 
        correct_result = value1 + value2;
    else if (strcmp(operation, "sub") == 0) 
        correct_result = value1 - value2;
    else if (strcmp(operation, "mul") == 0) 
        correct_result = value1 * value2;
    else if (strcmp(operation, "div") == 0) {

        correct_result = value1 / value2;
    } else 
        correct_result = 0;

    printf("Expected result: %d\n", correct_result);

    calcMessage response;
    memset(&response, 0, sizeof(response));
    response.type = htons(2);
    response.major_version = htons(1);
    response.minor_version = htons(1);
    response.message = htonl(client_result == correct_result ? 1 : 2);

    send(sock, &response, sizeof(response), 0);

    if (client_result == correct_result) {
        printf("Client was correct! (%d)\n", client_result);
    } else {
        printf("Client was wrong! Expected %d but got %d\n", correct_result, client_result);
    }

    close(sock);
    exit(0);
}

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

    //DO MAGIC :3

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
            continue;
        }

        struct timeval time;
        time.tv_sec = 5;
        time.tv_usec = 0;
        setsockopt(newsock, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork error: %s\n", strerror(errno));
            close(newsock);
            continue;
        }

        if (pid == 0) {
            close(sockfd);

            printf("Client connected!\n");

            const char *msg = "TEXT TCP 1.1\nBINARY TCP 1.1\n";
            send(newsock, msg, strlen(msg), 0);

            char buffer[256];
            int n = recv(newsock, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) {
            const char *errmsg = "ERROR: MESSAGE LOST (TIMEOUT)\n";
            send(newsock, errmsg, strlen(errmsg), 0);
            close(newsock);
            exit(1);
            }
            buffer[n] = '\0';

            if (strncmp(buffer, "TEXT", 4) == 0) {
                handle_text_client(newsock);
            } else if (strncmp(buffer, "BINARY", 6) == 0) {
                handle_binary_client(newsock);
            } else {
                const char *errmsg = "ERROR: Invalid protocol\n";
                send(newsock, errmsg, strlen(errmsg), 0);
                close(newsock);
                return 0;
            }
        } else {
            close(newsock);
        }
    }

    close(sockfd);
    return 0;
}


