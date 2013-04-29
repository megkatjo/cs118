#ifndef _PROXY_CONNECTION_H_
#define _PROXY_CONNECTION_H_

#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include "http-request.h"
using namespace std;

const int MAX_CONNECTIONS = 10;
const int BACKLOG = 10;
const int DEFAULT_BUFLEN = 512;

/*
Global Variables
*/
// to count the number of connections
extern int count_connections;
// to guard number of connections
extern pthread_mutex_t count_mutex;
extern pthread_cond_t count_cond;

typedef struct {
  int sockfd;
  struct sockaddr client_addr;
  socklen_t sin_size;
} param_t;

void* socketConnection(void* parameters);

#endif

