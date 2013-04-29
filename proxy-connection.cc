#include "proxy-connection.h"
#include "http-response.h"

/*
Global Variables
*/
// to count the number of connections
int count_connections;
// to guard number of connections
pthread_mutex_t count_mutex;
pthread_cond_t count_cond;


int createSocketAndConnect(string host, unsigned short port){
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;
  char portString[10];

  // we need the port to be a string to use
  sprintf(portString, "%u", port);  
  /* Obtain address(es) matching host/port */

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;    
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  hints.ai_flags = 0;
  hints.ai_protocol = 0;          /* Any protocol */

  s = getaddrinfo(host.c_str(), portString, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }

  /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype,
		 rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break;                  /* Success */

    close(sfd);
  }

  if (rp == NULL) {               /* No address succeeded */
    fprintf(stderr, "Could not connect\n");
    return -1;
  }
  cout<< "returning a socket connection!"<< sfd << endl;;
  freeaddrinfo(result);           /* No longer needed */
  return sfd;

  ///////////////////////////////////////////////////////////////////////////////////
  /*  int sockfd = socket(AF_INET , SOCK_STREAM, 0);

  if (sockfd < 0)
    return -1; // error code or something

  struct sockaddr client_addr;
  socklen_t sin_size = sizeof( struct sockaddr );

  int rv = connect(sockfd, (struct sockaddr *)&client_addr, &sin_size);
  if(rv < 0)
    return rv;
  return sockfd;*/
}

void* socketConnection( void* parameters){
  fprintf(stderr, "You are a connnection");

  param_t *p = (param_t *) parameters;
  const char *sendbuf = "this is a test";
  send(p->sockfd, sendbuf, (int)strlen(sendbuf), 0);

  char recvbuf[DEFAULT_BUFLEN];
  int recvbuflen = DEFAULT_BUFLEN-1;
  struct timeval tv;
  fd_set readfds;
  int n, rv;

  while(true)
  {
    //persistent connection:  continue to listen to the socket at all times.
    // clear the set ahead of time
    FD_ZERO(&readfds);

    // add our descriptors to the set
    FD_SET(p->sockfd, &readfds);
    //for now
    n = p->sockfd + 1;
    tv.tv_sec = 10;
    tv.tv_usec = 500000;
    rv = select(n, &readfds, NULL, NULL, &tv);

    if (rv == -1) {
      perror("select"); // error occurred in select()
    } else if (rv == 0) {
      fprintf(stderr, "Timeout occurred!  No data after 10.5 seconds.\n");
    } else {
      // one or both of the descriptors have data
      if (FD_ISSET(p->sockfd, &readfds)) {
	rv = recv(p->sockfd, recvbuf, recvbuflen, 0);
	if (rv < 0){
	  fprintf(stderr, "recv failed\n");
	  return NULL;
	}
	//recv doesn't put a \n cause it sucks so do it here:
	recvbuf[rv]='\0';
	HttpRequest myRequest;
	try{
	  myRequest.ParseRequest(recvbuf, rv);
	}catch(ParseException e){
	//TODO error 400 if bad request.
	  fprintf(stderr, "parse exception (will send notice to client)\n");
	}
	string host = myRequest.GetHost();
	unsigned short port = myRequest.GetPort();
	if (port==0){
	  port = 80;
	}
	cout<< "host and port " << host << ": " << port << endl;
	int hostSock = createSocketAndConnect(host, port);
	cout<<hostSock << " is the socket!\n";
	// echo response for now                                                                                                                                      
	string r(recvbuf);
	string message = "You said: " + r;
	send(p->sockfd,message.c_str(),strlen(message.c_str()),0);

      }

    }
  }

  //  int rv = recv(p->sockfd,recvbuf,recvbuflen, 0);
  //if (rv < 0){
  //fprintf(stderr, "recv failed\n");
  //return NULL;
  //}

	//update connection count
	close(p->sockfd);
    pthread_mutex_lock(&count_mutex);
    count_connections--;
    pthread_cond_signal(&count_cond);
    pthread_mutex_unlock(&count_mutex);
    pthread_exit(NULL);

  return NULL;
}

