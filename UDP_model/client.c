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

// void send_file_start_stop(int sockfd, struct sockaddr_in server_address,
//                           char *filename) {

//   int fd = open(filename, O_RDONLY);
//   DIE(fd < 0, "open");
//   int rc;
//   int seq = 0;

//   while (1) {
//     /* Reads a chunk of the file */
//     struct seq_udp d;
//     int n = read(fd, d.payload, sizeof(d.payload));
//     DIE(n < 0, "read");
//     d.len = n;
//     d.seq = seq;
//     seq++;

//     // TODO 1.1: Send the datagram.

//     // TODO 1.2: Wait for ACK before moving to the next datagram to send.
//     // If timeout or wrong seq number, resend the datagram.

//     if (n == 0) // end of file
//       break;
//   }
// }

// void send_a_message(int sockfd, struct sockaddr_in server_address) {
//   struct seq_udp d;
//   strcpy(d.payload, "Hello world!");
//   d.len = strlen("Hello world!");

//   // Send a UDP datagram. Sendto is implemented in the kernel (network stack of
//   // it), it basically creates a UDP datagram, sets the payload to the data we
//   // specified in the buffer, and the completes the IP header and UDP header
//   // using the sever_address info.
//   int rc = sendto(sockfd, &d, sizeof(struct seq_udp), 0,
//                   (struct sockaddr *)&server_address, sizeof(server_address));

//   DIE(rc < 0, "send");

//   // Receive the ACK. recvfrom is blocking with the current parameters 
//   int ack;
//   rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);
// }

void run_client(int sockfd, struct sockaddr_in server_address) {
    struct seq_udp sent_message;
    struct seq_udp recv_message;
    char buf[MAXSIZE];

    int rc;
    int seq = 0;

    struct seq_udp ack;

    // Cazul de baza, trimit INIT
    strcpy(sent_message.payload, "INIT");
    sent_message.seq = seq;
    sent_message.len = strlen(sent_message.payload) + 1;

    // Bucla de Start-Stop
    while (1) {
        sendto(sockfd, &sent_message, sizeof(sent_message), 0, (struct sockaddr *) &server_address, sizeof(server_address));
        rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

        // Verific daca am primit ACK bun, adica sa aiba "ACK" in payload
        if (rc < 0) {
            // S-a pierdut sau s-a primit prost, trebuie retransmis
            printf("[CLIENT] Pachet pierdut. Retransmit...\n");
            continue;
        } else if (rc > 0) {
            if (strncmp(ack.payload, "ACK", 3) == 0 && ack.seq == sent_message.seq) {
                // Am primit ACK bun
                seq++;
                break;
            } else if (strncmp(ack.payload, "ACK", 3) != 0) {
                // Am primit mesajul de conectare de la server, il afisez
                printf("[CLIENT] %s\n", ack.payload);

                // Trimit ACK serverului sa stie ca am primit pachetul
                struct seq_udp sent_ack;
                strcpy(sent_ack.payload, "ACK");
                sent_ack.seq = ack.seq;
                sent_ack.len = strlen(sent_ack.payload) + 1;

                sendto(sockfd, &sent_ack, sizeof(sent_ack), 0, (struct sockaddr *) &server_address, sizeof(server_address));

                seq++;
                break;
            }
        }
    }

    // Poll_fds pt a asculta socket ul si tastatura
    struct pollfd poll_fds[2];

    poll_fds[0].fd = sockfd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = STDIN_FILENO;
    poll_fds[1].events = POLLIN;

    while (1) {
        rc = poll(poll_fds, 2, -1);
        DIE(rc < 0, "poll");

        // Am primit raspuns de la server
        if (poll_fds[0].revents & POLLIN) {
            rc = recvfrom(sockfd, &recv_message, sizeof(recv_message), 0, NULL, NULL);

            if (rc > 0) {
                // Verific daca e EXIT
                if (strncmp(recv_message.payload, "EXIT", 4) == 0) {
                    printf("[CLIENT] Se inchide...\n");
                    return;
                }

                // Daca e vreun ACK ratacit, il ignor
                if (strncmp(recv_message.payload, "ACK", 3) == 0) {
                    continue;
                }

                // Trimit ACK serverului sa stie ca am primit pachetul
                struct seq_udp sent_ack;
                strcpy(sent_ack.payload, "ACK");
                sent_ack.seq = recv_message.seq;
                sent_ack.len = strlen(sent_ack.payload) + 1;

                sendto(sockfd, &sent_ack, sizeof(sent_ack), 0, (struct sockaddr *) &server_address, sizeof(server_address));

                printf("[CLIENT] %s\n", recv_message.payload);
            }
        }

        // Am primit input de la tastatura
        if (poll_fds[1].revents & POLLIN) {
            if (fgets(buf, sizeof(buf), stdin) && !isspace(buf[0])) {
                sent_message.seq = seq;
                strcpy(sent_message.payload, buf);
                sent_message.len = strlen(sent_message.payload) + 1;

                // Start-stop pt retransmisie
                while (1) {
                    sendto(sockfd, &sent_message, sizeof(sent_message), 0, (struct sockaddr *) &server_address, sizeof(server_address));
                    rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

                    if (rc < 0) {
                        printf("[CLIENT] Pachet pierdut. Retransmit...\n");
                        continue;
                    } else if (rc > 0) {
                        if (strncmp(ack.payload, "ACK", 3) == 0 && ack.seq == sent_message.seq) {
                            seq++;
                            break;
                        } else if (strncmp(ack.payload, "ACK", 3) != 0) {
                            // Am primit raspunsul si ii dau ACK inapoi sa stie ca l am primit
                            struct seq_udp sent_ack;
                            strcpy(sent_ack.payload, "ACK");
                            sent_ack.seq = ack.seq;
                            sent_ack.len = strlen(sent_ack.payload) + 1;
                            sendto(sockfd, &sent_ack, sizeof(sent_ack), 0, (struct sockaddr *) &server_address, sizeof(server_address));

                            printf("[CLIENT] %s\n", ack.payload);
                            seq++;
                            break;
                        }
                    }
                }

                if (strncmp(buf, "EXIT", 4) == 0) {
                    printf("[CLIENT] Iesire manuala...\n");
                    break;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("\n Usage: %s <ip> <port>\n", argv[0]);
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
  window = create_list();

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
