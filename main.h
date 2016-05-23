#ifndef REA_MAIN_H
#define REA_MAIN_H

#define MAX_CLIENTS 4096
#define RECV_BUFFER 4096

/*
 * Max. limit of queued connections passed to listen(2).
 * After that the client _may_ receive ECONNREFUSED.
 * This is just a hint though.
 */
#define RECV_BACKLOG 50

#include "http_parser.h"

/* Read/write file descriptor sets for select(2) */
extern fd_set rfds;
extern fd_set wfds;

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

	/* cstate represents the connection state of the client socket */
	enum { CONNECTED, DISCONNECTED } cstate;

	/* pstate represets the parse state of the client request */
	enum { IN_PROGRESS, ERROR, SUCCESS } pstate;

	// TODO: this smells; we should make it simpler.
	int to_reply;

	char buf[RECV_BUFFER];

	http_parser *parser;
	http_parser_settings *parser_settings;
};

/* clients contains all the active Clients */
extern struct Client *clients[MAX_CLIENTS];

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
 * Callback executed when a request is fully parsed successfully.
 */
int on_message_complete_cb(http_parser *p);

/*
 * Reads bytes from a connected Client and parses them.
 */
void read_response(struct Client *c);

/*
 * Writes the response back to the client.
 */
void respond(struct Client *c);

/*
 * make_client Initializes a new Client from the given file descriptor and
 * returns a pointer to it.
 */
struct Client *make_client(int fd);

/*
 * close_client closes the socket of a connected client and performs some
 * bookkeeping work.
 */
void close_client(int fd);

#endif /* REA_MAIN_H */
