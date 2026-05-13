/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP (Arhitectura Hibrida TCP+UDP)
 * server.c
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/poll.h>
#include <ctype.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32

struct bank_account {
  int id;
  int balance;
  char history[4096];
};

// Vector global pentru a tine minte conturile (folosim socket-ul ca ID/index)
struct bank_account accounts[1024];

void run_chat_multi_server(int listenfd, int udpfd) {
  struct pollfd poll_fds[MAX_CONNECTIONS];
  int num_sockets;
  int rc;

  struct chat_packet received_packet;

  // Setam socket-ul listenfd pentru ascultare
  rc = listen(listenfd, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // Initializam vectorul de poll cu cele 3 surse principale
  poll_fds[0].fd = listenfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = udpfd;
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = STDIN_FILENO;
  poll_fds[2].events = POLLIN;

  num_sockets = 3;

  while (1) {
    rc = poll(poll_fds, num_sockets, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_sockets; i++) {
      if (poll_fds[i].revents & POLLIN) {
        
        // -------------------------------------------------------------
        // 1. CLIENT NOU PE TCP
        // -------------------------------------------------------------
        if (poll_fds[i].fd == listenfd) {
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          const int newsockfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");

          poll_fds[num_sockets].fd = newsockfd;
          poll_fds[num_sockets].events = POLLIN;
          num_sockets++;

          // Cream noul cont si setam balanta la 100
          accounts[newsockfd].id = newsockfd;
          accounts[newsockfd].balance = 100;
          strcpy(accounts[newsockfd].history, "");

          // Trimit mesaj de confirmare cu ID-ul clientului
          struct chat_packet response;
          sprintf(response.message, "Bine ai venit! ID-ul tau este %d.\n", newsockfd);
          response.len = strlen(response.message) + 1;
          send_all(newsockfd, &response, sizeof(response));

          printf("Noua conexiune TCP de la %s, port %d, socket client %d\n",
                 inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
        
        // -------------------------------------------------------------
        // 2. PACHET PRIMIT PE UDP (SOLD_CURENT)
        // -------------------------------------------------------------
        } else if (poll_fds[i].fd == udpfd) {
          struct chat_packet sent_packet;
          struct chat_packet response;
          struct sockaddr_in client_addr;
          socklen_t clen = sizeof(client_addr);
          int id = 0;

          rc = recvfrom(udpfd, &sent_packet, sizeof(sent_packet), 0, (struct sockaddr *)&client_addr, &clen);
          DIE(rc < 0, "recvfrom");

          // Extrag id-ul din pachet. Caut separatorul '|'
          char *id_ptr = strchr(sent_packet.message, '|');
          if (id_ptr != NULL) {
            sscanf(id_ptr + 1, "%d", &id);
            printf("Interogare UDP pentru ID-ul: %d\n", id);

            // Trimit inapoi clientului soldul curent prin UDP
            sprintf(response.message, "Soldul curent: %d RON\n", accounts[id].balance);
            response.len = strlen(response.message) + 1;

            rc = sendto(udpfd, &response, sizeof(response), 0, (struct sockaddr *)&client_addr, clen);
            DIE(rc < 0, "sendto");
          }

        // -------------------------------------------------------------
        // 3. TASTATURA SERVERULUI
        // -------------------------------------------------------------
        } else if (poll_fds[i].fd == STDIN_FILENO) {
          char buf[128];
          if (fgets(buf, sizeof(buf), stdin)) {
            if (strncmp(buf, "EXIT", 4) == 0) {
              printf("[SERVER] Se inchide...\n");
              return;
            }
          }

        // -------------------------------------------------------------
        // 4. MESAJE DE LA CLIENTII DEJA CONECTATI PRIN TCP
        // -------------------------------------------------------------
        } else {
          int rc = recv_all(poll_fds[i].fd, &received_packet, sizeof(received_packet));
          DIE(rc < 0, "recv");

          if (rc == 0) {
            printf("Socket-ul client %d a inchis conexiunea\n", poll_fds[i].fd);
            accounts[poll_fds[i].fd].id = 0; // Marcam contul ca inactiv
            close(poll_fds[i].fd);

            // Scoatem din multimea de poll
            for (int j = i; j < num_sockets - 1; j++) {
              poll_fds[j] = poll_fds[j + 1];
            }
            i--;
            num_sockets--;
          } else {
            // Declaratiile pentru procesarea comenzii
            int my_fd = poll_fds[i].fd;
            struct chat_packet response;
            memset(&response, 0, sizeof(response));

            // Curatam \n daca exista in mesajul primit
            received_packet.message[strcspn(received_packet.message, "\n")] = 0;

            // --- COMANDA TRANSFER <ID_DEST> <SUMA> ---
            if (strncmp(received_packet.message, "TRANSFER", 8) == 0) {
              int id_dest = 0, suma = 0;
              if (sscanf(received_packet.message, "TRANSFER %d %d", &id_dest, &suma) == 2) {
                
                if (id_dest < 0 || id_dest >= 1024 || accounts[id_dest].id == 0) {
                  strcpy(response.message, "Eroare: Destinatarul nu exista sau nu este conectat.\n");
                } else if (accounts[my_fd].balance < suma) {
                  strcpy(response.message, "Eroare: Fonduri insuficiente.\n");
                } else {
                  // Executam transferul
                  accounts[my_fd].balance -= suma;
                  accounts[id_dest].balance += suma;

                  // Actualizam istoricul pentru amandoi
                  char log_sursa[100], log_dest[100];
                  sprintf(log_sursa, "Trimis %d catre ID %d\n", suma, id_dest);
                  sprintf(log_dest, "Primit %d de la ID %d\n", suma, my_fd);
                  strcat(accounts[my_fd].history, log_sursa);
                  strcat(accounts[id_dest].history, log_dest);

                  sprintf(response.message, "Succes! Soldul tau nou: %d RON\n", accounts[my_fd].balance);
                }
              } else {
                  strcpy(response.message, "Format invalid. Foloseste: TRANSFER <ID> <Suma>\n");
              }
            } 
            // --- COMANDA LIST ---
            else if (strncmp(received_packet.message, "LIST", 4) == 0) {
              if (strlen(accounts[my_fd].history) > 0) {
                strcpy(response.message, accounts[my_fd].history);
              } else {
                strcpy(response.message, "Nu ai nicio tranzactie in istoric.\n");
              }
            }
            // --- COMANDA EXIT ---
            else if (strncmp(received_packet.message, "EXIT", 4) == 0) {
                printf("Clientul %d s-a deconectat voluntar.\n", my_fd);
                accounts[my_fd].id = 0;
                close(my_fd);
                for (int j = i; j < num_sockets - 1; j++) {
                  poll_fds[j] = poll_fds[j + 1];
                }
                i--; 
                num_sockets--;
                continue; // Sarim peste send_all
            }
            else {
              sprintf(response.message, "Comanda necunoscuta\n");
            }

            // Trimitem raspunsul la operatiunea TCP
            response.len = strlen(response.message) + 1;
            send_all(my_fd, &response, sizeof(response));
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

  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP si unul UDP
  const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(listenfd < 0, "socket tcp");

  const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udpfd < 0, "socket udp");

  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  const int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Asociem adresa serverului cu ambele socket-uri create
  rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind tcp");

  rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind udp");

  // Rulam logica de server
  run_chat_multi_server(listenfd, udpfd);

  // Inchidem ce a mai ramas
  close(listenfd);
  close(udpfd);

  return 0;
}