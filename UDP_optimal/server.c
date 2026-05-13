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

// Baza de date a clientilor
struct date_client {
    int id;
    int port_sursa;
    struct sockaddr_in addr; // Trebuie sa tinem minte adresa lui
    
    // Variabilele adaugate pentru aplicatia de Vot:
    int e_candidat; 
    int a_votat;    
    int voturi;     
};

struct date_client clienti[100];
int nr_clienti = 0;
int secv_server = 0; // Pentru a numerota raspunsurile serverului

// Functie de receptie si trimitere imediata a ACK-ului
int recv_seq_udp(int sockfd, struct seq_udp *seq_packet, struct sockaddr_in *client_addr) {
  socklen_t clen = sizeof(*client_addr);

  // Receive a segment 
  int rc = recvfrom(sockfd, seq_packet, sizeof(struct seq_udp), 0,
                    (struct sockaddr *)client_addr, &clen);
  if (rc < 0) return -1; // Timeout

  // Trimitem ACK pentru ORICE secventa primim (Start-Stop)
  int ack = seq_packet->seq;
  sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)client_addr, clen);
  
  return rc;
}

// Functia Start-Stop pentru a trimite raspunsuri catre client
void send_msg_start_stop(int sockfd, struct sockaddr_in server_address, const char *msg, int seq) {
    struct seq_udp d;
    strcpy(d.payload, msg);
    d.len = strlen(msg) + 1;
    d.seq = seq;

    while (1) {
        // Send the datagram
        sendto(sockfd, &d, sizeof(struct seq_udp), 0,
               (struct sockaddr *)&server_address, sizeof(server_address));

        // Wait for ACK
        int ack = -1;
        int rc = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

        if (rc >= 0 && ack == seq) {
            break; // Am primit ACK-ul corect! Iesim din bucla
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

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  // Timeout vital pentru functionalitatea Start-Stop
  struct timeval tv = {0, 250000};
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(atoi(argv[1])); // PORT luat din terminal

  int rc = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
  DIE(rc < 0, "bind failed");

  struct pollfd fds[2];
  fds[0].fd = STDIN_FILENO; fds[0].events = POLLIN;
  fds[1].fd = sockfd;       fds[1].events = POLLIN;

  printf("[Server] PORNESTE\n");

  while(1) {
    poll(fds, 2, -1);
    
    // 1. Iesire Server (Comanda de la tastatura)
    if (fds[0].revents & POLLIN) {
        char buf[256];
        fgets(buf, sizeof(buf), stdin);
        if (strncmp(buf, "EXIT", 4) == 0) {
            // Anuntam toti clientii sa iasa (Cerinta 6)
            for (int i = 0; i < nr_clienti; i++) {
                send_msg_start_stop(sockfd, clienti[i].addr, "EXIT", secv_server++);
            }
            printf("[Server] SE INCHIDE\n");
            break;
        }
    }
    
    // 2. Primim un pachet pe UDP de la clienti
    if (fds[1].revents & POLLIN) {
        struct seq_udp p;
        struct sockaddr_in cli_addr;
        
        int rc = recv_seq_udp(sockfd, &p, &cli_addr);
        if (rc < 0) continue; // Timeout sau eroare, ignoram
        
        // --- LOGICA DE BUSINESS (Asemanatoare cu TCP) ---
        
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
             
             // Initializam datele de vot!
             clienti[idx].e_candidat = 0;
             clienti[idx].a_votat = 0;
             clienti[idx].voturi = 0;
             
             nr_clienti++;
        }
        
        // C. Parsezi mesajul (p.payload)
        char raspuns[256];
        memset(raspuns, 0, sizeof(raspuns)); // Curatam mereu bufferul

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
             sscanf(p.payload, "%*s %d", &target_id);
             
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
             // Clientul ne anunta ca iese. UDP e stateless, deci doar ignoram mai departe.
             continue; 
        } 
        else {
             sprintf(raspuns, "Comanda nerecunoscuta!");
        }
        
        // D. Raspundem clientului fiabil (folosind Start-Stop)
        send_msg_start_stop(sockfd, cli_addr, raspuns, secv_server++);
    }
  }
  
  close(sockfd);
  return 0;
}
// sa se implementeze o aplicatie client-server folosind UDP, pt simularea unei aplicatii votare. in aceasta aplicatie:
// 1. serverul va permite conectarea unui numar variabil de clienti care pot trimite oricand comenzi pt care vor 
// vizualiza un mesaj de confirmare doar dupa ce ele sunt procesate
// 2. clientii vor fi identificati prin cate un nr intreg asociat secvential de catre server incepand de la zero.
// 3. serverul va asculta toate adresele IP asociate masinii pe care ruleaza si se va rula folosind comanda:
// ./server PORT_ASCULTARE
// 4. clientul va vomunica direct doar cu serverul si va fi pornit folosind comanda:
// ./client IP_SERVER PORT_SERVER
// 5. la pornire, fiecare client va afisa ID-ul sau in cadrul aplicatiei.
// 6. toti clientii se vor inchide in cazul in care serverul se deconecteaza. singura permisa pe server la fi EXIT.

// toate comenzile se vor introduce de la STDIN, iar singurele comenzi permise sunt urmatoarele:
// CANDIDATE
// - va adauga clientul curent in lista de candidati. atentie, nu toti clientii conectati sunt considerati candidati
// - se va afisa un mesaj de forma:
//     - am adaugat candidatura pentru id-ul %d sau
//     - ai candidat deja. ai %d voturi

// VOTE x
// - va adauga un vot pentru candidatul x. un client poate vota o singura data. incercarea de vot multiplu
// se va solda cu eroare. raspunsurile posibile sunt:
// - am adaugat un vot pentru clientul %d
// - ai votat deja. nu o poti face inca o data

// LIST 
// lista tuturor candidatilor, alaturi de numarul de voturi

// exemplu:
// CANDIDAT       VOTE_COUNT
// 0              2
// 3              7
// 5              4
// SFARSIT

// EXIT
// inchide clientul curent, avand ca rezultat eliberarea tuturor resurselor alocate.
// Atentie! pentru a fi notata, implementarea trebuie sa implementeze controlul fluxului si 
// retransmisia mesajelor in ambele sensuri (conform indicatiilor din laboratorul 6) si sa folsoeasca in mod corect API ul pentru socketi UDP.

// Hint! diferit fata de tcp, la udp nu aveti optiunea de a astepta mesajele doar de la o sursa anume.
// aveti grija cum identificati sursa mesajelor (inclusiv pentru ACK-uri).