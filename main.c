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
#include <sys/fcntl.h>

#define MAX_CLIENTS 20
#define RECV_BUFFER 3000

void close_client(int fd, fd_set *rfds, fd_set *wfds, int clientfds[], int clen);

int main(int argc, char *argv[]) {
  int status, sockfd, clientfd, nfds, i, fd, added;
  struct addrinfo *res;
  struct addrinfo hints;
  char buf[RECV_BUFFER] = {0};
  char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nCool\r\n\r\n";
  int clientfds[MAX_CLIENTS] = {0};
  fd_set rfds, wfds;

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

  status = listen(sockfd, MAX_CLIENTS);
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
      if ((fd = clientfds[i]) > 0) {
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
        printf("%d\n", clientfd);
        err(EXIT_FAILURE, "Socket accept error");
      }

      added = 0;
      for (i = 0; i < MAX_CLIENTS; i++) {
        if (clientfds[i] == 0) {
          clientfds[i] = clientfd;
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
      fd = clientfds[i];
      if (fd == 0) {
        continue;
      }

      if (FD_ISSET(fd, &rfds)) {
        /* TODO: We assume that we got the full message in a single read */
        status = recv(fd, &buf, RECV_BUFFER-1, 0);

        if (status < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          err(EXIT_FAILURE, "Message recv error (client: %d)\n", fd);
        } else if (status == 0) {
          printf("Connection closed by client %d.\n", fd);
          close_client(fd, &rfds, &wfds, clientfds, MAX_CLIENTS);
        } else if (status > 0) {
          buf[RECV_BUFFER-1] = '\0';
          printf("Message from client %d: %.*s", fd, status+1, buf);
          FD_SET(fd, &wfds);
        }
      }

      if (FD_ISSET(fd, &wfds)) {
        send(fd, resp, strlen(resp), 0);
        close_client(fd, &rfds, &wfds, clientfds, MAX_CLIENTS);
      }
    }
  }

  status = close(sockfd);
  if (status != 0) {
    err(EXIT_FAILURE, "Socket cleanup error");
  }

  freeaddrinfo(res);
}

void close_client(int fd, fd_set *rfds, fd_set *wfds, int clientfds[], int clen) {
  int i;

  if (close(fd) < 0) {
    fprintf(stderr, "Client %d close error\n", fd);
    exit(EXIT_FAILURE);
  }

  FD_CLR(fd, rfds);
  FD_CLR(fd, wfds);
  for (i = 0; i < clen; i++) {
    if (clientfds[i] == fd) {
      clientfds[i] = 0;
    }
  }
}
