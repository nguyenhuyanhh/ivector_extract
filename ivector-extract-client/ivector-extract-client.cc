#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define MAXBUF 1024
#define PORTNO 1428

int main(int argc, char *argv[]){
  int sockfd;
  struct sockaddr_in dest;
  char buffer[MAXBUF];

  if (argc < 2){
    fprintf(stderr,"usage %s feature_rspecifier\n", argv[0]);
    exit(0);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0){
    perror("Socket");
    exit(errno);
  }

  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = inet_addr("0.0.0.0");
  dest.sin_port = htons(PORTNO);

  int c = connect(sockfd, (struct sockaddr *) &dest, sizeof(dest));
  if (c < 0){
    perror("Connect");
    exit(errno);
  }

  std::string input = argv[1];
  int n = send(sockfd, input.data(), input.size(), 0);
  if (n < 0){
    perror("Send");
    exit(errno);
  }
  else
    std::cout << "Successful send\n";
  if (strcmp(argv[1], "quit") != 0){
    int res = recv(sockfd, buffer, MAXBUF, 0);
    std::cout << buffer << "\n";  
  }
  
  close(sockfd);
  return 0;
}
