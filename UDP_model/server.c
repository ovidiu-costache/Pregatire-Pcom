#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>
#include <sys/poll.h>
#include <ctype.h>

#include "common.h"
#include "utils.h"

#define SAVED_FILENAME "received_file.bin"

struct bank_account {
    uint16_t port;
    int balance;
    int is_online;

    struct sockaddr_in addr;
    socklen_t addr_len;
};

struct bank_account accounts[100];
int num_accounts = 0;

// int recv_seq_udp(int sockfd, struct seq_udp *seq_packet, int expected_seq) {
//   struct sockaddr_in client_addr;
//   socklen_t clen = sizeof(client_addr);

//   // Receive a segment with seq_number seq_packet->seq
//   int rc = recvfrom(sockfd, seq_packet, sizeof(struct seq_udp), 0,
//                     (struct sockaddr *)&client_addr, &clen);

//   // TODO: Check if the sequence number is the expected one.

//   // TODO: If we got the expected packet (by seq) send ACK for the seq.packet
//   // and return the number of bytes read. 
//   // We will increase expected_seq in the calling function (recv_a_file(...))

//   // TODO: If segment is not with the expected number, send ACK
//   // for the last well received packet (expected_seq - 1) and return -1
// }

// void recv_a_file(int sockfd, char *filename) {
//   int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
//   DIE(fd < 0, "open");
//   struct seq_udp p;
//   int expected_seq = 0;
//   int rc;

//   while (1) {
//     // Receive a chunk
//     rc = recv_seq_udp(sockfd, &p, expected_seq);

//     // TODO: If rc == -1 => we didn't receive the expected segment. We continue (retry to receive the same chunk).

//     // TODO: If rc >=0 => we receive the expected segment. We increase expected_seq

//     // An empty payload means the file ended.
//     if (p.len == 0)
//       // Break if file ended
//       break;

//     // Write the chunk to the file
//     write(fd, p.payload, p.len);
//   }

//   close(fd);
// }

// void recv_a_message(int sockfd) {
//   // Receive a datagram and send an ACK
//   // The info of the who sent the datagram (PORT and IP)
//   struct sockaddr_in client_addr;
//   struct seq_udp p;
//   socklen_t clen = sizeof(client_addr);
//   int rc = recvfrom(sockfd, &p, sizeof(struct seq_udp), 0,
//                     (struct sockaddr *)&client_addr, &clen);

//   // We know it's a string so we print it
//   printf("[Server] Received: %s\n", p.payload);

//   int ack = 0;
//   // Sending ACK. We model ACK as datagrams with only an int of value 0.
//   rc = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr,
//               clen);
//   DIE(rc < 0, "send");
// }

void run_server(int sockfd) {
    struct pollfd poll_fds[2];
    struct seq_udp recv_packet;
    struct seq_udp response_packet;

    int server_seq = 0;
    int rc;

    poll_fds[0].fd = sockfd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = STDIN_FILENO;
    poll_fds[1].events = POLLIN;

    printf("[SERVER] Server-ul porneste...\n");

    while (1) {
        rc = poll(poll_fds, 2, -1);
        DIE(rc < 0, "poll");

        // Serverul primeste EXIT de la tastatura
        if (poll_fds[1].revents & POLLIN) {
            char buf[64];
            if (fgets(buf, sizeof(buf), stdin)) {
                if (strncmp(buf, "EXIT", 4) == 0) {
                    printf("[SERVER] Se inchide...\n");

                    strcpy(response_packet.payload, "EXIT");
                    response_packet.len = strlen(response_packet.payload) + 1;

                    // Trebuie anuntati toti clientii online
                    for (int i = 0; i < num_accounts; i++) {
                        if (accounts[i].is_online == 1) {
                            response_packet.seq = server_seq;
                            server_seq++;

                            struct seq_udp ack_recv;
                            int retries = 5;

                            while (retries > 0) {
                                sendto(sockfd, &response_packet, sizeof(response_packet), 0, (struct sockaddr *)&accounts[i].addr, accounts[i].addr_len);
                                rc = recvfrom(sockfd, &ack_recv, sizeof(ack_recv), 0, NULL, NULL);
                            
                                if (rc > 0 && strncmp(ack_recv.payload, "ACK", 3) == 0 && ack_recv.seq == response_packet.seq) {
                                    break; // A confirmat ca a primit EXIT
                                }
                                retries--;
                            }
                        }
                    }
                    return;
                }
            }
        }

        // Primeste pachet pe retea de la un client
        if (poll_fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t clen = sizeof(client_addr);
            
            rc = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &clen);
            
            // Ignoram ACK-urile ratacite/intarziate
            if (rc > 0 && strncmp(recv_packet.payload, "ACK", 3) == 0) {
                continue;
            }

            // TRUCUL SALVATOR: Trimitem instant ACK clientului pt comanda pe care abia a trimis-o!
            struct seq_udp ack_to_send;
            ack_to_send.seq = recv_packet.seq;
            strcpy(ack_to_send.payload, "ACK");
            ack_to_send.len = 4;
            sendto(sockfd, &ack_to_send, sizeof(ack_to_send), 0, (struct sockaddr *)&client_addr, clen);

            // --- Identificare Client ---
            uint16_t client_port = ntohs(client_addr.sin_port);
            int client_idx = -1;
            
            // Il cautam in baza de date
            for (int i = 0; i < num_accounts; i++) {
                if (accounts[i].port == client_port) {
                    client_idx = i;
                    break;
                }
            }

            // Daca nu exista, e un client nou, ii facem cont acum!
            if (client_idx == -1) {
                client_idx = num_accounts;
                accounts[client_idx].port = client_port;
                accounts[client_idx].balance = 100;
                num_accounts++;
            }
            
            // Actualizam starea (in caz ca a revenit) si adresa de rutare
            accounts[client_idx].is_online = 1;
            accounts[client_idx].addr = client_addr;
            accounts[client_idx].addr_len = clen;

            int send_response = 0; // Flag pt a stii daca ii mai trimitem ceva inapoi

            // --- Parsarea Comenzilor ---
            if (strncmp(recv_packet.payload, "INIT", 4) == 0) {
                sprintf(response_packet.payload, "Bun venit. Ai ID-ul %u.\n", client_port);
                send_response = 1;
                
            } else if (strncmp(recv_packet.payload, "LIST", 4) == 0) {
                strcpy(response_packet.payload, "CONT BALANȚĂ STARE\n");
                char line[128];
                for (int i = 0; i < num_accounts; i++) {
                    sprintf(line, "%u %d %s\n", accounts[i].port, accounts[i].balance, accounts[i].is_online ? "ONLINE" : "OFFLINE");
                    strcat(response_packet.payload, line);
                }
                send_response = 1;
                
            } else if (strncmp(recv_packet.payload, "TRANSFER", 8) == 0) {
                uint16_t id_dest;
                int suma;
                
                if (sscanf(recv_packet.payload, "TRANSFER %hu %d", &id_dest, &suma) == 2) {
                    // Cautam destinatarul
                    int dest_idx = -1;
                    for (int i = 0; i < num_accounts; i++) { 
                        if (accounts[i].port == id_dest) { dest_idx = i; break; } 
                    }
                    
                    if (dest_idx == -1) {
                        sprintf(response_packet.payload, "Contul către care dorești să transferi (%u) nu există.\n", id_dest);
                    } else if (accounts[client_idx].balance < suma) {
                        sprintf(response_packet.payload, "Balanța ta (%d) este mai mică decât suma ce se dorește a fi transferată.\n", accounts[client_idx].balance);
                    } else {
                        // Matematica e buna, facem transferul!
                        accounts[client_idx].balance -= suma;
                        accounts[dest_idx].balance += suma;
                        sprintf(response_packet.payload, "Ai transferat %d u.m. către contul %u. Noua ta balanță este %d.\n", suma, id_dest, accounts[client_idx].balance);
                    }
                }
                send_response = 1;
                
            } else if (strncmp(recv_packet.payload, "EXIT", 4) == 0) {
                accounts[client_idx].is_online = 0;
                send_response = 0; // A plecat, nu ii mai trimitem nimic
            }

            // --- Trimiterea Raspunsului catre client (cu Start-Stop) ---
            if (send_response == 1) {
                response_packet.seq = server_seq++;
                response_packet.len = strlen(response_packet.payload) + 1;
                
                struct seq_udp ack_recv;
                int retries = 5;
                
                while (retries > 0) {
                    sendto(sockfd, &response_packet, sizeof(response_packet), 0, (struct sockaddr *)&client_addr, clen);
                    rc = recvfrom(sockfd, &ack_recv, sizeof(ack_recv), 0, NULL, NULL);
                    
                    if (rc < 0) {
                        retries--; 
                        continue; 
                    } else if (rc > 0) {
                        if (strncmp(ack_recv.payload, "ACK", 3) == 0 && ack_recv.seq == response_packet.seq) {
                            break; // Clientul a primit raspunsul nostru
                        } else if (strncmp(ack_recv.payload, "ACK", 3) != 0) {
                            // Clientul n-a primit ACK-ul nostru la comanda initiala si a retrimis comanda!
                            // Ii retimitem ACK-ul pt comanda lui ca sa il deblocam.
                            struct seq_udp a; 
                            a.seq = ack_recv.seq; 
                            strcpy(a.payload, "ACK"); 
                            a.len = 4;
                            sendto(sockfd, &a, sizeof(a), 0, (struct sockaddr *)&client_addr, clen);
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("\n Usage: %s <port>\n", argv[0]);
    return 1;
  }

  int sockfd;
  struct sockaddr_in servaddr;

  uint16_t port = atoi(argv[1]);

  // Creating socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Make ports reusable, in case we run this really fast two times in a row
  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  // Set the timeout on the socket
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  int rc = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  DIE(rc < 0, "setsockopt");

  // Fill the details on what destination port should the
  // datagrams have to be sent to our process.
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET; // IPv4
  // 0.0.0.0, basically match any IP
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(port);

  // Bind the socket with the server address. The OS networking
  // implementation will redirect to us the contents of all UDP
  // datagrams that have our port as destination
  rc = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
  DIE(rc < 0, "bind failed");

  // TODO 1.0: Study the code. Uncoment this to receive a file chuck by chuck
  // and save it locally
  // recv_a_message(sockfd);
  // recv_a_file(sockfd, SAVED_FILENAME);

  run_server(sockfd);

  return 0;
}
