#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAXLINE 128

void dg_cli(FILE *fp, int sockfd, struct sockaddr *pservaddr, socklen_t servlen);

int main(int argc, char* argv[])
{
  int sockfd;
  short servport;
  struct sockaddr_in servaddr;

  if (argc!=3) {
    printf("Usage: %s <IPaddress> <Port>\n", argv[0]);
    return 0;
  }

  servport = atoi(argv[2]);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(servport);
  inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  dg_cli(stdin, sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  return 0;
}

void dg_cli(FILE *fp, int sockfd, struct sockaddr *pservaddr, socklen_t servlen)
{
  int n;
  char sendline[MAXLINE], recvline[MAXLINE+1];

  while(fgets(sendline, MAXLINE, fp)!=NULL){
    sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen);

    n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);

    recvline[n] = 0;
    fputs(recvline, stdout);
  }
}
