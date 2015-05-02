#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include <sys/stat.h>

#define SERV_PORT 9877
#define MAXLINE 4096
#define PACKET_SIZE 2048

void str_cli(FILE *fp, int sockfd);

int main(int argc, char* argv[])
{
  int sockfd;
  struct sockaddr_in servaddr;

  /* Create Download directory */
  mkdir("Download", 0777);

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
  char sendline[MAXLINE], recvline[MAXLINE], data[PACKET_SIZE];
  FILE *file;
  long file_size, left_size, n;

  /* Print prompt */
  fputs("Please select operation[1-5]: ", stdout);
  /* Process operation */
  while (fgets(sendline, MAXLINE, fp)!=NULL) {
    switch(sendline[0])
    {
    case '1':
      printf("Input directory to switch to: ");
      fgets(sendline+1, MAXLINE-1, fp);
      sendline[0]='1';
      sendline[strlen(sendline)]='\0';
      write(sockfd, sendline, strlen(sendline)+1);

      if (read(sockfd, recvline, MAXLINE)==0)
      {
        printf("ERROR\n");
        return;
      }
      printf("Current directory: %s\n", recvline);
      /* Send ACK */
      write(sockfd, "\x06", 1);
      break;
    case '3': /* Upload file from client to server */
      printf("Input file name to upload: ");
      /* Get file name */
      sendline[0]='3';
      fgets(sendline+1, MAXLINE-1, fp);
      sendline[strlen(sendline)-1]='\0';
      /* Get file size */
      file = fopen(sendline+1, "rb");
      if(!file){printf("File not found: %s\n",sendline+1);break;}
      fseek(file, 0, SEEK_END);
      file_size = ftell(file);
      rewind(file);

      /* Send operation and file name */
      write(sockfd, sendline, strlen(sendline)+1);
      /* Recv ACK */
      read(sockfd, recvline, 1);
      /* Send file size */
      write(sockfd, &file_size, sizeof(long));
      /* Recv ACK */
      read(sockfd, recvline, 1);

      left_size = file_size;
      while(left_size > 0)
      {
        if(left_size > PACKET_SIZE)
        {
          /* Send whole packet */
          n = fread(data, 1, PACKET_SIZE, file);
          write(sockfd, data, PACKET_SIZE);
          /* Recv ACK */
          read(sockfd, recvline, 1);
        }
        else
        {
          /* Send last packet */
          n = fread(data, 1, left_size, file);
          write(sockfd, data, left_size);
          /* Recv ACK */
          read(sockfd, recvline, 1);
        }
        left_size -= n;
      }
      printf("File upload complete.\n");
      /* Send ACK */
      write(sockfd, "\x06", 1);
      fclose(file);
      break;
    case '5': /* Quit client */
      return;
      break;
    default:
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
      break;
    }
    /* Print prompt */
    fputs("Please select operation[1-5]: ", stdout);
  }
}
