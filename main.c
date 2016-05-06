#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include "main.h"

int main(int argc, char *argv[]) {
  int status, sockfd, clientfd, nfds, i, fd, added;
  struct addrinfo *res;
  struct addrinfo hints;
  char buf[RECV_BUFFER] = {0};
  char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nCool\r\n\r\n";
  int clientfds[MAX_CLIENTS] = {0};
  fd_set rfds, wfds;
  struct clientCon *conns[MAX_CLIENTS];
  struct clientCon *con;

  memset(conns, 0, MAX_CLIENTS-1);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; /* Allow IPv4 for now */
  hints.ai_socktype = SOCK_STREAM; /* TCP socket */
  hints.ai_flags = AI_PASSIVE; /* For wildcard IP address */

  if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  /*
   * Normally only a single protocol exists to support a particular
   * socket [type, protocol family] combination, so we can skip specifying it
   */
  sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (sockfd < 0) {
    err(EXIT_FAILURE, "Socket creation error");
  }

  status = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (status != 0) {
    err(EXIT_FAILURE, "Socket bind error");
  }

  status = listen(sockfd, RECV_BACKLOG);
  if (status != 0) {
    err(EXIT_FAILURE, "Socket listen error");
  }

  printf("Listening on port %s ...\n", argv[1]);

  while(1) {
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(sockfd, &rfds);
    nfds = sockfd;

    for (i = 0; i < MAX_CLIENTS; i++) {
      if (conns[i]) {
        fd = conns[i]->fd;
        FD_SET(fd, &rfds);
        if (fd > nfds) {
          nfds = fd;
        }
      }
    }

    /* TODO: also add exception set */
    status = select(nfds+1, &rfds, &wfds, NULL, NULL);
    if (status < 0) {
      err(EXIT_FAILURE, "Socket select error");
    }

    if (FD_ISSET(sockfd, &rfds)) {
      clientfd = accept4(sockfd, res->ai_addr, &(res->ai_addrlen), SOCK_NONBLOCK);
      if (clientfd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        } else {
          printf("%d\n", clientfd);
          err(EXIT_FAILURE, "Socket accept error");
        }
      }

      added = 0;
      for (i = 0; i < MAX_CLIENTS; i++) {
        if (!conns[i]) {
          conns[i] = make_conn(clientfd);
          added = 1;
          if (clientfd > nfds) {
            nfds = clientfd;
          }
          break;
        }
      }

      if (!added) {
        fprintf(stderr, "Could not find room to monitor client fd: %d", clientfd);
        exit(EXIT_FAILURE);
      }

      FD_SET(clientfd, &rfds);

      printf("Accepted connection! (fd: %d)\n", clientfd);
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
      if (!conns[i]) {
        continue;
      }

      con = conns[i];

      if (FD_ISSET(con->fd, &rfds)) {
        /* TODO: We assume that we got the full message in a single read */
        status = recv(con->fd, &(con->buf), RECV_BUFFER-1, 0);

        if (status < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          err(EXIT_FAILURE, "Message recv error (client: %d)\n", con->fd);
        } else if (status == 0) {
          printf("Connection closed by client %d.\n", con->fd);
          close_client(con->fd, &rfds, &wfds, conns);
          break;
        } else if (status > 0) {
          con->buf[RECV_BUFFER-1] = '\0';
          printf("Message from client %d: %.*s\n", con->fd, status+1, con->buf);
          FD_SET(con->fd, &wfds);
        }
      }

      if (FD_ISSET(con->fd, &wfds)) {
        send(con->fd, resp, strlen(resp), 0);
        close_client(con->fd, &rfds, &wfds, conns);
      }
    }
  }

  status = close(sockfd);
  if (status != 0) {
    err(EXIT_FAILURE, "Socket cleanup error");
  }

  freeaddrinfo(res);
}

void close_client(int fd, fd_set *rfds, fd_set *wfds, struct clientCon *conns[]) {
  int i;

  if (close(fd) < 0) {
    fprintf(stderr, "Client %d close error\n", fd);
    exit(EXIT_FAILURE);
  }

  FD_CLR(fd, rfds);
  FD_CLR(fd, wfds);
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (conns[i] && conns[i]->fd == fd) {
      conns[i] = 0;
      //TODO: free mem.
    }
  }
}

// TODO: check that fd > 0
struct clientCon *make_conn(int fd) {
  int i;
  struct clientCon *c = (struct clientCon *)malloc(sizeof(struct clientCon));
  if (!c) {
    fprintf(stderr, "Couldn't allocate memory for connection %d\n", fd);
    exit(EXIT_FAILURE);
  }

  c->fd = fd;

  for (i = 0; i < RECV_BUFFER; i++) {
    c->buf[i] = 0;
  }

  return c;
}
