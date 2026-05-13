#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/poll.h>
#include <ctype.h>

#include "common.h"
#include "list.h"
#include "utils.h"

// Max size of the datagrams that we will be sending
#define CHUNKSIZE MAX_SIZE;
#define SENT_FILENAME "file.bin"
#define SERVER_IP "172.16.0.100"

#define TICK(X)                                                                \
  struct timespec X;                                                           \
  clock_gettime(CLOCK_MONOTONIC_RAW, &X)

#define TOCK(X)                                                                \
  struct timespec X##_end;                                                     \
  clock_gettime(CLOCK_MONOTONIC_RAW, &X##_end);                                \
  printf("Total time = %f seconds\n",                                          \
         (X##_end.tv_nsec - (X).tv_nsec) / 1000000000.0 +                      \
             (X##_end.tv_sec - (X).tv_sec))

list *window;

void send_msg_start_stop(int sockfd, struct sockaddr_in server_address,
                          char *msg, int seq) {
  struct seq_udp d;
  strcpy(d.payload, msg);
  d.len = strlen(msg) + 1;
  d.seq = seq;

  while (1) {
    // TODO 1.1: Send the datagram.
    sendto(sockfd, &d, sizeof(struct seq_udp), 0, (struct sockaddr *)&server_address, sizeof(server_address));

    // TODO 1.2: Wait for ACK before moving to the next datagram to send.
    // If timeout or wrong seq number, resend the datagram.
    int ack;
    int rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

    if (rc >= 0 && ack == seq)
      break;
  }
}

void run_client(int sockfd, struct sockaddr_in server_address) {
  // Multiplexarea
  struct pollfd poll_fds[2];

  poll_fds[0].fd = sockfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = STDIN_FILENO;
  poll_fds[1].events = POLLIN;

  int seq = 0;

  send_msg_start_stop(sockfd, server_address, "HELLO", seq++);

  while (1) {
    int rc = poll(poll_fds, 2, -1);
    DIE(rc < 0, "poll");

    // Server
    if (poll_fds[0].revents & POLLIN) {
      struct seq_udp recv_packet;
      recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);

      int ack = recv_packet.seq;
      sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&server_address, sizeof(server_address));

      printf("> %s\n", recv_packet.payload);
    }

    // Tastatura
    if (poll_fds[1].revents & POLLIN) {
      char buf[256];
        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf)-1] = '\0';

        if (strcmp(buf, "EXIT") == 0) break;

        send_msg_start_stop(sockfd, server_address, buf, seq++);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
      printf("Rulare: ./client <ip> <port>\n");
      return 1;
  }

  // We use this structure to store the server info. IP address and Port.
  // This will be written by the UDP implementation on recvfrom().
  struct sockaddr_in servaddr;
  int sockfd, rc;
  uint16_t port = atoi(argv[2]);

  // for benchmarking
  TICK(TIME_A);

  // Our transmission window
  // window = create_list();

  // Creating socket file descriptor. SOCK_DGRAM for UDP
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(sockfd < 0, "socket");

  // Set the timeout on the socket
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  rc = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  DIE(rc < 0, "setsockopt");

  // Fill the information that will be put into the IP and UDP header to
  // identify the target process (via PORT) on a given host (via SEVER_IP)
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  inet_aton(argv[1], &servaddr.sin_addr);

  // TODO: Read the demo function.
  // Implement and test (one at a time) each of the proposed versions for sending a
  // file.

  // send_a_message(sockfd, servaddr);
  // send_file_start_stop(sockfd, servaddr, SENT_FILENAME);
  // send_file_go_back_n(sockfd, servaddr, SENT_FILENAME);

  run_client(sockfd, servaddr);

  close(sockfd);

  free(window);

  // Print the runtime of the program
  TOCK(TIME_A);

  return 0;
}
