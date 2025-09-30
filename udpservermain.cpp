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
#include <map>
#include <ctime>

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
#define DEBUG


using namespace std;

struct ClientState {
    struct sockaddr_storage addr;
    socklen_t addrlen;
    time_t sent_time;
    int correct_result;
    char operation[10];
    int value1;
    int value2;
    uint32_t id;
    bool is_binary;
};

std::map<uint32_t, ClientState> pending_clients;

uint32_t generate_client_key(struct sockaddr_storage *addr) {
  return (uint32_t)time(NULL) + rand();
}

void send_text_assignment(int sockfd, struct sockaddr_storage *client_addr, socklen_t addrlen) {
  char buffer[256];

  initCalcLib();
  char *operation = randomType();
  int value1 = randomInt() ;
  int value2 = randomInt() ;

  int correct_result;

  if (strcmp(operation, "add") == 0) 
    correct_result = value1 + value2;
  else if (strcmp(operation, "sub") == 0) 
    correct_result = value1 - value2;
  else if (strcmp(operation, "mul") == 0) 
    correct_result = value1 * value2;
  else if (strcmp(operation, "div") == 0) 
  {
    if (value2 == 0) value2 = 1;
    correct_result = value1 / value2;
  }
  else 
    correct_result = 0;

  snprintf(buffer, sizeof(buffer), "%s %d %d\n", operation, value1, value2);
  sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)client_addr, addrlen);
  printf("Sent assignment (TEXT): %s %d %d\n", operation, value1, value2);

  ClientState state;
  memcpy(&state.addr, client_addr, sizeof(*client_addr));
  state.addrlen = addrlen;
  state.sent_time = time(NULL);
  state.correct_result = correct_result;
  strncpy(state.operation, operation, sizeof(state.operation));
  state.value1 = value1;
  state.value2 = value2;
  state.is_binary = false;
  
  uint32_t key = generate_client_key(client_addr);
  pending_clients[key] = state;
}

void handle_text_answer(int sockfd, char *buffer, int n,
  struct sockaddr_storage *from_addr, socklen_t addrlen)
{
  buffer[n] = '\0';
  printf("Text answer received: %s\n", buffer);

  if (pending_clients.empty())
  {
      fprintf(stderr, "Got answer but no pending clients\n");
      return;
  }

  ClientState *state = NULL;
  uint32_t found_key = 0;

  for (auto &pair : pending_clients)
  {
    if (!pair.second.is_binary)
    {
      state = &pair.second;
      found_key = pair.first;
      break;
    }
  }

  if (!state)
  {
    fprintf(stderr, "No pending TEXT clients found\n");
    return;
  }

  int client_result = atoi(buffer);

  if (client_result == state->correct_result)
  {
    const char *okmsg = "OK\n";
    sendto(sockfd, okmsg, strlen(okmsg), 0,
      (struct sockaddr *)&state->addr, state->addrlen);
    printf("Client was correct! (%d)\n", client_result);
  }
  else
  {
    const char *errmsg = "ERROR\n";
    sendto(sockfd, errmsg, strlen(errmsg), 0,
      (struct sockaddr *)&state->addr, state->addrlen);
    printf("Client was wrong! Expected %d but got %d\n",
      state->correct_result, client_result);
  }

  pending_clients.erase(found_key);
}


void send_binary_assignment(int sockfd, struct sockaddr_storage *client_addr, socklen_t addrlen) {
  initCalcLib();
  char *operation = randomType();
  int value1 = randomInt();
  int value2 = randomInt();

  calcProtocol msg;
  memset(&msg, 0, sizeof(msg));
  msg.type = htons(1);
  msg.major_version = htons(1);
  msg.minor_version = htons(1);
  uint32_t id;
  do {
      id = rand();
  } while (pending_clients.find(id) != pending_clients.end());
  msg.id = htonl(id);

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
  
  ClientState state;
  memcpy(&state.addr, client_addr, sizeof(*client_addr));
  state.addrlen = addrlen;
  state.sent_time = time(NULL);
  strncpy(state.operation, operation, sizeof(state.operation));
  state.value1 = value1;
  state.value2 = value2;
  state.id = id;
  state.is_binary = true;

  int correct_result;
  if (strcmp(operation, "add") == 0) 
    correct_result = value1 + value2;
  else if (strcmp(operation, "sub") == 0) 
    correct_result = value1 - value2;
  else if (strcmp(operation, "mul") == 0) 
    correct_result = value1 * value2;
  else if (strcmp(operation, "div") == 0) 
  {
    if (value2 == 0) value2 = 1;
    correct_result = value1 / value2;
  }
  else 
    correct_result = 0;

  state.correct_result = correct_result;
  pending_clients[id] = state;
}

void handle_binary_answer(int sockfd, calcProtocol *ans, struct sockaddr_storage * from_addr, socklen_t addrlen)
{
  uint32_t id = ntohl(ans->id);
  int client_result = ntohl(ans->inResult);

  printf("Binary answer received: %d (ID: %u)\n", client_result, id);

  auto it = pending_clients.find(id);
  if (it == pending_clients.end())
  {
    fprintf(stderr, "received answer from unown client ID: %u\n", id);
    return;
  }

  ClientState &state = it->second;

  calcMessage response;
  memset(&response, 0, sizeof(response));
  response.type = htons(2);
  response.major_version = htons(1);
  response.minor_version = htons(1);
  response.message = htonl(client_result == state.correct_result ? 1 : 2);
  sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr *)&state.addr, state.addrlen);

  if (client_result == state.correct_result)
    printf("Client was correct! (%d)\n", client_result);
  else
    printf("Client was wrong! Expected %d but got %d\n", state.correct_result, client_result);
  pending_clients.erase(it);
}
void cleanup_clients() {
  time_t now = time(NULL);
  auto it = pending_clients.begin();
  while (it != pending_clients.end()) {
    if (now - it->second.sent_time > 5) {
      printf("Removing stale client (ID: %u)\n", it->first);
      it = pending_clients.erase(it);
    } else {
      ++it;
    }
  }
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

    int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0) {
      perror("select");
      continue;
    }

    if (activity == 0) {
      cleanup_clients();
      continue;
    }

    if (FD_ISSET(sockfd, &readfds)) {
      addrlen = sizeof(client_addr);
      int n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                        (struct sockaddr *)&client_addr, &addrlen);
      if (n < 0) {
        perror("recvfrom");
        continue;
      }


    if (n == sizeof(calcMessage)) 
    {
      calcMessage msg;
      memcpy(&msg, buffer, sizeof(msg));

      msg.type          = ntohs(msg.type);
      msg.message       = ntohl(msg.message);
      msg.protocol      = ntohs(msg.protocol);
      msg.major_version = ntohs(msg.major_version);
      msg.minor_version = ntohs(msg.minor_version);

      if (msg.type == 22) {
        if (msg.protocol != 17) {
        fprintf(stderr, "Invalid protocol: %d\n", msg.protocol);
        continue;
        }
        send_binary_assignment(sockfd, &client_addr, addrlen);
      }
      else
      {
        fprintf(stderr, "Unexpected message type: %d\n", msg.type);
      }
    }

    //binary answer
    else if (n == sizeof(calcProtocol))
    {
      calcProtocol ans;
      memcpy(&ans, buffer, sizeof(ans));
      handle_binary_answer(sockfd, &ans, &client_addr, addrlen);
    }

    // Text protocol
    else 
    {
      buffer[n] = '\0';
      printf("Text packet received: %s\n", buffer);

      if (strncmp(buffer, "TEXT", 4) == 0) 
      {
        send_text_assignment(sockfd, &client_addr, addrlen);
      } 
      else 
      {
        handle_text_answer(sockfd, buffer, n, &client_addr, addrlen);
      }
    }
  }
  }
  close(sockfd);
  return 0;
}
