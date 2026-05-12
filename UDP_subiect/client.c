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

// void send_file_go_back_n(int sockfd, struct sockaddr_in server_address,
//                          char *filename) {

//   int fd = open(filename, O_RDONLY);
//   DIE(fd < 0, "open");
//   int rc;

//   // TODO 2.1: Increase window size to a value that optimally uses the link
//   int window_size = 5;
//   window->max_seq = 5;
  
//   // Read the entire file in chunks and add them into a list of seq_udp (window)
//   int seq = 1;
//   while (1) {
//     struct seq_udp *d = malloc(sizeof(struct seq_udp));
//     DIE(d == NULL, "malloc");

//     int n = read(fd, d->payload, sizeof(d->payload));
//     DIE(n < 0, "read");
//     d->len = n;
//     d->seq = seq;

//     add_list_elem(window, d, sizeof(struct seq_udp), seq);
//     seq++;

//     if (n == 0) // end of file
//       break;
//   }

//   // TODO 2.2: Send window_size  packets to the server to saturate the link

//   // In a loop, untill the list of packets is empty

//   // TODO 2.2: On ACK remove from the list all the segments that have been ACKed
//   //           and send the next new segments added to the window

//   // TODO 2.3: On timeout on recv resend all the segments from the window
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
  struct pollfd poll_fds[2];
  char buf[MAXSIZE];
  struct seq_udp sent_packet;
  struct seq_udp recv_packet;

  int seq = 0;
  int rc;
  socklen_t addr_len = sizeof(server_address);

  poll_fds[0].fd = sockfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = STDIN_FILENO;
  poll_fds[1].events = POLLIN;

  // Cer ID de la Server
  sent_packet.seq = seq;
  strcpy(sent_packet.payload, "INIT");
  sent_packet.len = strlen(sent_packet.payload) + 1;

  struct seq_udp ack_recv; 
  while (1) {
    sendto(sockfd, &sent_packet, sizeof(sent_packet), 0, (struct sockaddr *) &server_address, addr_len);
    rc = recvfrom(sockfd, &ack_recv, sizeof(ack_recv), 0, NULL, NULL);
    
    if (rc < 0) {
      printf("[CLIENT] Timeout la INIT. Retransmit...\n");
      continue;
    } else if (rc > 0) {
      if (strncmp(ack_recv.payload, "ACK", 3) == 0 && ack_recv.seq == sent_packet.seq) {
        seq++;
        break; // Am primit un simplu ACK, serverul a inregistrat cererea
      } else if (strncmp(ack_recv.payload, "ACK", 3) != 0) {
        // Am primit direct mesajul de bun venit (ex: "Bun venit. Ai ID-ul 0")
        
        // Trimitem noi ACK serverului ca am primit raspunsul
        struct seq_udp ack_to_send;
        ack_to_send.seq = ack_recv.seq;
        strcpy(ack_to_send.payload, "ACK");
        ack_to_send.len = 4;
        sendto(sockfd, &ack_to_send, sizeof(ack_to_send), 0, (struct sockaddr *) &server_address, addr_len);
        
        // Afisam ce a zis serverul si trecem la comenzi
        printf("> %s", ack_recv.payload);
        seq++;
        break;
      }
    }
  }

  // Astept comenzi sau raspunsuri
  while (1) {
    rc = poll(poll_fds, 2, -1);
    DIE(rc < 0, "poll");

    // -------------------------------------------------------------
    // CAZ 1: Am primit un mesaj necerut de la server (ex: serverul a dat EXIT)
    // -------------------------------------------------------------
    if (poll_fds[0].revents & POLLIN) {
      rc = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
      if (rc > 0) {
        if (strncmp(recv_packet.payload, "ACK", 3) == 0) continue; // Ignoram ACK-uri rătăcite

        // Confirmam primirea trimitand un ACK inapoi
        struct seq_udp ack_to_send;
        ack_to_send.seq = recv_packet.seq;
        strcpy(ack_to_send.payload, "ACK");
        ack_to_send.len = 4;
        sendto(sockfd, &ack_to_send, sizeof(ack_to_send), 0, (struct sockaddr *) &server_address, addr_len);

        if (strncmp(recv_packet.payload, "EXIT", 4) == 0) {
          printf("[CLIENT] Serverul s-a inchis. Iesire...\n");
          break;
        }
        printf("> %s", recv_packet.payload);
      }
    }

    // -------------------------------------------------------------
    // CAZ 2: Tastam o comanda (CANDIDATE, VOTE, LIST, EXIT)
    // -------------------------------------------------------------
    if (poll_fds[1].revents & POLLIN) {
      if (fgets(buf, sizeof(buf), stdin) && !isspace(buf[0])) {
        sent_packet.seq = seq;
        strcpy(sent_packet.payload, buf);
        sent_packet.len = strlen(sent_packet.payload) + 1;

        // Start-Stop pentru comanda curenta
        while (1) {
          sendto(sockfd, &sent_packet, sizeof(sent_packet), 0, (struct sockaddr *)&server_address, addr_len);
          rc = recvfrom(sockfd, &ack_recv, sizeof(ack_recv), 0, NULL, NULL);

          if (rc < 0) {
            printf("[CLIENT] Timeout la comanda! Retransmit...\n");
            continue;
          } else if (rc > 0) {
            if (strncmp(ack_recv.payload, "ACK", 3) == 0 && ack_recv.seq == sent_packet.seq) {
              seq++;
              break; // Serverul a prins comanda, va trimite raspunsul curand
            } else if (strncmp(ack_recv.payload, "ACK", 3) != 0) {
              // Serverul s-a miscat asa repede incat ne-a trimis direct raspunsul
              
              struct seq_udp ack_to_send;
              ack_to_send.seq = ack_recv.seq;
              strcpy(ack_to_send.payload, "ACK");
              ack_to_send.len = 4;
              sendto(sockfd, &ack_to_send, sizeof(ack_to_send), 0, (struct sockaddr *)&server_address, addr_len);
              
              if (strncmp(ack_recv.payload, "EXIT", 4) == 0) {
                printf("[CLIENT] Serverul se inchide...\n");
                exit(0);
              }
              
              printf("> %s", ack_recv.payload);
              seq++;
              break;
            }
          }
        }

        // Daca comanda noastra a fost EXIT si am reusit sa o trimitem, inchidem clientul local
        if (strncmp(buf, "EXIT", 4) == 0) {
          printf("[CLIENT] Se inchide...\n");
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
