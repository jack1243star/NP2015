#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define LISTENQ     10
#define PACKETSIZE  512
#define NUM_THREADS 10
#define MAXUSER     16

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

struct file {
  char filename[64];
  struct file *prev;
  struct file *next;
};

struct user {
  char id[16];
  char pw[16];
  char ip[64];
  short port;
  int online;
  char listenip[64];
  short listenport;
  struct file *filelist;
};

struct session {
  int connfd;
  char ip[64];
  short port;
};

void *serve(void *session);
int sendall(int sockfd, char *buf, size_t size);
int recvall(int sockfd, char *buf, size_t size);
void addfile(struct user *u, char* filename);
void freefile(struct file *root);

/* Global user list */
struct user users[MAXUSER];

int main(int argc, char* argv[])
{
  int listenfd, connfd, i, j = 0;
  struct sockaddr_in servaddr, cliaddr;
  size_t clilen;
  struct session *currsession;
  pthread_t threads[NUM_THREADS];

  /* Check command line argument */
  if (argc!=2) {
    printf("Usage: %s <Port>\n", argv[0]);
    return 0;
  }

  /* Setup user list */
  for (i=0; i<MAXUSER; i++) {
    memset(users[i].id, 0, MAXIDLEN);
    memset(users[i].pw, 0, MAXPWLEN);
    users[i].online = 0;
    users[i].filelist = NULL;
  }


  /* Create TCP socket */
  listenfd = socket(PF_INET, SOCK_STREAM, 0);
  /* Initialize address structure */
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(atoi(argv[1]));
  /* Bind to address */
  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  /* Listen to socket */
  listen(listenfd, LISTENQ);

  for (;;) {
    clilen = sizeof(cliaddr);
    /* Accept connection */
    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen);
    currsession = (struct session*)malloc(sizeof(struct session));
    currsession->connfd = connfd;
    /* Get IP */
    strcpy(currsession->ip, inet_ntoa(cliaddr.sin_addr));
    /* Get port number */
    currsession->port = ntohs(cliaddr.sin_port);
    /* Create client thread */
    if (pthread_create(&threads[j++], NULL, serve, (void *)currsession)) {
      printf("pthread error\n");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

void *serve(void *session)
{
  int connfd = ((struct session *)session)->connfd;
  short port = ((struct session *)session)->port;
  char ip[64];
  char buf[PACKETSIZE];
  int i, curruser;
  struct file *tfile;

  strncpy(ip, ((struct session *)session)->ip, 64);
  free(session);

  for (;;) {
    if(recvall(connfd, buf, PACKETSIZE)){printf("User disconnected.\n");return NULL;}
    printf("Receive from IP=%s PORT=%hu\n", ip, port);
    switch (buf[0]) {
    case OP_NEWUSER:
      for (i=0; i<MAXUSER; i++) {
        if (users[i].id[0] == 0) {
          strncpy(users[i].id, buf+IDOFFSET, MAXIDLEN);
          strncpy(users[i].pw, buf+PWOFFSET, MAXPWLEN);
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
            sendall(connfd, buf, PACKETSIZE);
            break;
          } else {
            continue;
          }
        }
        if (strncmp(users[i].pw, buf+PWOFFSET, MAXIDLEN) != 0) {
          /* PW incorrect */
          buf[0] = 0;
          strcpy(buf+1, "Incorrect PW\n");
          sendall(connfd, buf, PACKETSIZE);
          break;
        } else {
          /* Login success */
	  curruser = i;
          users[i].online = 1;
	  strncpy(users[i].ip, ip, 64);
	  users[i].port = port;
          buf[0] = 1;
	  sendall(connfd, buf, PACKETSIZE);
	  /* Receive file list */
          for (;;) {
            recvall(connfd, buf, PACKETSIZE);
            if (buf[0] == 1) {
              printf("%s\n", buf+1);
	      addfile(&users[i], buf+1);
	    } else
	      break;
	  }
	  /* Receive listen IP and port */
	  recvall(connfd, buf, PACKETSIZE);
	  strncpy(users[i].listenip, buf+IPOFFSET, 64);
	  memcpy(&users[i].listenport, buf+PORTOFFSET, sizeof(short));
          break;
        }
      }
      break;
    case OP_LOGOUT:
      users[curruser].online = 0;
      freefile(users[curruser].filelist);
      break;
    case OP_SHOWUSER:
      for (i=0; i<MAXUSER; i++) {
        if (users[i].online == 1) {
          buf[0] = 1;
          strncpy(buf+1, users[i].id, MAXIDLEN);
	  strncpy(buf+IPOFFSET, users[i].ip, 64);
	  memcpy(buf+PORTOFFSET, (void*)&users[i].port, sizeof(short));
          sendall(connfd, buf, PACKETSIZE);
        }
      }
      buf[0] = 0;
      sendall(connfd, buf, PACKETSIZE);
      break;
    case OP_SHOWFILE:
      for (i=0; i<MAXUSER; i++) {
        if (users[i].online == 1) {
	  tfile = users[i].filelist;
	  while (tfile != NULL) {
	    buf[0] = 1;
	    strncpy(buf+1, users[i].id, MAXIDLEN);
	    strncpy(buf+FNOFFSET, tfile->filename, 64);
	    sendall(connfd, buf, PACKETSIZE);
	    tfile = tfile->next;
	  }
        }
      }
      buf[0] = 0;
      sendall(connfd, buf, PACKETSIZE);
      break;
    case OP_DOWNLOAD:
      /* Send ip and port of peer which has the file */
      for (i=0; i<MAXUSER; i++) {
        if (strncmp(users[i].id, buf+IDOFFSET, MAXIDLEN) == 0) {
	  buf[0] = 1;
	  strcpy(buf+IPOFFSET, users[i].listenip);
	  memcpy(buf+PORTOFFSET, &users[i].listenport, sizeof(short));
	  sendall(connfd, buf, PACKETSIZE);
	  break;
        }
      }
      /* ID incorrect */
      buf[0] = 0;
      strcpy(buf+1, "Incorrect ID\n");
      sendall(connfd, buf, PACKETSIZE);
      break;
    case OP_TELL:
      /* Send ip and port of peer which has the file */
      for (i=0; i<MAXUSER; i++) {
        if (strncmp(users[i].id, buf+IDOFFSET, MAXIDLEN) == 0) {
	  buf[0] = 1;
	  strcpy(buf+IPOFFSET, users[i].listenip);
	  memcpy(buf+PORTOFFSET, &users[i].listenport, sizeof(short));
	  sendall(connfd, buf, PACKETSIZE);
	  break;
        }
      }
      /* ID incorrect */
      buf[0] = 0;
      strcpy(buf+1, "Incorrect ID\n");
      sendall(connfd, buf, PACKETSIZE);
      break;
    default:
      break;
    }
  }
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

void addfile(struct user *u, char* filename)
{
  struct file *cf, *pf, *new;

  new = (struct file *)malloc(sizeof(struct file));
  strcpy(new->filename, filename);
  new->prev = NULL;
  new->next = NULL;

  if (u->filelist == NULL) {
    u->filelist = new;
  } else {
    cf = u->filelist;
    pf = NULL;
    while (cf != NULL) {
      pf = cf;
      cf = cf->next;
    }
    pf->next = new;
    new->prev = pf;
  }
}

void freefile(struct file *root)
{
  struct file *curr = root;
  while (curr->next != NULL) {
    curr = curr->next;
    free(curr->prev);
  }
  free(curr);
}
