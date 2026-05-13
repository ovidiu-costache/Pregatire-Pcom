/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP
 * Echo Server
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

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32

struct clienti {
  int id;
  int stare;
  int mesaje;
};
struct clienti clienti[1024];

// Primeste date de pe connfd1 si trimite mesajul receptionat pe connfd2
int receive_and_send(int connfd1, int connfd2, size_t len) {
  int bytes_received;
  char buffer[len];

  // Primim exact len octeti de la connfd1
  bytes_received = recv_all(connfd1, buffer, len);
  // S-a inchis conexiunea
  if (bytes_received == 0) {
    return 0;
  }
  DIE(bytes_received < 0, "recv");

  // Trimitem mesajul catre connfd2
  int rc = send_all(connfd2, buffer, len);
  if (rc <= 0) {
    perror("send_all");
    return -1;
  }

  return bytes_received;
}

void run_chat_server(int listenfd) {
  struct sockaddr_in client_addr1;
  struct sockaddr_in client_addr2;
  socklen_t clen1 = sizeof(client_addr1);
  socklen_t clen2 = sizeof(client_addr2);

  int connfd1 = -1;
  int connfd2 = -1;
  int rc;

  // Setam socket-ul listenfd pentru ascultare
  rc = listen(listenfd, 2);
  DIE(rc < 0, "listen");

  // Acceptam doua conexiuni
  printf("Astept conectarea primului client...\n");
  connfd1 = accept(listenfd, (struct sockaddr *)&client_addr1, &clen1);
  DIE(connfd1 < 0, "accept");

  printf("Astept connectarea clientului 2...\n");
  connfd2 = accept(listenfd, (struct sockaddr *)&client_addr2, &clen2);
  DIE(connfd2 < 0, "accept");

  while (1) {
    printf("Primesc de la 1 si trimit catre 2...\n");
    int rc = receive_and_send(connfd1, connfd2, sizeof(struct chat_packet));
    if (rc <= 0) {
      break;
    }

    printf("Primesc de la 2 si trimit catre 1...\n");
    rc = receive_and_send(connfd2, connfd1, sizeof(struct chat_packet));
    if (rc <= 0) {
      break;
    }
  }

  // Inchidem conexiunile si socketii creati
  close(connfd1);
  close(connfd2);
}

void run_chat_multi_server(int listenfd) {

  struct pollfd poll_fds[MAX_CONNECTIONS];
  int num_sockets = 1;
  int rc;

  struct chat_packet received_packet;

  // Setam socket-ul listenfd pentru ascultare
  rc = listen(listenfd, MAX_CONNECTIONS);
  DIE(rc < 0, "listen");

  // Adaugam noul file descriptor (socketul pe care se asculta conexiuni) in
  // multimea poll_fds
  poll_fds[0].fd = listenfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = STDIN_FILENO;
  poll_fds[1].events = POLLIN;

  num_sockets++;
  /*
    TODO 3: Adaugati un timerfd. Read-ul pe el se va debloca periodic, moment
    in care veti trimite anuntul promotional catre toti clientii.
  */

  while (1) {
    // Asteptam sa primim ceva pe unul dintre cei num_sockets socketi
    rc = poll(poll_fds, num_sockets, -1);
    DIE(rc < 0, "poll");

    for (int i = 0; i < num_sockets; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == listenfd) {
          // Am primit o cerere de conexiune pe socketul de listen, pe care
          // o acceptam
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);
          const int newsockfd =
              accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "accept");

          struct chat_packet res;
          memset(&res, 0, sizeof(res));

          sprintf(res.message, "Bine ai venit ai id-ul: %d\n", newsockfd);
          res.len = strlen(res.message) + 1;
          send_all(newsockfd, &res, sizeof(res));
          
          for (int k = 0; k < 1024; k++) {
              if (clienti[k].id == 0) { // 0 înseamnă loc gol
                  clienti[k].id = newsockfd;
                  clienti[k].stare = 1;
                  clienti[k].mesaje = 0;
                  break; // Ne oprim imediat ce am găsit un loc
              }
          }
          
          // Adaugam noul socket intors de accept() la multimea descriptorilor
          // de citire
          poll_fds[num_sockets].fd = newsockfd;
          poll_fds[num_sockets].events = POLLIN;
          num_sockets++;

          printf("Noua conexiune de la %s, port %d, socket client %d\n",
                 inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
                 newsockfd);
        } else if (poll_fds[i].fd == STDIN_FILENO) {
            char buf[10];
            fgets(buf, sizeof(buf), stdin);

            if (strncmp(buf, "EXIT", 4) == 0) {
              printf("Serverul se inchide\n");
              return;
            }
            printf("comanda gresita in server\n");
        } else {
          // Am primit date pe unul din socketii de client, asa ca le receptionam
          int rc = recv_all(poll_fds[i].fd, &received_packet,
                            sizeof(received_packet));
          DIE(rc < 0, "recv");

          if (rc == 0) {
            printf("Socket-ul client %d a inchis conexiunea\n", i);
            close(poll_fds[i].fd);

            // Scoatem din multimea de citire socketul inchis
            for (int j = i; j < num_sockets - 1; j++) {
              poll_fds[j] = poll_fds[j + 1];

            }

            i--;
            num_sockets--;
          } else {
            printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n",
                   poll_fds[i].fd, received_packet.message);
            /* TODO 2.1: Trimite mesajul catre toti ceilalti clienti */
            char *msg = received_packet.message;
            struct chat_packet res;
            res.message[0] = '\0';

            // Găsim indexul real al clientului EXPEDITOR în baza de date
            int idx_expeditor = -1;
            for (int k = 0; k < 1024; k++) {
                if (clienti[k].id == poll_fds[i].fd) {
                    idx_expeditor = k;
                    break;
                }
            }

            // --- COMANDA LIST ---
            if (strncmp(msg, "LIST", 4) == 0) {
              strcpy(res.message, "ID_CLIENT STARE MESAJE\n");
              // Parcurgem baza de date clienti, nu poll_fds!
              for (int j = 0; j < 1024; j++) {
                if (clienti[j].id != 0) { // Daca a fost creat vreodata
                  char tmp[50];
                  char status[10];
                  strcpy(status, clienti[j].stare == 1 ? "ONLINE" : "OFFLINE");
                  
                  sprintf(tmp, "%d %s %d\n", clienti[j].id, status, clienti[j].mesaje);
                  strcat(res.message, tmp);
                }
              }
              strcat(res.message, "SFARSIT\n");
              res.len = strlen(res.message) + 1;
              send_all(poll_fds[i].fd, &res, sizeof(res));
            } 
            
            // --- COMANDA EXIT ---
            else if (strncmp(msg, "EXIT", 4) == 0) {
                printf("Clientul %d se deconecteaza.\n", poll_fds[i].fd);
                clienti[idx_expeditor].stare = 0; // Il marcam OFFLINE
                
                close(poll_fds[i].fd);
                
                // Il scoatem din poll_fds (shiftam la stanga)
                for (int j = i; j < num_sockets - 1; j++) {
                    poll_fds[j] = poll_fds[j + 1];
                }
                num_sockets--;
                i--; 
            } 
            
            // --- COMANDA BCAST ---
            else if (strncmp(msg, "BCAST", 5) == 0) {
                // Sarim peste "BCAST " (6 caractere) folosind pointeri
                char *mesaj_text = msg + 6; 
                
                sprintf(res.message, "BCAST %d: %s", clienti[idx_expeditor].id, mesaj_text);
                res.len = strlen(res.message) + 1;

                for (int j = 1; j < num_sockets; j++) {
                    if (j != i) { // Trimitem tuturor celor activi in poll, mai putin lui
                        send_all(poll_fds[j].fd, &res, sizeof(res));
                    }
                }
                clienti[idx_expeditor].mesaje++; // Incrementam nr de mesaje!
            } 
            
            // --- COMANDA PRIV ---
            else if (strncmp(msg, "PRIV", 4) == 0) {
                int id_destinatar;
                // %*s sare peste "PRIV", %d ia ID-ul, %[^\n] ia restul propozitiei
                char mesaj_text[200];
                sscanf(msg, "%*s %d %[^\n]", &id_destinatar, mesaj_text);
                
                // Cautam destinarul in poll_fds pentru a vedea daca e ONLINE si pentru a-i afla pozitia (fd-ul)
                int gasit_online = 0;
                int fd_destinatar = -1;
                
                for (int j = 1; j < num_sockets; j++) {
                    if (poll_fds[j].fd == id_destinatar) {
                        gasit_online = 1;
                        fd_destinatar = poll_fds[j].fd;
                        break;
                    }
                }

                if (gasit_online) {
                    // Trimitem mesajul destinatarului
                    sprintf(res.message, "PRIV %d: %s\n", clienti[idx_expeditor].id, mesaj_text);
                    res.len = strlen(res.message) + 1;
                    send_all(fd_destinatar, &res, sizeof(res));
                    
                    // Confirmam expeditorului
                    struct chat_packet conf;
                    strcpy(conf.message, "Mesaj trimis cu succes.\n");
                    conf.len = strlen(conf.message) + 1;
                    send_all(poll_fds[i].fd, &conf, sizeof(conf));
                    
                    clienti[idx_expeditor].mesaje++; // Incrementam nr de mesaje!
                } else {
                    // Eroare
                    sprintf(res.message, "Clientul %d nu este conectat.\n", id_destinatar);
                    res.len = strlen(res.message) + 1;
                    send_all(poll_fds[i].fd, &res, sizeof(res));
                }
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

  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru receptionarea conexiunilor
  const int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(listenfd < 0, "socket");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
  // rulam de 2 ori rapid
  // Vezi https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux
  const int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  //rc = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
  //DIE(rc <= 0, "inet_pton");
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Asociem adresa serverului cu socketul creat folosind bind
  rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "bind");

  /*
    TODO 2.1: Folositi implementarea cu multiplexare
  */
  //run_chat_server(listenfd);
   run_chat_multi_server(listenfd);

  // Inchidem listenfd
  close(listenfd);

  return 0;
}
// Să se implementeze o aplicație client-server folosind TCP, pentru simularea unei
// aplicații de chat cu mesaje private și broadcast. În această aplicație:
// 1. Clienții pot trimite oricând mesaj unui anume client sau tuturor celorlalți folosind
// comenzile PRIV și BCAST. În plus, se va implementa comenzile LIST și EXIT.
// 2. Serverul va permite conectarea unui număr variabil de clienți, care pot trimite
// oricând comenzi pentru care vor vizualiza un mesaj de confirmare;
// 3. Clienții vor fi identificați prin numărul întreg asociat file descriptorului primit de
// socket-ul clientului în server;
// 4. Serverul va asculta pe toate adresele IP asociate mașinii pe care rulează și se va
// rula folosind comanda:
// ./server PORT_ASCULTARE
// Exemplu: ./server 8080
// 5. Clientul va comunica direct doar cu serverul și va fi pornit folosind comanda:
// ./client IP_SERVER PORT_SERVER
// Exemplu: ./client 127.0.0.1 8080
// 6. La pornire, fiecare client va afișa ID-ul său în cadrul aplicației;
// 7. Toți clienții se vor închide în cazul în care serverul se deconectează. Singura
// comandă permisă pe server va fi EXIT.
// Toate comenzile se vor introduce de la STDIN, iar singurele comenzi permise sunt
// următoarele:
// BCAST Mesaj
// - Va trimite mesajul către server pentru a fi trimis către toți clienții conectați.
// - Toți clienții vor afișa mesajul de forma: „BCAST %d: %s”, unde %d este
// ID-ul celui care a trimis mesajul.
// PRIV ID_CLIENT Mesaj
// - Va trimite mesajul către server pentru a fi trimis către toți clienții conectați.
// - Clientul destinatar va afisa mesajul de forma: „PRIV %d: %s”
// - În inițiator va avea ca rezultat:
// - Afișarea “Mesaj trimis cu succes.” și trimiterea mesajului sau
// Pagina 1 din 3
// - Afișarea erorii “Clinetul %d nu este conectat”.
// LIST
// Lista tuturor clienților, alături de starea lor (ONLINE sau OFFLINE) și numărul de
// mesaje trimise de fiecare:
// ID_CLIENT STARE MESAJE
// 3 ONLINE 4
// 4 ONLINE 0
// 5 ONLINE 2
// SFARSIT
// EXIT
// Închide clientul curent, având ca rezultat eliberarea tuturor resurselor alocate.
// Atenție! Pentru a fi notată, implementarea trebuie să facă separarea corectă a
// mesajelor (conform indicațiilor din laboratorul 7) și să folosească în mod corect API-ul
// pentru sockeți TCP.
// Un exemplu de rulare ar putea fi următorul:
// [Server] PORNESTE
// [Client1] PORNESTE
// [Client1] > Bun venit. Ai ID-ul 3.
// [Client2] PORNESTE
// [Client2] > Bun venit. Ai ID-ul 4.
// [Client3] PORNESTE
// [Client3] > Bun venit. Ai ID-ul 5.
// [Client2] < PRIV 5 Salut prietene!
// [Client3] > PRIV 3: Salut prietene!
// [Client2] > Mesaj trimis cu succes.
// [Client2] < LIST
// [Client2] > ID_CLIENT STARE MESAJE
// [Client2] > 3 ONLINE 1
// [Client2] > 4 ONLINE 0
// [Client2] > 5 ONLINE 0
// [Client2] > SFARSIT
// [Client2] < BCAST A terminat cineva testul?
// [Client1] > BCAST 3: A terminat cineva testul?
// [Client3] > BCAST 3: A terminat cineva testul?
// [Client1] < EXIT
// [Client1] SE ÎNCHIDE
// [Server] > EXIT
// [Client2] SE ÎNCHIDE
// [Client3] SE ÎNCHIDE
// [Server] SE ÎNCHIDE