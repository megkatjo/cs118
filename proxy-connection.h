#ifndef _PROXY_CONNECTION_H_
#define _PROXY_CONNECTION_H_

#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string>
#include <cstring>
using namespace std;

const int MAX_CONNECTIONS = 10;
const int BACKLOG = 10;
const int DEFAULT_BUFLEN = 512;

typedef struct {
  int sockfd;
  struct sockaddr client_addr;
  socklen_t sin_size;
} param_t;

void* socketConnection(void* parameters);

#endif

