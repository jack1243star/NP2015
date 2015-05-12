#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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

welcome:
  fputs("[R]egister [L]ogin\n", stdout);
  fgets(sendline, MAXLINE, fp);
  switch(sendline[0]) {
  case 'R': /* Register */
  case 'r':
    memset(sendline, 0, MAXLINE);
    sendline[0] = OP_NEWUSER;

    printf("ID(1-20 characters):");
    fgets(sendline+IDOFFSET, MAXIDLEN, fp);

    printf("PW(1-20 characters):");
    fgets(sendline+PWOFFSET, MAXPWLEN, fp);

    sendto(sockfd, sendline, MAXLINE, 0, pservaddr, servlen);
    n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
    break;
  case 'L': /* Login */
  case 'l':
    memset(sendline, 0, MAXLINE);
    sendline[0] = OP_LOGIN;

    printf("ID(1-20 characters):");
    fgets(sendline+IDOFFSET, MAXIDLEN, fp);

    printf("PW(1-20 characters):");
    fgets(sendline+PWOFFSET, MAXPWLEN, fp);

    sendto(sockfd, sendline, MAXLINE, 0, pservaddr, servlen);
    n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
    break;
  default:
    break;
  }

  goto welcome;
}
