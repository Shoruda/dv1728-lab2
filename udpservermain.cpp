#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* You will to add includes here */

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

void handle_text_client(int sockfd, struct sockaddr_storage *client_addr, socklen_t addrlen) {
    char buffer[256];

    initCalcLib();
    char *operation = randomType();
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

    snprintf(buffer, sizeof(buffer), "%s %d %d\n", operation, value1, value2);
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)client_addr, addrlen);
    printf("Sent assignment (TEXT): %s %d %d\n", operation, value1, value2);

    int n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)client_addr, &addrlen);
    if (n < 0) {
      perror("recvfrom");
      return;
    }
    buffer[n] = '\0';
    printf("Answer: %s\n", buffer);

    int client_result = atoi(buffer);
    if (client_result == correct_result) {
      const char *okmsg = "OK\n";
      sendto(sockfd, okmsg, strlen(okmsg), 0, (struct sockaddr *)client_addr, addrlen);
      printf("Client was correct! (%d)\n", client_result);
    } else {
      const char *errmsg = "ERROR\n";
      sendto(sockfd, errmsg, strlen(errmsg), 0, (struct sockaddr *)client_addr, addrlen);
      printf("Client was wrong! Expected %d but got %d\n", correct_result, client_result);
    }
}

void handle_binary_client(int sockfd, struct sockaddr_storage *client_addr, socklen_t addrlen) {
    initCalcLib();
    char *operation = randomType();
    int value1 = randomInt();
    int value2 = randomInt();

    calcProtocol msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = htons(1);
    msg.major_version = htons(1);
    msg.minor_version = htons(1);
    msg.id = htonl(rand());

    if (strcmp(operation, "add") == 0) 
      msg.arith = htonl(1);
    else if (strcmp(operation, "sub") == 0) 
      msg.arith = htonl(2);
    else if (strcmp(operation, "mul") == 0) 
      msg.arith = htonl(3);
    else if (strcmp(operation, "div") == 0) 
      msg.arith = htonl(4);
    msg.inValue1 = htonl(value1);
    msg.inValue2 = htonl(value2);

    printf("Sent assignment (BINARY): %s %d %d\n", operation, value1, value2);
    sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)client_addr, addrlen);

    calcProtocol ans;
    int n = recvfrom(sockfd, &ans, sizeof(ans), 0, (struct sockaddr *)client_addr, &addrlen);
    if (n < sizeof(ans)) {
      fprintf(stderr, "binary recv failed\n");
      return;
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
    else if (strcmp(operation, "div") == 0) 
      correct_result = value1 / value2;
    else 
      correct_result = 0;

    printf("Expected result: %d\n", correct_result);

    calcMessage response;
    memset(&response, 0, sizeof(response));
    response.type = htons(2);
    response.major_version = htons(1);
    response.minor_version = htons(1);
    response.message = htonl(client_result == correct_result ? 1 : 2);

    sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr *)client_addr, addrlen);

    if (client_result == correct_result)
      printf("Client was correct! (%d)\n", client_result);
    else
      printf("Client was wrong! Expected %d but got %d\n", correct_result, client_result);
}

int main(int argc, char *argv[]){
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  
  char *input = argv[1];
  char *sep = strchr(input, ':');
  
  if (!sep) {
    fprintf(stderr, "Error: input must be in host:port format\n");
    return 1;
  }
  
  // Allocate buffers big enough
  char hoststring[256];
  char portstring[64];
  
  // Copy host part
  size_t hostlen = sep - input;
  if (hostlen >= sizeof(hoststring)) {
    fprintf(stderr, "Error: hostname too long\n");
    return 1;
  }
  strncpy(hoststring, input, hostlen);
  hoststring[hostlen] = '\0';
  
  // Copy port part
  strncpy(portstring, sep + 1, sizeof(portstring) - 1);
  portstring[sizeof(portstring) - 1] = '\0';
  
  printf("UDP Server on: %s:%s\n", hoststring,portstring);

  // DO MAGIC :3

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(hoststring, portstring, &hints, &res) != 0) {
    perror("getaddrinfo");
    exit(1);
  }

  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0) {
    perror("socket");
    exit(1);
  }

  if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
    perror("bind");
    close(sockfd);
    exit(1);
  }
  freeaddrinfo(res);

  fd_set readfds;
  char buffer[256];
  struct sockaddr_storage client_addr;
  socklen_t addrlen = sizeof(client_addr);

  printf("Server ready, waiting for UDP packets...\n");
  while (1) {
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    int activity = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);

    if (activity < 0) {
      perror("select");
      continue;
    }

    if (activity == 0) {
      printf("No activity, waiting again...\n");
      continue;
    }

    if (FD_ISSET(sockfd, &readfds)) {
      int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                        (struct sockaddr *)&client_addr, &addrlen);
      if (n < 0) {
          perror("recvfrom");
          continue;
      }

      printf("Received %d bytes: %s\n", n, buffer);
      // binary
      if (n == sizeof(calcMessage)) {
        calcProtocol pkt;
        memcpy(&pkt, buffer, sizeof(pkt));

        pkt.type          = ntohs(pkt.type);
        pkt.major_version = ntohs(pkt.major_version);
        pkt.minor_version = ntohs(pkt.minor_version);
        pkt.id            = ntohl(pkt.id);
        pkt.arith         = ntohl(pkt.arith);
        pkt.inValue1      = ntohl(pkt.inValue1);
        pkt.inValue2      = ntohl(pkt.inValue2);
        pkt.inResult      = ntohl(pkt.inResult);

        printf("Binary packet received: type=%d id=%d arith=%d v1=%d v2=%d res=%d\n",
          pkt.type, pkt.id, pkt.arith, pkt.inValue1, pkt.inValue2, pkt.inResult);

        handle_binary_client(sockfd, &client_addr, addrlen);
      }
      // text
      else {
        buffer[n] = '\0';
        printf("Text packet received: %s\n", buffer);

        if (strncmp(buffer, "TEXT", 4) == 0) {
            handle_text_client(sockfd, &client_addr, addrlen);
        } else {
            fprintf(stderr, "Unknown text format\n");
        }
      }
        }
      }

close(sockfd);
return 0;
}
