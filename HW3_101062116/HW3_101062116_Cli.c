#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>      /* isalnum() */

#include <arpa/inet.h>
#include <sys/socket.h> /* Sockets */

/* Directory operations */
#include <dirent.h>

#define PACKETSIZE 512

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

#define IDOFFSET 32
#define PWOFFSET 64
#define MAXIDLEN 16
#define MAXPWLEN 16

#define IPOFFSET   128
#define PORTOFFSET 192
#define FNOFFSET   64

void client(void);
void alnum_prefix(char *str);
int sendall(int sockfd, char *buf, size_t size);
int recvall(int sockfd, char *buf, size_t size);

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

  /* Directory list stuffs */
  DIR *dir;
  struct dirent *ent;

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
  printf(
    "[SU]Show User [SF]Show File [T]ell [L]ogout\n"
    );
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
  /* Send file name */
  sendall(sockfd, filename, PACKETSIZE);
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

  return 0;
}
