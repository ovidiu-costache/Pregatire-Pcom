#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/*
    TODO 1.1: Rescrieți funcția de mai jos astfel încât ea să facă primirea
    a exact len octeți din buffer.
*/
int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

  while(bytes_remaining) {
    int curr_recv = recv(sockfd, buff + bytes_received, bytes_remaining, 0);

    if (curr_recv <= 0) {
      return curr_recv;
    }

    bytes_received += curr_recv;
    bytes_remaining -= curr_recv;
  }

  /*
    TODO: Returnam exact cati octeti am citit
  */
  return bytes_received;
}

/*
    TODO 1.2: Rescrieți funcția de mai jos astfel încât ea să facă trimiterea
    a exact len octeți din buffer.
*/

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

  while(bytes_remaining) {
    int curr_sent = send(sockfd, buff + bytes_sent, bytes_remaining, 0);

    if (curr_sent <= 0) {
      return curr_sent;
    }

    bytes_sent += curr_sent;
    bytes_remaining -= curr_sent;
  }

  /*
    TODO: Returnam exact cati octeti am trimis
  */
  return bytes_sent;
}
