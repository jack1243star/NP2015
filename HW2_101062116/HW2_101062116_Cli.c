#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>      /* isalnum() */

#include <arpa/inet.h>
#include <sys/socket.h> /* Sockets */
#include <unistd.h>     /* close() */

/* POSIX threads */
#include <pthread.h>
/* Directory operations */
#include <dirent.h>

#define PACKETSIZE 512
#define LISTENQ    10

#define OP_NEWUSER  1
#define OP_DELUSER  2
#define OP_LOGIN    3
#define OP_LOGOUT   4
#define OP_DOWNLOAD 5
#define OP_UPLOAD   6
#define OP_TELL     7
#define OP_YELL     8
#define OP_SHOWUSER 9
#define OP_SHOWFILE 10
#define OP_GET      11
#define OP_PUT      12

#define IDOFFSET 32
#define PWOFFSET 64
#define MAXIDLEN 16
#define MAXPWLEN 16

#define IPOFFSET   128
#define PORTOFFSET 192
#define FNOFFSET   64
#define MAXFNSIZE  128

void client(void);
void alnum_prefix(char *str);
int sendall(int sockfd, char *buf, size_t size);
int recvall(int sockfd, char *buf, size_t size);
int newsock(char **ip, short *port);
void *listener(void *connfd);
int sendfile(int sockfd, char* filename);
int getfile(char* peer_ip, short peer_port, char* filename);
int tell(char* peer_ip, short peer_port, char* msg);

int sockfd;
struct sockaddr_in servaddr;

int main(int argc, char* argv[])
{
  /* Check command line argument */
  if (argc!=3) {
    printf("Usage: %s <IPaddress> <Port>\n", argv[0]);
    return 0;
  }

  /* Create TCP socket */
  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  /* Initialize address structure */
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
  servaddr.sin_port = htons(atoi(argv[2]));
  /* Connect to server */
  connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

  /* Start client */
  client();

  return EXIT_SUCCESS;
}

void client(void)
{
  char buf[PACKETSIZE];
  pthread_t listener_thread;
  int listener_fd;
  char* listener_ip;
  short listener_port;
  char peer_ip[64];
  short peer_port;

  /* Directory list stuffs */
  DIR *dir;
  struct dirent *ent;

  /* Create listener thread */
  listener_fd = newsock(&listener_ip, &listener_port);
  if (pthread_create(&listener_thread, NULL, listener, &listener_fd)) {
    printf("pthread error\n");
    exit(EXIT_FAILURE);
  }
  printf("Listening on PORT=%hu\n", listener_port);

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

    sendall(sockfd, buf, PACKETSIZE);
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

    sendall(sockfd, buf, PACKETSIZE);
    recvall(sockfd, buf, PACKETSIZE);

    if (buf[0] == 1) {
      printf("*** Hello %s ***\n", buf+IDOFFSET);
      /* Send file list */
      dir = opendir(".");
      while((ent = readdir(dir)) != NULL)
      {
        buf[0] = 1;
	strcpy(buf+1, ent->d_name);
        sendall(sockfd, buf, PACKETSIZE);
      }
      closedir(dir);
      buf[0] = 0;
      sendall(sockfd, buf, PACKETSIZE);
      /* Send file server ip and port */
      strcpy(buf+IPOFFSET, listener_ip);
      memcpy(buf+PORTOFFSET, (void*)&listener_port, sizeof(short));
      sendall(sockfd, buf, PACKETSIZE);
      /* Goto main menu */
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
  printf("[SU]Show User [SF]Show File [T]ell [L]ogout\n"
         "[D]ownload [U]pload\n");
  fgets(buf, 16, stdin);
  switch(buf[0]) {
  case 'L':
  case 'l':
    buf[0] = OP_LOGOUT;
    sendall(sockfd, buf, PACKETSIZE);
    goto welcome;
    break;
  case 'S':
  case 's':
    switch(buf[1]) {
    case 'U': /* Show user */
    case 'u':
      buf[0] = OP_SHOWUSER;
      sendall(sockfd, buf, PACKETSIZE);
      for (;;) {
        recvall(sockfd, buf, PACKETSIZE);
        if (buf[0] == 1)
          printf("%16s %16s %5hu\n",
		 buf+1, buf+IPOFFSET, *(short*)(buf+PORTOFFSET));
        else
          break;
      }
      break;

    case 'F': /* Show file */
    case 'f':
      buf[0] = OP_SHOWFILE;
      sendall(sockfd, buf, PACKETSIZE);
      for (;;) {
        recvall(sockfd, buf, PACKETSIZE);
        if (buf[0] == 1)
          printf("%16s%48s\n", buf+1, buf+FNOFFSET);
        else
          break;
      }
      break;

    default:
      break;
    }
    break;

  case 'D': /* Download file */
  case 'd':
    buf[0] = OP_DOWNLOAD;
    printf("Input user ID:");
    fgets(buf+IDOFFSET, 16, stdin);
    alnum_prefix(buf+IDOFFSET);
    printf("Input file name:");
    fgets(buf+FNOFFSET, MAXFNSIZE, stdin);
    buf[FNOFFSET+strlen(buf+FNOFFSET)-1]='\0';
    printf("%s\n",buf+FNOFFSET);
    sendall(sockfd, buf, PACKETSIZE);
    /* Get IP and port */
    recvall(sockfd, buf, PACKETSIZE);
    /* Check for error */
    if (buf[0] == 0) {printf("%s\n", buf+1); break;}
    strcpy(peer_ip, buf+IPOFFSET);
    memcpy(&peer_port, buf+PORTOFFSET, sizeof(short));
    /* Go get file from peer */
    getfile(peer_ip, peer_port, buf+FNOFFSET);
    break;

  case 'T':
  case 't':
    buf[0] = OP_TELL;
    printf("Input user ID:");
    fgets(buf+IDOFFSET, 16, stdin);
    alnum_prefix(buf+IDOFFSET);
    printf("Input message:");
    fgets(buf+FNOFFSET, MAXFNSIZE, stdin);
    buf[FNOFFSET+strlen(buf+FNOFFSET)-1]='\0';
    /* Get IP and port */
    sendall(sockfd, buf, PACKETSIZE);
    recvall(sockfd, buf, PACKETSIZE);
    /* Go get file from peer */
    strcpy(peer_ip, buf+IPOFFSET);
    memcpy(&peer_port, buf+PORTOFFSET, sizeof(short));
    tell(peer_ip, peer_port, buf+FNOFFSET);
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

int sendall(int sockfd, char *buf, size_t size)
{
  size_t sentbyte = 0;
  int n;

  while (sentbyte < size) {
    n = send(sockfd, buf+sentbyte, size-sentbyte, 0);
    if (n == -1) return -1;
    sentbyte += n;
  }

  return 0;
}

int recvall(int sockfd, char *buf, size_t size)
{
  size_t recvbyte = 0;
  int n;

  while (recvbyte < size) {
    n = recv(sockfd, buf+recvbyte, size-recvbyte, 0);
    if (n == 0) return -1;
    recvbyte += n;
  }

  return 0;
}

int newsock(char **ip, short *port)
{
  int sockfd;
  struct sockaddr_in servaddr;
  socklen_t servlen = sizeof(servaddr);

  /* Create TCP socket */
  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  /* Initialize address structure */
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = 0; /* Any socket */
  /* Bind to address */
  bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  getsockname(sockfd, (struct sockaddr*)&servaddr, &servlen);
  *ip = strdup(inet_ntoa(servaddr.sin_addr));
  *port = ntohs(servaddr.sin_port);
  /* Listen to socket */
  listen(sockfd, LISTENQ);

  return sockfd;
}

void *listener(void *arg)
{
  int listenfd, connfd;
  struct sockaddr_in cliaddr;
  size_t clilen;
  char buf[PACKETSIZE];

  listenfd = *(int*)arg;

  for (;;) {
    clilen = sizeof(cliaddr);
    /* Accept connection */
    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen);
    printf("Connection from IP=%s PORT=%hu\n",
	   inet_ntoa(cliaddr.sin_addr),
	   ntohs(cliaddr.sin_port));
    /* Handle request */
    if(recvall(connfd, buf, PACKETSIZE)){continue;}
    switch (buf[0]) {
    case OP_TELL:
	printf("%s\n", buf+FNOFFSET);
	break;
    case OP_GET:
      if (sendfile(connfd, buf+FNOFFSET))
	printf("Send file '%s' failed.\n", buf+FNOFFSET);
      else
	printf("Send file '%s' success.\n", buf+FNOFFSET);
      break;
    case OP_PUT:
      break;
    default:
      break;
    }
  }
}

int sendfile(int sockfd, char* filename)
{
  FILE* file;
  size_t leftsize, filesize, n;
  char data[PACKETSIZE];

  /* Open file */
  file = fopen(filename, "rb");
  if(!file){printf("File not found: %s\n",filename);return -1;}
  /* Calculate file size */
  fseek(file, 0, SEEK_END);
  filesize = ftell(file);
  rewind(file);
  /* Send file size */
  sendall(sockfd, (void*)(&filesize), sizeof(long));

  leftsize = filesize;
  while (leftsize > 0) {
    if (leftsize > PACKETSIZE) {
      /* Send whole packet */
      n = fread(data, 1, PACKETSIZE, file);
      sendall(sockfd, data, PACKETSIZE);
    } else {
      /* Send last packet */
      n = fread(data, 1, leftsize, file);
      sendall(sockfd, data, leftsize);
    }
    leftsize -= n;
  }
  fclose(file);

  return 0;
}

int getfile(char* peer_ip, short peer_port, char* filename)
{
  int connfd;
  struct sockaddr_in peeraddr;
  FILE* file;
  size_t leftsize, filesize, n;
  char data[PACKETSIZE];

  printf("Try to get '%s' from IP=%s PORT=%hu...", filename,
	 peer_ip, peer_port);

  /* Create TCP socket */
  connfd = socket(PF_INET, SOCK_STREAM, 0);
  /* Initialize address structure */
  memset(&peeraddr, 0, sizeof(peeraddr));
  peeraddr.sin_family = AF_INET;
  inet_pton(AF_INET, peer_ip, &peeraddr.sin_addr);
  peeraddr.sin_port = htons(peer_port);
  /* Connect to peer */
  connect(connfd, (struct sockaddr*)&peeraddr, sizeof(peeraddr));

  /* Open file */
  file = fopen(filename, "wb");
  /* Send request */
  data[0] = OP_GET;
  strcpy(data+FNOFFSET, filename);
  sendall(connfd, data, PACKETSIZE);
  /* Recv file size */
  recvall(connfd, (void*)(&filesize), sizeof(long));

  leftsize = filesize;
  while (leftsize > 0) {
    if (leftsize > PACKETSIZE) {
      /* Recv whole packet */
      recvall(connfd, data, PACKETSIZE);
      n = fwrite(data, 1, PACKETSIZE, file);
    } else {
      /* Send last packet */
      recvall(connfd, data, leftsize);
      n = fwrite(data, 1, leftsize, file);
    }
    leftsize -= n;
  }
  fclose(file);
  close(connfd);
  printf("Success.\n");
  return 0;
}

int tell(char* peer_ip, short peer_port, char* msg)
{
  int connfd;
  struct sockaddr_in peeraddr;
  char data[PACKETSIZE];

  printf("Tell '%s' to IP=%s PORT=%hu...", msg, peer_ip, peer_port);

  /* Create TCP socket */
  connfd = socket(PF_INET, SOCK_STREAM, 0);
  /* Initialize address structure */
  memset(&peeraddr, 0, sizeof(peeraddr));
  peeraddr.sin_family = AF_INET;
  inet_pton(AF_INET, peer_ip, &peeraddr.sin_addr);
  peeraddr.sin_port = htons(peer_port);
  /* Connect to peer */
  connect(connfd, (struct sockaddr*)&peeraddr, sizeof(peeraddr));

  /* Send request */
  data[0] = OP_TELL;
  strcpy(data+FNOFFSET, msg);
  sendall(connfd, data, PACKETSIZE);

  close(connfd);
  printf("Success.\n");
  return 0;
}

