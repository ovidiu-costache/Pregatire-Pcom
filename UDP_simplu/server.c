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

struct date_client {
    int id;
    int port_sursa;
    struct sockaddr_in addr;

    int e_candidat; 
    int a_votat;    
    int voturi;     
};

struct date_client clienti[100];
int nr_clienti = 0;
int secv_server = 0;

// CORECTAT: scos ';' si pus '*' la client_addr
int recv_seq_udp(int sockfd, struct seq_udp *seq_packet, struct sockaddr_in *client_addr) {
  socklen_t clen = sizeof(*client_addr);

  // Receive a segment with seq_number seq_packet->seq
  int rc = recvfrom(sockfd, seq_packet, sizeof(struct seq_udp), 0,
                    (struct sockaddr *)client_addr, &clen);

  if (rc < 0) return -1;

  // Trimitem ACK
  int ack = seq_packet->seq;
  sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)client_addr, clen);
  
  return rc;
}

void send_msg_start_stop(int sockfd, struct sockaddr_in server_address,
                          const char *msg, int seq) { // CORECTAT: adaugat const
  struct seq_udp d;
  strcpy(d.payload, msg);
  d.len = strlen(msg) + 1;
  d.seq = seq;

  while (1) {
    sendto(sockfd, &d, sizeof(struct seq_udp), 0, (struct sockaddr *)&server_address, sizeof(server_address));

    int ack = -1;
    int rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

    if (rc >= 0 && ack == seq)
      break;
  }
}

void run_server(int sockfd) {
  // CORECTAT: Uniformizat numele la poll_fds
  struct pollfd poll_fds[2];

  poll_fds[0].fd = STDIN_FILENO;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = sockfd;
  poll_fds[1].events = POLLIN; // CORECTAT: typo pol_fds

  printf("[Server] PORNESTE\n");

  while (1) {
    int rc = poll(poll_fds, 2, -1);
    DIE(rc < 0, "poll");

    if (poll_fds[0].revents & POLLIN) {
        char buf[256];
        fgets(buf, sizeof(buf), stdin);
        if (strncmp(buf, "EXIT", 4) == 0) {
            // Anuntam toti clientii sa iasa
            for (int i = 0; i < nr_clienti; i++) {
                send_msg_start_stop(sockfd, clienti[i].addr, "EXIT", secv_server++);
            }
            printf("[Server] SE INCHIDE\n");
            break;
        }
    }

    // 2. Primim un pachet pe UDP de la clienti
    if (poll_fds[1].revents & POLLIN) {
        struct seq_udp p;
        struct sockaddr_in cli_addr;
        
        int rc = recv_seq_udp(sockfd, &p, &cli_addr);
        if (rc < 0) continue; // Timeout sau eroare, ignoram
        
        // --- LOGICA DE BUSINESS ---
        
        // A. Cauti clientul dupa portul sursa
        int idx = -1;
        for (int i = 0; i < nr_clienti; i++) {
             if (clienti[i].port_sursa == ntohs(cli_addr.sin_port)) idx = i;
        }
        
        // B. Daca e nou, il adaugi in array
        if (idx == -1) {
             idx = nr_clienti;
             clienti[idx].id = nr_clienti;
             clienti[idx].port_sursa = ntohs(cli_addr.sin_port);
             clienti[idx].addr = cli_addr;
             
             // Initializam datele de vot
             clienti[idx].e_candidat = 0;
             clienti[idx].a_votat = 0;
             clienti[idx].voturi = 0;
             
             nr_clienti++;
        }
        
        // C. Parsezi mesajul
        char raspuns[1024]; // CORECTAT: marit bufferul pentru lista
        memset(raspuns, 0, sizeof(raspuns));

        if (strncmp(p.payload, "HELLO", 5) == 0) {
             sprintf(raspuns, "ID:%d", clienti[idx].id);
        } 
        else if (strncmp(p.payload, "CANDIDATE", 9) == 0) {
             if (clienti[idx].e_candidat == 1) {
                 sprintf(raspuns, "Ai candidat deja. Ai %d voturi", clienti[idx].voturi);
             } else {
                 clienti[idx].e_candidat = 1;
                 sprintf(raspuns, "Am adaugat candidatura pentru id-ul %d", clienti[idx].id);
             }
        } 
        else if (strncmp(p.payload, "VOTE", 4) == 0) {
             int target_id;
             if (sscanf(p.payload, "VOTE %d", &target_id) == 1) { // CORECTAT: parsare mai sigura
                 if (clienti[idx].a_votat == 1) {
                     sprintf(raspuns, "Ai votat deja. Nu o poti face inca o data");
                 } 
                 else if (target_id < 0 || target_id >= nr_clienti || clienti[target_id].e_candidat == 0) {
                     sprintf(raspuns, "Eroare: Candidat invalid sau inexistent!");
                 } 
                 else {
                     clienti[idx].a_votat = 1;
                     clienti[target_id].voturi++;
                     sprintf(raspuns, "Am adaugat un vot pentru clientul %d", target_id);
                 }
             }
        } 
        else if (strncmp(p.payload, "LIST", 4) == 0) {
             strcpy(raspuns, "CANDIDAT       VOTE_COUNT\n");
             for (int i = 0; i < nr_clienti; i++) {
                 if (clienti[i].e_candidat == 1) {
                     char rand_tmp[50];
                     sprintf(rand_tmp, "%d              %d\n", clienti[i].id, clienti[i].voturi);
                     strcat(raspuns, rand_tmp);
                 }
             }
             strcat(raspuns, "SFARSIT");
        } 
        else if (strncmp(p.payload, "EXIT", 4) == 0) {
             continue; 
        } 
        else {
             sprintf(raspuns, "Comanda nerecunoscuta!");
        }
        
        // D. Raspundem clientului fiabil (folosind Start-Stop)
        send_msg_start_stop(sockfd, cli_addr, raspuns, secv_server++);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
      printf("Rulare: ./server <port>\n");
      return 1;
  }

  int sockfd;
  struct sockaddr_in servaddr;
  uint16_t port = atoi(argv[1]);

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  // CORECTAT: Adaugat declararea lui rc
  int rc = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
  DIE(rc < 0, "setsockopt");

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET; 
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(port);

  rc = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
  DIE(rc < 0, "bind failed");

  run_server(sockfd);

  return 0;
}