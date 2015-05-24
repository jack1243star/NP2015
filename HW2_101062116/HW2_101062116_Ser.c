#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PACKETSIZE 1024
#define ACKSIZE 1

#define IDOFFSET 32
#define PWOFFSET 64
#define MAXIDLEN 16
#define MAXPWLEN 16

#define MAXUSER 16

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

struct user {
  char id[16];
  char pw[16];
  int online;
};

void server(void);
int rawsend(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len);
int rawrecv(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len);
void acksend(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len);
void ackrecv(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len);

int sockfd;
struct sockaddr_in servaddr;

int main(int argc, char* argv[])
{
  /* Check command line argument */
  if (argc!=2) {
    printf("Usage: %s <Port>\n", argv[0]);
    return 0;
  }

  /* Create UDP socket */
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  /* Initialize address structure */
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(atoi(argv[1]));

  /* Bind to address */
  bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

  /* Start server */
  server();

  return EXIT_SUCCESS;
}

void server(void)
{
  int i;
  char buf[PACKETSIZE];
  struct sockaddr_in cliaddr;
  struct user users[MAXUSER];

  for (i=0; i<MAXUSER; i++) {
    memset(users[i].id, 0, MAXIDLEN);
    memset(users[i].pw, 0, MAXPWLEN);
    users[i].online = 0;
  }

  for (;;) {
    ackrecv(buf, PACKETSIZE, (struct sockaddr *)(&cliaddr), sizeof(cliaddr));
    printf("Receive from IP=%s PORT=%d\n",
           inet_ntoa(cliaddr.sin_addr),
           htons(cliaddr.sin_port));
    switch (buf[0]) {
    case OP_NEWUSER:
      for (i=0; i<MAXUSER; i++) {
        if (users[i].id[0] == 0) {
          strcpy(users[i].id, buf+IDOFFSET);
          strcpy(users[i].pw, buf+PWOFFSET);
          printf("Registered new user: %s\n", users[i].id);
          break;
        }
        if (i == MAXUSER-1)
          printf("Error: No more space for new user!\n");
      }
      break;
    case OP_LOGIN:
      printf("LOGIN\n");
      for (i=0; i<MAXUSER; i++) {
        if (strncmp(users[i].id, buf+IDOFFSET, MAXIDLEN) != 0) {
          if (i == MAXUSER-1) {
            /* ID incorrect */
            buf[0] = 0;
            strcpy(buf+1, "Incorrect ID\n");
            acksend(buf, PACKETSIZE,
                    (struct sockaddr *)(&cliaddr), sizeof(cliaddr));
            break;
          } else {
            continue;
          }
        }
        if (strncmp(users[i].pw, buf+PWOFFSET, MAXIDLEN) != 0) {
          /* PW incorrect */
          buf[0] = 0;
          strcpy(buf+1, "Incorrect PW\n");
          acksend(buf, PACKETSIZE,
                  (struct sockaddr *)(&cliaddr), sizeof(cliaddr));
          break;
        } else {
          /* Login success */
          users[i].online = 1;
          buf[0] = 1;
          break;
        }
      }
    case OP_SHOWUSER:
      for (i=0; i<MAXUSER; i++) {
        if (users[i].online == 1) {
          buf[0] = 1;
          strcpy(buf+1, users[i].id);
          acksend(buf, PACKETSIZE,
                  (struct sockaddr *)(&cliaddr), sizeof(cliaddr));
        }
      }
      buf[0] = 0;
      acksend(buf, PACKETSIZE,
              (struct sockaddr *)(&cliaddr), sizeof(cliaddr));
      break;
    default:
      break;
    }
  }
}

int rawsend(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len)
{
  return sendto(sockfd, buf, size, 0, pcliaddr, len);
}


int rawrecv(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len)
{
  return recvfrom(sockfd, buf, size, 0, pcliaddr, &len);
}

void acksend(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len)
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
    rawsend(buf, size, pcliaddr, len);
    n = rawrecv(ackbuf, ACKSIZE, pcliaddr, len);
    if (n > 0)
      break;
  }
}

void ackrecv(void *buf, size_t size, struct sockaddr *pcliaddr, socklen_t len)
{
  char ack = '\x06';
  struct timeval tv;

  /* Set recv timeout */
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv, sizeof(tv)) < 0) {
    perror("Error");
  }

  rawrecv(buf, size, pcliaddr, len);
  rawsend(&ack, ACKSIZE, pcliaddr, len);
}
