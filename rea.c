#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h>
#include <sys/epoll.h>
#include "rea.h"

Server *server;
Client *clients[MAX_CLIENTS];
int epfd;

int main(int argc, char *argv[])
{
	int status, i, fd, added, nfds;
	struct epoll_event ev;
	struct epoll_event evs[MAX_EP_EVENTS];
	Client *c;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	setupAndListen(argv[1]);

	epfd = epoll_create1(0);
	if (epfd == -1) {
		err(EXIT_FAILURE, "epoll_create error");
	}

	ev.events = EPOLLIN;
	ev.data.ptr = server;

	status = epoll_ctl(epfd, EPOLL_CTL_ADD, server->fd, &ev);
	if (status == -1) {
		err(EXIT_FAILURE, "6epoll_ctl error");
	}

	while(1) {
		nfds = epoll_wait(epfd, evs, MAX_EP_EVENTS, -1);
		if (nfds == -1) {
			err(EXIT_FAILURE, "epoll_wait error");
		}

		for (i = 0; i < nfds; i++) {
			if (((Server *)evs[i].data.ptr)->fd == server->fd) {
				fd = accept4(server->fd, server->addr->ai_addr,
					&(server->addr->ai_addrlen), SOCK_NONBLOCK);
				if (fd < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						continue;
					}
					err(EXIT_FAILURE, "Socket accept error");
				}

				added = 0;
				for (i = 0; i < MAX_CLIENTS; i++) {
					if (clients[i] == 0) {
						clients[i] = makeClient(fd);
						added = 1;
						break;
					}
				}
				if (!added) {
					fprintf(stderr, "Could not find room for client fd: %d\n", fd);
				}

				ev.events = EPOLLIN;
				ev.data.ptr = clients[i];
				status = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
				if (status == -1) {
					err(EXIT_FAILURE, "2epoll_ctl error");
				}

				printf("Accepted connection! (fd: %d)\n", clients[i]->fd);
			} else {
				c = (Client *)evs[i].data.ptr;

				if (evs[i].events & EPOLLERR) {
					closeClient(c);
				} else if (evs[i].events & EPOLLIN) {
					if (c->replied) {
						closeClient(c);
					} else if (c->cstate == CONNECTED) {
						readRequest(c);
					}
				} else if ((evs[i].events & EPOLLOUT) && !(c->replied)) {
					respond(c);
				}
			}
		}
	}
}


void setupAndListen(char *port)
{
	int status, fd;
	struct addrinfo hints;
	struct addrinfo *ai;

	server = (Server *)malloc(sizeof(Server));
	if (!server) {
		fprintf(stderr, "Couldn't allocate memory for starting the server\n");
		exit(EXIT_FAILURE);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; /* Only IPv4 for now */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; /* Listen on all network addresses */

	if ((status = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	/*
	 * Normally only a single protocol exists to support a particular
	 * socket [type, protocol family] combination, so we can skip specifying it
	 */
	fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (fd < 0) {
		err(EXIT_FAILURE, "Socket creation error");
	}

	status = bind(fd, ai->ai_addr, ai->ai_addrlen);
	if (status != 0) {
		err(EXIT_FAILURE, "Socket bind error");
	}

	status = listen(fd, RECV_BACKLOG);
	if (status != 0) {
		err(EXIT_FAILURE, "Socket listen error");
	}

	server->fd = fd;
	server->addr = ai;

	setupSighandlers();

	printf("Listening on 0.0.0.0:%s ...\n", port);
}


void readRequest(Client *c)
{
	int nparsed, status;
	struct epoll_event ev;

	status = recv(c->fd, c->buf, RECV_BUFFER, 0);
	if (status < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return;
		} else {
			err(EXIT_FAILURE, "Message recv error (client: %d)\n", c->fd); }
	} else {
		nparsed = http_parser_execute(c->parser, c->parser_settings, c->buf, status);
		if (nparsed != status) {
			c->pstate = ERROR;
			ev.events = EPOLLOUT;
			ev.data.ptr = c;

			if (epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev) == -1) {
				err(EXIT_FAILURE, "3epoll_ctl error");
			}

			printf("Parse error (client %d): %s\n",
					c->fd, http_errno_description(HTTP_PARSER_ERRNO(c->parser)));
		}

		if (status == 0) {
			printf("Client %d closed the connection.\n", c->fd);
			c->cstate = DISCONNECTED;
			closeClient(c);
		}
	}
}


int on_message_complete_cb(http_parser *p)
{
	struct epoll_event ev;
	Client *c = p->data;

	c->pstate = SUCCESS;
	ev.events = EPOLLOUT;
	ev.data.ptr = c;

	if (epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev) == -1) {
		err(EXIT_FAILURE, "4epoll_ctl error");
	}

	return 0;
}



void respond(Client *c)
{
	int status;
	char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nCool\r\n\r\n";
	char *resp400 = "HTTP/1.1 400 Bad Request\r\n\r\n";

	if (c->pstate == ERROR) {
		status = send(c->fd, resp400, strlen(resp400), 0);
		if (status == -1) {
			err(EXIT_FAILURE, "send error (client: %d)", c->fd);
		}

		c->replied = 1;
		closeClient(c);
	} else if (c->pstate == SUCCESS) {
		status = send(c->fd, resp, strlen(resp), 0);
		if (status == -1) {
			err(EXIT_FAILURE, "send error (client: %d)", c->fd);
		}

		c->replied = 1;
		closeClient(c);
	}
}


Client *makeClient(int fd)
{
	http_parser_settings *settings = malloc(sizeof(http_parser_settings));
	http_parser *parser = malloc(sizeof(http_parser));

	http_parser_settings_init(settings);
	http_parser_init(parser, HTTP_REQUEST);

	settings->on_message_complete = on_message_complete_cb;

	Client *c = malloc(sizeof(Client));
	if (!c) {
		fprintf(stderr, "Couldn't allocate memory for connection %d\n", fd);
		exit(EXIT_FAILURE);
	}

	c->fd = fd;
	c->cstate = CONNECTED;
	c->pstate = IN_PROGRESS;
	c->replied = 0;
	memset(c->buf, 0, RECV_BUFFER);
	c->parser_settings = settings;
	c->parser = parser;
	c->parser->data = c;

	return c;
}


void closeClient(Client *c)
{
	int i, found, status;

	/* passing NULL only works in kernel versions 2.6.9+ */
	status = epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
	if (status == -1) {
		err(EXIT_FAILURE, "5nepoll_ctl error");
	}

	/* this also removes fd from the epoll set */
	if (close(c->fd) < 0) {
		err(EXIT_FAILURE, "close(2) error");
	}

	found = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i] && clients[i] == c) {
			clients[i] = 0;
			found = 1;
			break;
		}
	}
	if (found != 1) {
		err(EXIT_FAILURE, "Couldn't find client fd to close");
	}

	c->cstate = DISCONNECTED;
	free(c);
}


void setupSighandlers(void)
{
	struct sigaction act;
	act.sa_handler = shutdownServer;

	int status = sigaction(SIGINT, &act, NULL);
	if (status != 0) {
		err(EXIT_FAILURE, "Error setting up signal handler\n");
	}
}


void shutdownServer(int sig)
{
	printf("\nShutting down...\n");

	int status = close(server->fd);
	if (status != 0) {
		err(EXIT_FAILURE, "Socket cleanup error");
	}

	freeaddrinfo(server->addr);
	printf("Goodbye!\n");
	exit(EXIT_SUCCESS);
}
