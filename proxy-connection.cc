#include "proxy-connection.h"
#include "http-response.h"

void* socketConnection( void* parameters){
  fprintf(stderr, "You are a connnection");

  param_t *p = (param_t *) parameters;
  const char *sendbuf = "this is a test";
  send(p->sockfd, sendbuf, (int)strlen(sendbuf), 0);

  char recvbuf[DEFAULT_BUFLEN];
  int recvbuflen = DEFAULT_BUFLEN;
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
	// echo response for now                                                                                                                                      
	string r(recvbuf);
	string message = "You said: " + r;
	//char sendbuf[DEFAULT_BUFLEN];
	//	strcpy (&sendbuf, message.c_str());
	send(p->sockfd,message.c_str(),strlen(message.c_str())+1,0);

      }

    }
  }

  //  int rv = recv(p->sockfd,recvbuf,recvbuflen, 0);
  //if (rv < 0){
  //fprintf(stderr, "recv failed\n");
  //return NULL;
  //}



  return NULL;
}

