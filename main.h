#ifndef POWI
#define POWI

#define MAX_CLIENTS 4096
#define RECV_BUFFER 4096
#define RECV_BACKLOG 50 /* Max number of incoming connections to queue */

/* clientCon holds state about the connection of a client */
struct clientCon {
  int fd;
  int nread; /* Number of bytes read from the socket */
  char buf[RECV_BUFFER];
};

struct clientCon *make_conn(int fd);

void close_client(int fd, fd_set *rfds, fd_set *wfds, struct clientCon *conns[]);


#endif
