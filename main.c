#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <err.h>
#include <netdb.h>

#define MAX_CLIENTS 10
#define RECV_BUFFER 3000

int main(int argc, char *argv[]) {
  int status, sockfd, clientfd;
  struct addrinfo *res;
  struct addrinfo hints;
  char buf[RECV_BUFFER] = {0};
  char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; /* Allow IPv4 for now */
  hints.ai_socktype = SOCK_STREAM; /* TCP socket */
  hints.ai_flags = AI_PASSIVE; /* For wildcard IP address */
  // TODO: what if we don't do PASSIVE?

  if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  /*
   * Normally only a single protocol exists to support a particular
   * socket [type, protocol family] combination, so we can specify this as 0
   */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
    clientfd = accept(sockfd, res->ai_addr, &(res->ai_addrlen));
    if (clientfd < 0) {
      err(EXIT_FAILURE, "Socket accept error");
    }
    printf("Accepted connection! (fd: %d)\n", clientfd);

    // MOVE TO FORK OR SOMETHNG
    while(1) {
      status = recv(clientfd, &buf, RECV_BUFFER-1, 0);
      if (status < 0) {
        err(EXIT_FAILURE, "Message recv error");
      } else if (status == 0) {
        printf("Connection closed by the client.\n");
        close(clientfd);
        break;
      } else {
        buf[RECV_BUFFER-1] = '\0';
        printf("%.*s", status+1, buf);
        send(clientfd, resp, strlen(resp), 0);
      }
    }
  }

  status = close(sockfd);
  if (status != 0) {
    err(EXIT_FAILURE, "Socket cleanup error");
  }

  freeaddrinfo(res);
}
