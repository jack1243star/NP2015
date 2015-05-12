#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define MAXLINE 128

void dg_echo(int sockfd, struct sockaddr *pcliaddr, socklen_t clilen);

int main(int argc, char* argv[])
{
  int sockfd;
  short servport;
  struct sockaddr_in servaddr, cliaddr;

  if (argc!=2) {
    printf("Usage: %s <Port>\n", argv[0]);
    return 0;
  }

  servport = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(servport);

  bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

  dg_echo(sockfd, (struct sockaddr*)&cliaddr, sizeof(cliaddr));

  return 0;
}

void dg_echo(int sockfd, struct sockaddr *pcliaddr, socklen_t clilen)
{
  int n;
  socklen_t len;
  char mesg[MAXLINE];

  for(;;) {
    len = clilen;
    n = recvfrom(sockfd, mesg, MAXLINE, 0, pcliaddr, &len);
    printf("Receive from IP=%s PORT=%d\n",
	   inet_ntoa(((struct sockaddr_in*)pcliaddr)->sin_addr),
	   htons(((struct sockaddr_in*)pcliaddr)->sin_port));

    sendto(sockfd, mesg, n, 0, pcliaddr, len);
  }
}
