#ifndef REA_MAIN_H
#define REA_MAIN_H

#define MAX_CLIENTS 4096
#define RECV_BUFFER 4096

/*
 * Max. limit of queued connections passed to listen(2).
 * After that the client _may_ * receive ECONNREFUSED.
 * This is just a hint though.
 */
#define RECV_BACKLOG 50

struct Server {
	/* the file descriptor of the listening socket */
	int fd;

	/* the address we will bind the listening socket to */
	struct addrinfo *addr;
};

/* server is the global Server instance */
struct Server *server;

/* Client represents a connection of an active client. */
struct Client {
	int fd;
	int nread; /* Number of bytes read from the socket */
	char buf[RECV_BUFFER];
};

/*
 * Creates a Server on a listening socket, binds it to the specified port and
 * sets its to the server global variable
 */
void setup_and_listen(char *port);

/*
 * Registers signal handlers for shutting down the server gracefully etc.
 */
void setup_sighandlers(void);

/*
 * Shuts down the server gracefully
 */
void shutdown_server(int sig);

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
