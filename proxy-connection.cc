#include "proxy-connection.h"
#include "http-response.h"

/*
Global Variables
*/
// to count the number of connections
int count_connections;
cache_t cache;
// to guard number of connections
pthread_mutex_t count_mutex;
pthread_cond_t count_cond;
pthread_mutex_t cache_lock;


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
	//TODO error 400 if bad request. (send to client)
	  cout << e.what() << endl;
	  fprintf(stderr, "parse exception (will send notice to client)\n");
	  continue;
	}
	//also TODO look at the parserequest code and see if it also 
	//throws a unsupported method exception and catch that and 
	//send whatever error tho the client.
	string host = myRequest.GetHost();
	unsigned short port = myRequest.GetPort();
	if (port==0){
	  port = 80;
	  myRequest.SetPort(port);
	}
	cout<< "host and port " << host << ": " << port << endl;
	
	// Format request
	size_t req_length = myRequest.GetTotalLength() + 1;
	char *request = (char *)malloc(req_length);
	myRequest.FormatRequest(request);


	// Check if in cache already
	string data;
	string path = host + myRequest.GetPath();
	cache_t::iterator it = cache.find(path);
	if(it != cache.end()){
	  // Found in cache
	  fprintf(stderr,"in cache!\n");
	  data = it->second;
	}
	else {
	  // not in cache
	  fprintf(stderr,"not in cache\n");
	//TODO: put the following line into an if  depending on whether 
	//we already have a connection (question: do we want to keep the
	//connection with the host going??) well for now we'll close it.
	int hostSock = createSocketAndConnect(host, port);
	cout<<hostSock << " is the socket!\n";
	// echo response for now
	//TODO send request to host socket (whether new or old)
	//and then parse the response.  (need to set the port and
	//stuff of our client)

		  
	// Send request to remote host
	if(send(hostSock,request,req_length,0) < 0){
	  // error
	  fprintf(stderr,"could not send request to remote\n");
	  free(request);
	  return NULL;
	}
	fprintf(stderr,"sent request to remote host\n");
	// Get data from remote host
	// Loop until we get all the data
	while(true){
	  fprintf(stderr,"in while\n");
	  char databuffer[DEFAULT_BUFLEN];
	  int bytesWritten = recv(hostSock,databuffer,DEFAULT_BUFLEN,0);
	  fprintf(stderr,"recv returned %d\n",bytesWritten);
	  if (bytesWritten < 0){
	    fprintf(stderr,"error\n");
	    return NULL;
	  }
	  else if(bytesWritten == 0) {
	    // didn't recieve anything, done getting data?
	    fprintf(stderr,"done getting data\n");
	    break;
	  }
	  // append to data
	  fprintf(stderr,"appending\n");
	  data.append(databuffer,bytesWritten);
	  break;
	}
	fprintf(stderr,"got data\n");
	// Add to cache
	pthread_mutex_lock(&cache_lock);
	cache.insert(pair<string,string>(path,data));
	pthread_mutex_unlock(&cache_lock);
	fprintf(stderr,"added to cache\n");
	close(hostSock);
	} // end else (not in cache)

	// Send data back to client
	if(send(p->sockfd,data.c_str(),strlen(data.c_str()),0) == -1){
	  fprintf(stderr,"could not send data to client\n");
	  free(request);
	  return NULL;
	}

	free(request);
	
	//string r(recvbuf);
	//string message = "You said: " + r;
	//send(p->sockfd,message.c_str(),strlen(message.c_str()),0);
	//close(hostSock);//// TODO maybe.  for now we close it...
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

