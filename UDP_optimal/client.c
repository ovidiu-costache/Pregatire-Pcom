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



// AM MODIFICAT DOAR PARAMETRII: In loc de nume fisier, dam textul (msg) si secventa.
void send_msg_start_stop(int sockfd, struct sockaddr_in server_address, const char *msg, int seq) {
    struct seq_udp d;
    strcpy(d.payload, msg);
    d.len = strlen(msg) + 1;
    d.seq = seq;

    while (1) {
        // TODO 1.1: Send the datagram.
        sendto(sockfd, &d, sizeof(struct seq_udp), 0,
               (struct sockaddr *)&server_address, sizeof(server_address));

        // TODO 1.2: Wait for ACK before moving to the next datagram.
        // If timeout or wrong seq number, resend the datagram.
        int ack = -1;
        int rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

        if (rc >= 0 && ack == seq) {
            break; // Am primit ACK-ul corect! Iesim din bucla (Start-Stop realizat)
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

  // for benchmarking
  TICK(TIME_A);

  // Our transmission window
  window = create_list();

  // Creating socket file descriptor. SOCK_DGRAM for UDP
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(sockfd < 0, "socket");

  // Set the timeout on the socket
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 250000; // 250ms

  rc = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  DIE(rc < 0, "setsockopt");

  // Fill the information that will be put into the IP and UDP header to
  // identify the target process (via PORT) on a given host (via SEVER_IP)
 memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(atoi(argv[2])); // PORT-ul DIN TERMINAL
  inet_aton(argv[1], &servaddr.sin_addr);   // IP-ul DIN TERMINAL

  // TODO: Read the demo function.
  // Implement and test (one at a time) each of the proposed versions for sending a
  // file.

    // In main() la client, dupa ce setezi servaddr, faci multiplexarea:
  struct pollfd fds[2];
  fds[0].fd = STDIN_FILENO; fds[0].events = POLLIN;
  fds[1].fd = sockfd;       fds[1].events = POLLIN;

  int secventa = 0; // Pentru a numara pachetele

  // Trimitem un HELLO initial ca sa primim ID-ul
  send_msg_start_stop(sockfd, servaddr, "HELLO", secventa++);

  while (1) {
      poll(fds, 2, -1);

      // Daca scriem de la tastatura
      if (fds[0].revents & POLLIN) {
          char buf[256];
          fgets(buf, sizeof(buf), stdin);
          buf[strlen(buf)-1] = '\0';
          
          if (strcmp(buf, "EXIT") == 0) break;

          // In loc de send_all, folosim functia din schelet completata:
          send_msg_start_stop(sockfd, servaddr, buf, secventa++);
      }

      // Daca primim ceva de la server
      if (fds[1].revents & POLLIN) {
          struct seq_udp recv_packet;
          recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
          
          // Ii trimitem ACK-ul inapoi (Start-stop sens invers)
          int ack = recv_packet.seq;
          sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
          
          printf("> %s\n", recv_packet.payload);
      }
  }
  //send_a_message(sockfd, servaddr);
  // send_file_start_stop(sockfd, servaddr, SENT_FILENAME);
  // send_file_go_back_n(sockfd, servaddr, SENT_FILENAME);

  close(sockfd);

  free(window);

  // Print the runtime of the program
  TOCK(TIME_A);

  return 0;
}
