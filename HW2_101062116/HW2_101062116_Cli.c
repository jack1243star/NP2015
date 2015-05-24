#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PACKETSIZE 1024
#define ACKSIZE 1

#define IDOFFSET 32
#define PWOFFSET 64
#define MAXIDLEN 16
#define MAXPWLEN 16

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

void client(void);
void alnum_prefix(char *str);
int rawsend(void *buf, size_t size);
int rawrecv(void *buf, size_t size);
void acksend(void *buf, size_t size);
void ackrecv(void *buf, size_t size);

int sockfd;
struct sockaddr_in servaddr;

int main(int argc, char* argv[])
{
  /* Check command line argument */
  if (argc!=3) {
    printf("Usage: %s <IPaddress> <Port>\n", argv[0]);
    return 0;
  }

  /* Create UDP socket */
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  /* Initialize address structure */
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
  servaddr.sin_port = htons(atoi(argv[2]));

  /* Start client */
  client();

  return EXIT_SUCCESS;
}

void client(void)
{
  char buf[PACKETSIZE];

  printf("*** Welcome ***\n");
welcome:
  printf("[R]egister [L]ogin\n");
  fgets(buf, 16, stdin);
  switch(buf[0]) {
  case 'R': /* Register */
  case 'r':
    memset(buf, 0, PACKETSIZE);
    buf[0] = OP_NEWUSER;

    printf("ID(1-20 characters):");
    fgets(buf+IDOFFSET, MAXIDLEN, stdin);
    alnum_prefix(buf+IDOFFSET);

    printf("PW(1-20 characters):");
    fgets(buf+PWOFFSET, MAXPWLEN, stdin);
    alnum_prefix(buf+PWOFFSET);

    acksend(buf, PACKETSIZE);
    break;
  case 'L': /* Login */
  case 'l':
    memset(buf, 0, PACKETSIZE);
    buf[0] = OP_LOGIN;

    printf("ID(1-20 characters):");
    fgets(buf+IDOFFSET, MAXIDLEN, stdin);
    alnum_prefix(buf+IDOFFSET);

    printf("PW(1-20 characters):");
    fgets(buf+PWOFFSET, MAXPWLEN, stdin);
    alnum_prefix(buf+PWOFFSET);

    acksend(buf, PACKETSIZE);
    ackrecv(buf, PACKETSIZE);

    if (buf[0] == 1) {
      printf("*** Hello %s ***\n", buf+IDOFFSET);
      goto main;
    } else {
      printf("%s", buf+1);
    }
    break;
  default:
    break;
  }
  goto welcome;

main:
  printf(
    "[SU]Show User [SA]Show Article [A]dd Article [E]nter Article\n"
    "[Y]ell [T]ell [L]ogout\n"
    );
  fgets(buf, 16, stdin);
  switch(buf[0]) {
  case 'S':
  case 's':
    switch(buf[1]) {
    case 'U': /* Show user */
    case 'u':
      buf[0] = OP_SHOWUSER;
      acksend(buf, PACKETSIZE);
      for (;;) {
        ackrecv(buf, PACKETSIZE);
        if (buf[0] == 1)
          printf("%s\n", buf+1);
        else
          break;
      }
      break;

    case 'A': /* Show Article */
    case 'a':
      break;

    default:
      break;
    }
    break;

  default:
    break;
  }
  goto main;  
}

void alnum_prefix(char* str)
{
  unsigned int i = 0;
  while (isalnum(str[i]))
    i++;
  str[i] = '\0';
}

int rawsend(void *buf, size_t size)
{
  return sendto(sockfd, buf, size,
                0, (struct sockaddr *)(&servaddr), sizeof(servaddr));
}

int rawrecv(void *buf, size_t size)
{
  return recvfrom(sockfd, buf, size, 0, NULL, NULL);
}

void acksend(void *buf, size_t size)
{
  int n;
  char ackbuf[ACKSIZE];
  struct timeval tv;

  /* Set recv timeout */
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv, sizeof(tv)) < 0) {
    perror("Error");
  }

  for (;;) {
    rawsend(buf, size);
    n = rawrecv(ackbuf, ACKSIZE);
    if (n > 0)
      break;
  }
}

void ackrecv(void *buf, size_t size)
{
  char ack = '\x06';
  struct timeval tv;

  /* Set recv timeout */
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv, sizeof(tv)) < 0) {
    perror("Error");
  }

  rawrecv(buf, size);
  rawsend(&ack, ACKSIZE);
}
