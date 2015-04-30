#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>

#define SERV_PORT 9877
#define MAXLINE 4096

void str_cli(FILE *fp, int sockfd);

int main(int argc, char* argv[])
{
  int sockfd;
  struct sockaddr_in servaddr;

  if (argc != 2) {
    printf("Usage: %s <IPaddress>\n", argv[0]);
    return 0;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SERV_PORT);
  inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
  connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

  str_cli(stdin, sockfd);

  return 0;
}

void str_cli(FILE *fp, int sockfd)
{
  char sendline[MAXLINE], recvline[MAXLINE];
  while (fgets(sendline, MAXLINE, fp)!=NULL) {
    sendline[strlen(sendline)]='\0';
    write(sockfd, sendline, strlen(sendline)+1);

    if (read(sockfd, recvline, MAXLINE)==0)
    {
      printf("ERROR\n");
      return;
    }
    fputs(recvline, stdout);
    /* Send ACK */
    write(sockfd, "\x06", 1);
  }
}
