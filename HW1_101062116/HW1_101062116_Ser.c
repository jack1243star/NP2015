#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#define SERV_PORT 9877
#define MAXLINE 128
#define LISTENQ 100 /* Backlog size */
#define PACKET_SIZE 2048

void str_echo(int sockfd);
void sig_chld(int signo);

int main()
{
  int listenfd, connfd;
  pid_t childpid;
  socklen_t clilen;
  struct sockaddr_in cliaddr, servaddr;
  char* ip;
  short int port;

  /* Create Upload directory */
  mkdir("Upload", 0777);

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  /* Bind wildcard address */
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  /* Bind to port SERV_PORT */
  servaddr.sin_port = htons(SERV_PORT);
  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  listen(listenfd, LISTENQ);
  signal(SIGCHLD, sig_chld);
  for(;;) {
    clilen = sizeof(cliaddr);
    connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
    ip = inet_ntoa(cliaddr.sin_addr);
    port = ntohs(cliaddr.sin_port);
    fprintf(stdout, "connection from %s, port %hu\n",ip, port);
    if((childpid=fork())==0) {
      close(listenfd);
      str_echo(connfd);
      exit(0);
    }
    close(connfd);
  }
}

void str_echo(int sockfd)
{
  char buf[MAXLINE], data[PACKET_SIZE];
  DIR *dir;
  struct dirent *ent;
  char* ls_result = NULL;
  char* ls_result_new = NULL;
  int ls_result_size = 1;
  long file_size, left_size, n;
  FILE *file;

 again:
  while((n=read(sockfd, buf, MAXLINE))>0)
  {
    switch(buf[0])
    {
    case '1': /* Change directory */
      /* Trim the newline character */
      buf[strlen(buf)-1] = '\0';
      chdir(buf+1);
      getcwd(buf, MAXLINE);
      /* Send current working directory */
      write(sockfd, buf, strlen(buf)+1);
      read(sockfd, buf, MAXLINE);
      break;
    case '2': /* List directory contents */
      ls_result = (char*)malloc(1);
      ls_result[0] = '\0';
      ls_result_size = 1;

      dir = opendir(".");
      while((ent = readdir(dir)) != NULL)
      {
        ls_result_size += (strlen(ent->d_name)+1);
        ls_result_new = (char*)calloc(ls_result_size, sizeof(char));
        strcpy(ls_result_new, ls_result);
        strcat(ls_result_new, ent->d_name);
        free(ls_result);
        ls_result = ls_result_new;
        ls_result[ls_result_size-2] = '\n';
        ls_result[ls_result_size-1] = '\0';
      }
      closedir(dir);
      write(sockfd, ls_result, ls_result_size);
      free(ls_result);
      read(sockfd, buf, MAXLINE);
      break;
    case '3': /* Upload file from client to server */
      /* Send ACK */
      write(sockfd, "\x06", 1);
      /* Recv file size */
      read(sockfd, &file_size, sizeof(long));
      /* Send ACK */
      write(sockfd, "\x06", 1);

      printf("Upload file: %s(size = %ld)\n", buf+1, file_size);
      file = fopen(buf+1, "wb");
      left_size = file_size;
      while(left_size > 0)
      {
        if(left_size > PACKET_SIZE)
        {
          /* Recv whole packet */
          read(sockfd, data, PACKET_SIZE);
          n = fwrite(data, 1, PACKET_SIZE, file);
          /* Send ACK */
          write(sockfd, "\x06", 1);
        }
        else
        {
          /* Recv last packet */
          read(sockfd, data, left_size);
          n = fwrite(data, 1, left_size, file);
          /* Send ACK */
          write(sockfd, "\x06", 1);
        }
        left_size -= n;
      }
      printf("File upload complete.\n");
      /* Recv ACK */
      read(sockfd, buf, 1);
      fclose(file);
      break;
    case '4': /* Download file from server to client */
      /* Get file size */
      file = fopen(buf+1, "rb");
      if(!file){
        printf("File not found: %s\n", buf+1);
        file_size = 0;
        write(sockfd, &file_size, sizeof(long));
        read(sockfd, buf, 1);
        break;
      }
      fseek(file, 0, SEEK_END);
      file_size = ftell(file);
      rewind(file);
      /* Send file size */
      write(sockfd, &file_size, sizeof(long));
      /* Recv ACK */
      read(sockfd, buf, 1);

      printf("Download file: %s(size = %ld)\n", buf+1, file_size);
      left_size = file_size;
      while(left_size > 0)
      {
        if(left_size > PACKET_SIZE)
        {
          /* Send whole packet */
          n = fread(data, 1, PACKET_SIZE, file);
          write(sockfd, data, PACKET_SIZE);
          /* Recv ACK */
          read(sockfd, buf, 1);
        }
        else
        {
          /* Send last packet */
          n = fread(data, 1, left_size, file);
          write(sockfd, data, left_size);
          /* Recv ACK */
          read(sockfd, buf, 1);
        }
        left_size -= n;
      }
      printf("File download complete.\n");
      fclose(file);
      break;
    default:
      break;
    }
  }
  if (n<0 && errno == EINTR) /* interrupted by a signal before any data was read */
    goto again; /* ignore EINTR */
  else if (n<0)
    printf("str_echo:read error");
}

void sig_chld(int signo)
{
  pid_t pid;
  int stat;

  (void)signo;

  while((pid=waitpid(-1, &stat, WNOHANG))>0)
    printf("child %d terminated\n", pid);
  fflush(stdout);
  return;
}
