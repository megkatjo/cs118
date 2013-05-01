#ifndef _PROXY_CONNECTION_H_
#define _PROXY_CONNECTION_H_

#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include "http-request.h"
#include <map>
#include <malloc.h>
#include <errno.h>
using namespace std;

#define PORT_NUMBER 14801

const int MAX_CONNECTIONS = 10;
const int MAX_HOST_CONNECTIONS = 100;
const int BACKLOG = 10;
const int DEFAULT_BUFLEN = 512;

/*
Global Variables
*/
// to count the number of connections
extern int count_connections;
typedef map<string,string> cache_t; // cache typedef
extern cache_t cache;  // cache
// to guard number of connections
extern pthread_mutex_t count_mutex;
extern pthread_cond_t count_cond;
extern pthread_mutex_t cache_lock; // cache lock

typedef struct {
  int sockfd;
  struct sockaddr client_addr;
  socklen_t sin_size;
} param_t;

void* socketConnection(void* parameters);

#endif

