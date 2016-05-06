#ifndef REA_MAIN_H
#define REA_MAIN_H

#define MAX_CLIENTS 4096
#define RECV_BUFFER 4096
#define RECV_BACKLOG 50

/* Client represents a connection of an active client. */
struct Client {
	int fd;
	int nread; /* Number of bytes read from the socket */
	char buf[RECV_BUFFER];
};

/*
 * make_client Initializes a new Client from the given file descriptor and
 * returns a pointer to it.
 */
struct Client * make_client(int fd);

/*
 * close_client closes the socket of a connected client and performs some
 * bookkeeping work.
 */
void close_client(int fd, fd_set *rfds, fd_set *wfds, struct Client *conns[]);

#endif /* REA_MAIN_H */
