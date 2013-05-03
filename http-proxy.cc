/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */


#include "proxy-connection.h"
#include <iostream>
using namespace std;

int main (int argc, char *argv[])
{
	//initialize global variables
	count_connections = 0;
	pthread_mutex_init(&count_mutex, NULL);
	pthread_cond_init(&count_cond, NULL);

  // command line parsing
	printf("come in\n");
	int sockfd = socket(AF_INET , SOCK_STREAM, 0);

	if (sockfd < 0)
	    return -1; // error code or something
	fprintf(stderr, "passed socket\n");

	/*
	struct sockaddr service;
	service.sa_family = AF_INET;
	*/
	struct sockaddr_in service;	
	memset(&service, 0, sizeof(service));
	
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = htons(PORT_NUMBER);
	
        printf("created service\n");
	int bind_ret = bind(sockfd, (struct sockaddr*) &service, sizeof (service));
	if (bind_ret < 0)
	{
		fprintf(stderr, "failed bind\n");
                fprintf(stderr, "errno: %d",errno);
		close(sockfd);
		return bind_ret;
	}
	fprintf(stderr, "passed bind\n");
	int listen_ret = listen(sockfd, BACKLOG);    //how many are queued
	if (listen_ret < 0)
	{
		close(sockfd);
		return listen_ret; // error
	}
	fprintf(stderr, "passed listen\n");
	while(1){

		fprintf(stderr, "beginning while\n");  

		fprintf(stderr, "number of connections: %d\n", count_connections);  		
	
    	// check if already have MAXCONNECTIONS	
		pthread_mutex_lock(&count_mutex);
		if(count_connections >= MAX_CONNECTIONS){
			pthread_cond_wait(&count_cond, &count_mutex);
			fprintf(stderr, "passed max connections!\n"); 
		}
		pthread_mutex_unlock(&count_mutex);
		
	
		struct sockaddr client_addr;
		memset(&client_addr, 0, sizeof(client_addr));
    
		socklen_t sin_size = sizeof( struct sockaddr );
    
		int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
		if(clientfd < 0)
			return clientfd;
	fprintf(stderr, "passed accept\n");	
    
		// update connection count
        pthread_mutex_lock(&count_mutex);
		count_connections++;
        pthread_mutex_unlock(&count_mutex);
    
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
	fprintf(stderr, "created thread\n");
		// note: socketConnection is our client function
    
	}
	// close socket
	close(sockfd);

	pthread_exit(NULL);
	return 0;
}
