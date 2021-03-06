#include "proxy-connection.h"
#include "http-response.h"

//TODO ERROR CODES
// 502 "Bad Gateway" -> invalid response
// 404 "Not Found" -> not found anything matching request URI
// 503 "Service Unavailable" -> can't handle request right now
// 408 "Request Timeout"
// 304 "Not Modified" -> used w/ conditional GET
// 400 "Bad Request" -> parsing error
// 501 "Not Implemented" -> only get implemented

/*
Global Variables
*/
// to count the number of connections
int count_connections;
int count_hostConnections;
cache_t cache;
// to guard number of connections
pthread_mutex_t count_mutex;
pthread_cond_t count_cond;
pthread_mutex_t cache_lock;
pthread_mutex_t hostConn_lock;

// Used for cache purposes
string parseDate(string header,HttpResponse temp_response){
  string rawdate = temp_response.FindHeader(header);
  if (rawdate == ""){
    return "";
  }
  struct tm tm;
  strptime(rawdate.c_str(), "%a, %d %b %Y %T %Z", &tm);
  char date[80];
  strftime(date,sizeof(date),"%Y-%m-%d %X",&tm);
  return date;
}

// Used for cache purposes
string parseRAWDate(string rawdate){
  struct tm tm;
  strptime(rawdate.c_str(), "%a, %d %b %Y %T %Z", &tm);
  char date[80];
  strftime(date,sizeof(date),"%Y-%m-%d %X",&tm);
  return date;
}

// to create error message to send back to client
HttpResponse createErrorMessage(string error_code, string error_message)
{
  HttpResponse error_response;
  error_response.SetVersion("1.1");
  error_response.SetStatusCode(error_code);
  error_response.SetStatusMsg(error_message);
  
  return error_response;
}

int getResponseHeader(string& data, bool& inHeader, bool& complete, int hostSock, int& content_length)
{	
  int bytesWritten;
  size_t n = 0;
  char databuffer[2048];
  bytesWritten = recv(hostSock,databuffer,sizeof(databuffer),0);
  if (bytesWritten < 0){
    return -1;
  }
  
  data.append(databuffer,bytesWritten);
  
  //end of header
  if((n = data.find("\r\n\r\n")) != string::npos)
  {
    
    inHeader = false;
    // get body
    string part_body = data.substr(n+4, data.length()-n-4);
    // this is the header
    data = data.substr(0, n+4);
    
    HttpResponse temp_response;
    temp_response.ParseResponse(data.c_str(), data.length());
    content_length = atoi(temp_response.FindHeader("Content-Length").c_str());
    
    int part_body_length = part_body.length();			
    // done
    if(part_body_length >= content_length)
     {
       data += part_body.substr(0, content_length);	
       complete = true;				
     }
    // not done
    else
    {	
      data += part_body;
      content_length -= part_body_length;	
    }
  }
  
  return 0;
}


int getResponseData(string& data, bool& inHeader, bool& complete, int hostSock, int& content_length)
{	
  int bytesWritten;
  char databuffer[2048];
  bytesWritten = recv(hostSock,databuffer,sizeof(databuffer),0);
  if (bytesWritten < 0){
    fprintf(stderr,"error: %d\n",errno);
    return -1;
  }		
  
  if(bytesWritten >= content_length)
  {
    data.append(databuffer, content_length);
    complete = true;
    return 0;
  }
  
  data.append(databuffer,bytesWritten);
  content_length -= bytesWritten;
  
  return 0;
}


int createSocketAndConnect(string host, unsigned short port){
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;
  char portString[10];

  // we need the port to be a string to use
  sprintf(portString, "%u", port);  
  /* Obtain address(es) matching host/port */
  
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;  
  hints.ai_socktype = SOCK_STREAM;
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
    
    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1){
      break;                  /* Success */
    }
    
    close(sfd);
  }
  
  if (rp == NULL) {               /* No address succeeded */
    fprintf(stderr, "Could not connect\n");
    return -1;
  }
  freeaddrinfo(result);           /* No longer needed */
  return sfd;

}

void* socketConnection( void* parameters){

  param_t *p = (param_t *) parameters;
  //const char *sendbuf = "this is a test";
  //send(p->sockfd, sendbuf, (int)strlen(sendbuf), 0);

  //int recvbuflen = DEFAULT_BUFLEN-1;
  struct timeval tv;
  fd_set readfds;
  int n, rv;
  map<string, int> hostConnections;

  //this loops to maintain a persistent connection, using select to listen to the 
  //client during each iteration.
  while(true)
  {
    //persistent connection:  continue to listen to the socket at all times.
    // clear the set ahead of time
    FD_ZERO(&readfds);

    // add our descriptors to the set (just the client socket)
    FD_SET(p->sockfd, &readfds);

    n = p->sockfd + 1;
    tv.tv_sec = 10;
    tv.tv_usec = 500000;
    rv = select(n, &readfds, NULL, NULL, &tv);

    string rv_data;
    char recvbuf[DEFAULT_BUFLEN];
    memset(&recvbuf, 0, sizeof(recvbuf));
    if (rv == -1) {
      perror("select"); // error occurred in select()
    } else if (rv == 0) {
      //timed out, just loop and try again.
    } else {
      // one or both of the descriptors have data
      if (FD_ISSET(p->sockfd, &readfds)) {
	
	while((rv = recv(p->sockfd, recvbuf, sizeof(recvbuf), 0)) > 0){
	  rv_data.append(recvbuf, rv);
	  
	  if((rv_data.find("\r\n\r\n")) != string::npos)
	    break;
	  memset(&recvbuf, 0, sizeof(recvbuf));
	  
	}
	if (rv < 0){
	  fprintf(stderr, "recv failed\n");
	  return NULL;
	} else if (rv == 0)
	  {
	    break;
	  }
	
	
	HttpRequest myRequest;
	try{
	  myRequest.ParseRequest(rv_data.c_str(), rv_data.length());
	}catch(ParseException e){
	  //error 400 if "Bad Request". (send to client)
	  //error 501 if "Not Implemented"
	  HttpResponse error_response;
	  if(string(e.what()).find("GET") != string::npos)
	    error_response = createErrorMessage("501", "Not Implemented");
	  else
	    error_response = createErrorMessage("400", "Bad Request");
	  char response_buffer[error_response.GetTotalLength()+1];
	  error_response.FormatResponse(response_buffer);
	  response_buffer[error_response.GetTotalLength()] = '\0';
	  try{
	    send(p->sockfd, response_buffer, strlen(response_buffer), 0);
	  }
	  catch(exception e)
	  {
	    fprintf(stderr, "failed sending error message\n");
	  }
	  
	  continue;
	}
	//DONE: GetHost and GetPort don't throw exceptions
	//look at the parserequest code and see if it also 
	//throws a unsupported method exception and catch that and 
	//send whatever error tho the client.
	string host = myRequest.GetHost();
	unsigned short port = myRequest.GetPort();
	if (port==0){
	  port = 80;
	  myRequest.SetPort(port);
	}
	
	
	// Format request
	size_t req_length = myRequest.GetTotalLength() + 1;
	char *request = (char *)malloc(req_length);
	myRequest.FormatRequest(request);
	request[req_length-1] = '\0';

	// Check if in cache already
	string data;
	string path = host + myRequest.GetPath();
	cache_t::iterator it = cache.find(path);
	bool needToFetch = true;
	string last_modified;
	string dataInCache;
	string dataToSend;
	
	if(it != cache.end()){
	  // Found in cache
	  
	  // Get current time
	  time_t now = time(0);
	  struct tm tstruct = *gmtime( &now );
	  char today[80];
	  strftime(today, sizeof(today), "%Y-%m-%d %X", &tstruct);
	  
	  cache_data cd = it->second;
	  string exp = cd.expired;
	  last_modified = cd.last_modified;
	  dataInCache = cd.data;
	  
	  // Check if expired 
	  if (exp != "" && today > exp){
	    // Expired, need to refetch, delete old entry in cache
	    
	    
	    // Add If-Modified-Since header to request
	    myRequest.AddHeader("If-Modified-Since",last_modified);
	    req_length = myRequest.GetTotalLength() + 1;
	    request = (char *)malloc(req_length);
	    myRequest.FormatRequest(request);
	  }
	  else {
	    // Not expired, no need to check if stale, just send back to client
	    data = dataInCache;
	    needToFetch =false;
	  }
	  
	}
	if(needToFetch) {
	  // not in cache

	  char portStr[10];
	  sprintf(portStr, "%u", port);  
	  string hostPort = host + portStr;
	  map<string, int>::iterator i = hostConnections.find(hostPort);
	  int hostSock;

	  if(i != hostConnections.end())
	  {
	    //this means we already have a host connection.
	    hostSock = i->second;
	  }
	  else
	  {
	    pthread_mutex_lock(&hostConn_lock);
	    if(count_hostConnections >= MAX_HOST_CONNECTIONS)
	      continue; //just ignore the request and keep doing your thing.
	    else
	      count_hostConnections++;
	    hostSock = createSocketAndConnect(host, port);
	    hostConnections[hostPort] = hostSock;
	    
	    pthread_mutex_unlock(&hostConn_lock);
	  }
	  
	  HttpResponse the_response;
	  // getResponse
	  
	  // Send request to remote host
	  try{
	    send(hostSock,request,req_length,0);
	  }catch(exception e){
	    // error
	    fprintf(stderr,"could not send request to remote\n");
	    free(request);
	    //close(hostSock);
	    //return NULL;
	    break;
	  }
	  // Get data from remote host
	  // Loop until we get all the data
	  int content_length;	
	  bool inHeader = true;
	  bool complete = false;
	  
	  // get header
	  while(inHeader)
	   {
	     if(getResponseHeader(data, inHeader, complete, hostSock, content_length) < 0)
	       return NULL;
	   }
	  
	  //get body
	  while(!inHeader && !complete)
	  {
	    if(getResponseData(data, inHeader, complete, hostSock, content_length) < 0)
	      return NULL;
	  }

	} // end else (not in cache)
	
	// Get response so we can parse it for headers and status
	HttpResponse temp_response;
	temp_response.ParseResponse(data.c_str(), data.length());
	string resp_status = temp_response.GetStatusCode();
	
	// Check if we are doing a CONDITIONAL GET
	if(atoi(resp_status.c_str()) == 304){
	  // Not modified, send to client what we have in cache
	  // No need to add anything to cache
	  dataToSend = dataInCache;
	  
	  // Update Expires field
	  if(it != cache.end()){
	    string expdate = parseDate("Expires",temp_response);
	    //string expdate = temp_response.FindHeader("Expires");
	    
	    it->second.expired = expdate.c_str();
	  }
	}
	else{
	  dataToSend = data;
	  
	  // Delete old entry, if applicable
	  if(it != cache.end()){
	    // Delete old entry
	    pthread_mutex_lock(&cache_lock);
	    cache.erase(it);
	    pthread_mutex_unlock(&cache_lock);
	  }
	  
	  // Parse response for info
	  string expdate = parseDate("Expires",temp_response);
	  //string lm = parseDate("Last-Modified",temp_response);
	  string lm = temp_response.FindHeader("Last-Modified");
	  
	  // Create new entry
	  cache_data thiscache;
	  thiscache.expired = expdate.c_str();
	  thiscache.last_modified = lm.c_str();
	  thiscache.data = data;
	  
	  // Insert into cache
	  pthread_mutex_lock(&cache_lock);
	  cache.insert(pair<string,cache_data>(path,thiscache));
	  pthread_mutex_unlock(&cache_lock);
	}
	
	// Send data back to client
	try{
	  send(p->sockfd,dataToSend.c_str(),strlen(dataToSend.c_str()),0);	
	}catch(ParseException e){
	  fprintf(stderr,"could not send data to client\n");
	  free(request);
	  return NULL;
	}
	
	
	free(request);
	
      }
      
    }
  }
  
  
  //update connection count
  close(p->sockfd);
  pthread_mutex_lock(&hostConn_lock);
  map<string, int>::iterator i;
  for(i = hostConnections.begin(); i != hostConnections.end(); i++)
  {
    close(i->second);
  }
  count_hostConnections -= hostConnections.size();
  pthread_mutex_unlock(&hostConn_lock);

  pthread_mutex_lock(&count_mutex);
  count_connections--;
  pthread_cond_signal(&count_cond);
  pthread_mutex_unlock(&count_mutex);
  pthread_exit(NULL);

  return NULL;
}

