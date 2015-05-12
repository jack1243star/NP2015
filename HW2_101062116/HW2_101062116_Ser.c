#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define MAXLINE 128
#define IDOFFSET 1
#define MAXIDLEN 20
#define PWOFFSET 64
#define MAXPWLEN 20

#define OP_NEWUSER   1
#define OP_LOGIN     2
#define OP_SHOWUSER  3
#define OP_SHOWLIST  4
#define OP_ADDTEXT   5
#define OP_SHOWTEXT  6
#define OP_YELL      7
#define OP_TELL      8
#define OP_LOGOUT    9
#define OP_RESPONSE 10
#define OP_DOWNLOAD 11
#define OP_UPLOAD   12
#define OP_ADDBLIST 13
#define OP_DELBLIST 14
#define OP_DELTEXT  15
#define OP_DELUSER  16

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

  len = clilen;
  for(;;) {
    n = recvfrom(sockfd, mesg, MAXLINE, 0, pcliaddr, &len);
    switch(mesg[0]) {
    case OP_NEWUSER: /* Register */
      printf("IP=%s PORT=%d OP_NEWUSER ID=%s PW=%s",
	     inet_ntoa(((struct sockaddr_in*)pcliaddr)->sin_addr),
	     htons(((struct sockaddr_in*)pcliaddr)->sin_port),
	     mesg+IDOFFSET, mesg+PWOFFSET);
      sendto(sockfd, mesg, MAXLINE, 0, pcliaddr, clilen);
      break;
    case OP_LOGIN: /* Register */
      printf("IP=%s PORT=%d OP_LOGIN ID=%s PW=%s",
	     inet_ntoa(((struct sockaddr_in*)pcliaddr)->sin_addr),
	     htons(((struct sockaddr_in*)pcliaddr)->sin_port),
	     mesg+IDOFFSET, mesg+PWOFFSET);
      sendto(sockfd, mesg, MAXLINE, 0, pcliaddr, clilen);
      break;
    default:
      printf("Receive from IP=%s PORT=%d\n",
	     inet_ntoa(((struct sockaddr_in*)pcliaddr)->sin_addr),
	     htons(((struct sockaddr_in*)pcliaddr)->sin_port));
      sendto(sockfd, mesg, MAXLINE, 0, pcliaddr, clilen);
      break;
    }
  }
}
