#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <netdb.h>
#include "main.h"

int main(int argc, char *argv[]) {
	int status, maxfd, i, fd, added;
	char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nCool\r\n\r\n";
	fd_set rfds, wfds;
	struct Client *clients[MAX_CLIENTS];

	memset(clients, 0, sizeof(struct Client*)*MAX_CLIENTS);

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	setup_and_listen(argv[1]);
	setup_sighandlers();

	while(1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(server->fd, &rfds);
		maxfd = server->fd;

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i]) {
				fd = clients[i]->fd;
				FD_SET(fd, &rfds);
				if (fd > maxfd) {
					maxfd = fd;
				}
			}
		}

		/* TODO: also add exception set */
		status = select(maxfd+1, &rfds, &wfds, NULL, NULL);
		if (status < 0) {
			err(EXIT_FAILURE, "Socket select error");
		}

		if (FD_ISSET(server->fd, &rfds)) {
			fd = accept4(server->fd, server->addr->ai_addr, &(server->addr->ai_addrlen), SOCK_NONBLOCK);
			if (fd < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				printf("%d\n", fd);
				err(EXIT_FAILURE, "Socket accept error");
			}

			added = 0;
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (!clients[i]) {
					clients[i] = make_client(fd);
					added = 1;
					if (fd > maxfd) {
						maxfd = fd;
					}
					break;
				}
			}

			if (!added) {
				fprintf(stderr, "Could not find room for client fd: %d\n", fd);
				exit(EXIT_FAILURE);
			}

			FD_SET(fd, &rfds);

			printf("Accepted connection! (fd: %d)\n", fd);
		}

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (!clients[i]) {
				continue;
			}

			fd = clients[i]->fd;

			if (FD_ISSET(fd, &rfds)) {
				/* TODO: We assume that we got the full message in a single read */
				status = recv(fd, &(clients[i]->buf), RECV_BUFFER-1, 0);

				if (status < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
					err(EXIT_FAILURE, "Message recv error (client: %d)\n", fd);
				} else if (status == 0) {
					printf("Connection closed by client %d.\n", fd);
					close_client(fd, &rfds, &wfds, clients);
					break;
				} else if (status > 0) {
					clients[i]->buf[RECV_BUFFER-1] = '\0';
					printf("Message from client %d: %.*s\n", fd, status+1, clients[i]->buf);
					FD_SET(fd, &wfds);
				}
			}

			if (FD_ISSET(fd, &wfds)) {
				send(fd, resp, strlen(resp), 0);
				close_client(fd, &rfds, &wfds, clients);
			}
		}
	}
}

void setup_and_listen(char *port) {
	int status, fd;
	struct addrinfo hints;
	struct addrinfo *ai;

	server = (struct Server *)malloc(sizeof(struct Server));
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

	printf("Listening on 0.0.0.0:%s ...\n", port);
}

struct Client * make_client(int fd) {
	int i;

	struct Client *c = (struct Client *)malloc(sizeof(struct Client));
	if (!c) {
		fprintf(stderr, "Couldn't allocate memory for connection %d\n", fd);
		exit(EXIT_FAILURE);
	}

	c->fd = fd;
	memset(c->buf, 0, RECV_BUFFER);

	return c;
}

void close_client(int fd, fd_set *rfds, fd_set *wfds, struct Client *clients[]) {
	int i;

	if (close(fd) < 0) {
		fprintf(stderr, "Client %d close error\n", fd);
		exit(EXIT_FAILURE);
	}

	FD_CLR(fd, rfds);
	FD_CLR(fd, wfds);

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i] && clients[i]->fd == fd) {
			free(clients[i]);
			clients[i] = 0;
			return;
		}
	}
}

void setup_sighandlers(void) {
	struct sigaction act;
	act.sa_handler = shutdown_server;

	int status = sigaction(SIGINT, &act, NULL);
	if (status != 0) {
		err(EXIT_FAILURE, "Error setting up signal handler\n");
	}
}

void shutdown_server(int sig) {
	printf("\nShutting down...\n");

	int status = close(server->fd);
	if (status != 0) {
		err(EXIT_FAILURE, "Socket cleanup error");
	}

	freeaddrinfo(server->addr);
	printf("Goodbye!\n");
	exit(EXIT_SUCCESS);
}
