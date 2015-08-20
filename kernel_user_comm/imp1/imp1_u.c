/*imp1_u.c*/
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/in.h>
#include "imp1.h"

#define UMSG      "a message from userspace\n"
#define UMSG_LEN  sizeof("a message from userspace\n")

char kmsg[64];

int main(void)
{
  int sockfd;
  int len;

  sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if(sockfd < 0)
    {
      printf("can not create a socket\n");
      return -1;
    }

  /*call function data_to_kernel()*/
  setsockopt(sockfd, IPPROTO_IP, IMP1_SET, UMSG, UMSG_LEN);

  len = sizeof(char)*64;

  /*call function data_from_kernel()*/
  getsockopt(sockfd, IPPROTO_IP, IMP1_GET, kmsg, &len);
  printf("kmsg: %s", kmsg);

  close(sockfd);
  return 0;
}
