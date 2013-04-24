#include "proxy-connection.h"

void* socketConnection( void* parameters){
  fprintf(stderr, "You are a connnection");

  param_t *p = (param_t *) parameters;
  const char *sendbuf = "this is a test";
  send(p->sockfd, sendbuf, (int)strlen(sendbuf), 0);

  char recvbuf[DEFAULT_BUFLEN];
  int recvbuflen = DEFAULT_BUFLEN;

  int rv = recv(p->sockfd,recvbuf,recvbuflen, 0);
  if (rv < 0){
    fprintf(stderr, "recv failed\n");
    return NULL;
  }

  // echo response for now                                                                                                                                                          
  string r(recvbuf);
  string message = "You said: " + r;
  send(p->sockfd,message.c_str(),(int)strlen( message.c_str()),0);

  return NULL;
}

