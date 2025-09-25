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

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG

#define BACKLOG 5 

using namespace std;


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
  
  printf("TCP server on: %s:%s\n", hoststring,portstring);

  // DO MAGIC :3

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
    if (sockfd == -1)
        continue;

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        fprintf(stderr, "setsockopt error: %s\n", strerror(errno));
        exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
        break;
    }

    close(sockfd);
  }

  if (p == NULL) {
    fprintf(stderr, "Failed to bind socket\n");
    exit(1);
  }

  freeaddrinfo(res);

  //listen

  if (listen(sockfd, BACKLOG) == -1) {
    fprintf(stderr, "listen error: %s\n", strerror(errno));
    exit(1);
  }

  printf("Server is listening...\n");

  //accept

  struct sockaddr_storage cli_addr;
  socklen_t clilen = sizeof(cli_addr);

  int newsock = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
  if (newsock == -1) {
    fprintf(stderr, "accept error: %s\n", strerror(errno));
    exit(1);
  }

  printf("Client connected!\n");

  char buffer[256];
  int n = recv(newsock, buffer, sizeof(buffer) - 1, 0);
  if (n < 0) {
    fprintf(stderr, "recv error: %s\n", strerror(errno));
    exit(1);
  }
  buffer[n] = '\0';
  printf("Got message: %s\n", buffer);

  const char *msg = "Hello from TCP server";
  send(newsock, msg, strlen(msg), 0);

  close(newsock);
  close(sockfd);
  return 0;
}
