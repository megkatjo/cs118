const int MAX_CONNECTIONS = 10;
const int BACKLOG = 10;

typedef struct {
  int sockfd;
  struct sockaddr client_addr;
  socklen_t sin_size;
} param_t;

void socketConnection((void *) paramaters);
