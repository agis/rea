#ifndef REA_MAIN_H
#define REA_MAIN_H

#define MAX_CLIENTS 8000
#define RECV_BUFFER 4096
#define MAX_EP_EVENTS 10240

/*
 * Max. limit of queued connections passed to listen(2).
 * After that the client _may_ receive ECONNREFUSED.
 * This is just a hint though.
 */
#define RECV_BACKLOG 50

#include "http_parser.h"


/* epoll instance fd */
extern int epfd;


/*
 * Server represents the listening server.
 */
typedef struct Server {
	/* the file descriptor of the listening socket */
	int fd;

	/* the address we will bind the listening socket to */
	struct addrinfo *addr;
} Server;


/*
 * server points to a global Server instance.
 */
extern Server *server;


/*
 * Client represents a connection of an active client.
 */
typedef struct Client {
	int fd;

	/* cstate represents the connection state of the client socket */
	enum { CONNECTED, DISCONNECTED } cstate;

	/* pstate represets the parse state of the client request */
	enum { IN_PROGRESS, ERROR, SUCCESS } pstate;

	char buf[RECV_BUFFER];

	http_parser *parser;
	http_parser_settings *parser_settings;
	int replied;

} Client;


/* clients contains all the active Clients */
extern Client *clients[MAX_CLIENTS];


/*
 * Creates a Server on a listening socket, binds it to the specified port and
 * sets its to the server global variable.
 */
void setupAndListen(char *port);


/*
 * Registers signal handlers for shutting down the server gracefully etc.
 */
void setupSighandlers(void);


/*
 * Shuts down the server gracefully.
 */
void shutdownServer(int sig);


/*
 * Callback executed when a request is fully parsed successfully.
 */
int on_message_complete_cb(http_parser *p);


/*
 * Reads bytes from a connected Client and parses them.
 */
void readRequest(Client *c);


/*
 * Writes the response back to the client.
 */
void respond(Client *c);


/*
 * makeClient initializes a new Client from the given file descriptor and
 * returns a pointer to it.
 */
Client *makeClient(int fd);


/*
 * Closes the socket of a connected client and performs some
 * bookkeeping.
 */
void closeClient(Client *c);

#endif /* REA_MAIN_H */
