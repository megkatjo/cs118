/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */


#include "proxy-connection.h"
#include <iostream>
using namespace std;


int main (int argc, char *argv[])
{
  // command line parsing

	int sockfd = socket(AF_INET , SOCK_STREAM, 0);

	if (sockfd < 0)
	    return -1; // error code or something

	/*
	struct sockaddr service;
	service.sa_family = AF_INET;
	*/
	
	struct sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = htons(27015);
	

	int bind_ret = bind(sockfd, (struct sockaddr*) &service, sizeof (service));
	if (bind_ret < 0)
		return bind_ret;

	int listen_ret = listen(sockfd, BACKLOG);    //how many are queued
	if (listen_ret == -1)
		return listen_ret; // error

	while(1){
    
    		// TODO: check if already have MAXCONNECTIONS
    
		struct sockaddr client_addr;
    
		socklen_t sin_size = sizeof( struct sockaddr );
    
		int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
		if(clientfd < 0)
			return clientfd;
	
		//int select(int numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
    
    
		//pthreads business
    
		param_t *params;
                params = new param_t;
                params->sockfd = clientfd;
                params->client_addr = client_addr;
                params->sin_size = sin_size;
    
		pthread_t threadID;
		int p = pthread_create (&threadID, NULL, socketConnection, (void*) params);
		if(p < 0)
			return p;	//change later    

		// note: socketConnection is our client function
    
	}
	// close socket
	//close(sockfd);

	pthread_exit(NULL);
	return 0;
}
