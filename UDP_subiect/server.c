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

#include "common.h"
#include "utils.h"

#define SAVED_FILENAME "received_file.bin"

struct voter_account {
  int id;                  // 0, 1, 2...
  struct sockaddr_in addr; // Adresa lui, ca sa stim unde trimitem raspunsuri
  socklen_t addr_len;
  
  int is_candidate;        // 1 daca a dat CANDIDATE, altfel 0
  int has_voted;           // 1 daca a dat VOTE, altfel 0
  int votes_received;      // Numarul de voturi primite
  int is_online;           // 1 daca e activ, 0 daca a dat EXIT
};

struct voter_account clients[100];
int total_clients = 0; // Tine minte cati clienti s-au conectat (ne da si ID-ul urmator)

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

  poll_fds[0].fd = sockfd;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = STDIN_FILENO;
  poll_fds[1].events = POLLIN;

  printf("[SERVER] Pornește aplicația de votare...\n");

  while (1) {
    int rc = poll(poll_fds, 2, -1);
    DIE(rc < 0, "poll error");

    // ===================================================================
    // CAZUL A: SERVERUL TASTEAZA EXIT
    // ===================================================================
    if (poll_fds[1].revents & POLLIN) {
      char buf[128];
      if (fgets(buf, sizeof(buf), stdin)) {
        if (strncmp(buf, "EXIT", 4) == 0) {
          printf("[SERVER] SE ÎNCHIDE\n");
          strcpy(response_packet.payload, "EXIT");
          response_packet.len = strlen("EXIT") + 1;
          
          // Anuntam toti clientii online
          for (int i = 0; i < total_clients; i++) {
            if (clients[i].is_online == 1) {
              response_packet.seq = server_seq++;
              struct seq_udp ack_recv;
              int retries = 5;
              while(retries > 0) {
                sendto(sockfd, &response_packet, sizeof(response_packet), 0, (struct sockaddr *)&clients[i].addr, clients[i].addr_len);
                rc = recvfrom(sockfd, &ack_recv, sizeof(ack_recv), 0, NULL, NULL);
                if (rc > 0 && strncmp(ack_recv.payload, "ACK", 3) == 0 && ack_recv.seq == response_packet.seq) {
                  break; 
                }
                retries--;
              }
            }
          }
          return;
        }
      }
    }

    // ===================================================================
    // CAZUL B: AM PRIMIT UN PACHET DE LA UN CLIENT
    // ===================================================================
    if (poll_fds[0].revents & POLLIN) {
      struct sockaddr_in client_addr;
      socklen_t clen = sizeof(client_addr);
      
      rc = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&client_addr, &clen);
      
      // Ignoram ACK-urile ratacite
      if (rc > 0 && strncmp(recv_packet.payload, "ACK", 3) == 0) continue; 

      // 1. TRIMITEM ACK-ul INAPOI IMEDIAT
      struct seq_udp ack_to_send;
      ack_to_send.seq = recv_packet.seq;
      strcpy(ack_to_send.payload, "ACK");
      ack_to_send.len = 4;
      sendto(sockfd, &ack_to_send, sizeof(ack_to_send), 0, (struct sockaddr *)&client_addr, clen);

      // 2. IDENTIFICAM SAU CREAM CLIENTUL
      uint16_t client_port = ntohs(client_addr.sin_port);
      int client_idx = -1;
      
      for (int i = 0; i < total_clients; i++) {
        if (clients[i].addr.sin_port == client_addr.sin_port) {
          client_idx = i;
          break;
        }
      }

      // Client nou! Primeste ID incepand de la 0
      if (client_idx == -1) {
        client_idx = total_clients++;
        clients[client_idx].id = client_idx; 
        clients[client_idx].is_candidate = 0;
        clients[client_idx].has_voted = 0;
        clients[client_idx].votes_received = 0;
      }
      
      // Actualizam starea in caz ca a picat / revenit
      clients[client_idx].is_online = 1;
      clients[client_idx].addr = client_addr;
      clients[client_idx].addr_len = clen;

      int send_response = 0; 

      // 3. PARSAREA COMENZILOR DE VOTARE
      if (strncmp(recv_packet.payload, "INIT", 4) == 0) {
        sprintf(response_packet.payload, "Bun venit. Ai ID-ul %d.\n", clients[client_idx].id);
        send_response = 1;
        
      } else if (strncmp(recv_packet.payload, "CANDIDATE", 9) == 0) {
        if (clients[client_idx].is_candidate == 1) {
          sprintf(response_packet.payload, "Ai candidat deja. Ai %d voturi\n", clients[client_idx].votes_received);
        } else {
          clients[client_idx].is_candidate = 1;
          sprintf(response_packet.payload, "Am adaugat candidatura pentru id-ul %d\n", clients[client_idx].id);
        }
        send_response = 1;
        
      } else if (strncmp(recv_packet.payload, "VOTE", 4) == 0) {
        int id_votat;
        if (sscanf(recv_packet.payload, "VOTE %d", &id_votat) == 1) {
          if (clients[client_idx].has_voted == 1) {
            strcpy(response_packet.payload, "Ai votat deja. Nu o poti face inca o data\n");
          } else {
            // Verificam daca ID-ul votat e valid si e candidat
            if (id_votat >= 0 && id_votat < total_clients && clients[id_votat].is_candidate == 1) {
              clients[id_votat].votes_received++;
              clients[client_idx].has_voted = 1;
              sprintf(response_packet.payload, "Am adaugat un vot pentru clientul %d\n", id_votat);
            } else {
              strcpy(response_packet.payload, "ID-ul introdus nu exista sau nu este candidat!\n");
            }
          }
        }
        send_response = 1;
        
      } else if (strncmp(recv_packet.payload, "LIST", 4) == 0) {
        strcpy(response_packet.payload, "CANDIDAT        VOTE_COUNT\n");
        char line[128];
        for (int i = 0; i < total_clients; i++) {
          if (clients[i].is_candidate == 1) {
            // Formatam frumos cu tab-uri, cum cere in exemplu
            sprintf(line, "%d\t\t%d\n", clients[i].id, clients[i].votes_received);
            strcat(response_packet.payload, line);
          }
        }
        strcat(response_packet.payload, "SFARSIT\n");
        send_response = 1;
        
      } else if (strncmp(recv_packet.payload, "EXIT", 4) == 0) {
        clients[client_idx].is_online = 0;
        send_response = 0; 
      }

      // 4. TRIMITEREA RASPUNSULUI CATRE CLIENT (Start-Stop robust)
      if (send_response == 1) {
        response_packet.seq = server_seq++;
        response_packet.len = strlen(response_packet.payload) + 1;
        
        struct seq_udp ack_recv;
        int retries = 5;
        while(retries > 0) {
          sendto(sockfd, &response_packet, sizeof(response_packet), 0, (struct sockaddr *)&client_addr, clen);
          rc = recvfrom(sockfd, &ack_recv, sizeof(ack_recv), 0, NULL, NULL);
          
          if (rc < 0) {
            retries--; 
            continue; 
          } else if (rc > 0 && strncmp(ack_recv.payload, "ACK", 3) == 0 && ack_recv.seq == response_packet.seq) {
            break; 
          } else if (rc > 0 && strncmp(ack_recv.payload, "ACK", 3) != 0) {
            // Tratam cazul in care clientul trimite iar comanda pt ca a pierdut ACK-ul nostru
            struct seq_udp a; a.seq = ack_recv.seq; strcpy(a.payload, "ACK"); a.len = 4;
            sendto(sockfd, &a, sizeof(a), 0, (struct sockaddr *)&client_addr, clen);
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

  uint16_t port = atoi(argv[1]);
  int sockfd;
  struct sockaddr_in servaddr;

  // Creating socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Make ports reusable, in case we run this really fast two times in a row
  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

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
  int rc = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
  DIE(rc < 0, "bind failed");

  // TODO 1.0: Study the code. Uncoment this to receive a file chuck by chuck
  // and save it locally
  // recv_a_message(sockfd);
  // recv_a_file(sockfd, SAVED_FILENAME);

  run_server(sockfd);

  return 0;
}
