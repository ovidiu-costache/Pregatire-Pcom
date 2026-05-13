/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP si mulplixare
 * client.c
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/poll.h>
#include <ctype.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd, int udpfd, struct sockaddr_in server_address) {
  char buf[MSG_MAXSIZE + 1];
  memset(buf, 0, MSG_MAXSIZE + 1);

  struct chat_packet sent_packet;
  struct chat_packet recv_packet;

  /*
    TODO 2.2: Multiplexati intre citirea de la tastatura si primirea unui
    mesaj, ca sa nu mai fie impusa ordinea.

    Hint: server::run_multi_chat_server
  */
  struct pollfd poll_fds[3];

  poll_fds[0].fd = sockfd;
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = udpfd;
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = STDIN_FILENO;
  poll_fds[2].events = POLLIN;

  int my_id;

  while (1) {
    int rc = poll(poll_fds, 3, -1);
    DIE(rc < 0, "poll");

    // Am primit ceva de la Server prin TCP
    if (poll_fds[0].revents & POLLIN) {
      rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
      if (rc <= 0) {
        printf("[CLIENT] Se inchide...\n");
        break;
      }

      printf("%s\n", recv_packet.message);
      // Aici am primit ID-ul pe care il stochez
      // char my_id_str[10];
      // sprintf(my_id_str, "%d", my_id);
      sscanf(recv_packet.message, "Bine ai venit! ID-ul tau este %d.\n", &my_id);
    }

    // Am primit ceva de la Server prin UDP
    if (poll_fds[1].revents & POLLIN) {
      rc = recvfrom(udpfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);

      printf("[CLIENT] %s\n", recv_packet.message);
    }

    // Am primit ceva de la Tastatura
    if (poll_fds[2].revents & POLLIN) {
      char buf[256];
      if (fgets(buf, sizeof(buf), stdin) && !isspace(buf[0])) {
        // verific ce comanda e, daca este SOLD_CURENT de exemplu, trimit prin udp, altfel prin tcp
        buf[strcspn(buf, "\n")] = 0;
        if (strncmp(buf, "SOLD_CURENT", 11) == 0) {
          // UDP
          sent_packet.len = strlen(buf) + 1;
          strcpy(sent_packet.message, buf);
          strcat(sent_packet.message, "|");

          char my_id_str[10];
          sprintf(my_id_str, "%d", my_id);
          printf("CLIENT: %s\n", my_id_str);
          strcat(sent_packet.message, my_id_str);

          rc = sendto(udpfd, &sent_packet, sizeof(sent_packet), 0, (struct sockaddr *)&server_address, sizeof(server_address));
          DIE(rc < 0, "send");
        } else {
          // TCP
          sent_packet.len = strlen(buf) + 1;
          strcpy(sent_packet.message, buf);

          // Trimitem pachetul la server.
          send_all(sockfd, &sent_packet, sizeof(sent_packet));
        }
      }
    }
  }

  // while (fgets(buf, sizeof(buf), stdin) && !isspace(buf[0])) {
  //   sent_packet.len = strlen(buf) + 1;
  //   strcpy(sent_packet.message, buf);

  //   // Trimitem pachetul la server.
  //   send_all(sockfd, &sent_packet, sizeof(sent_packet));

  //   // Primim un mesaj de la server si il afisam.
  //   int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
  //   if (rc <= 0) {
  //     break;
  //   }

  //   printf("%s\n", recv_packet.message);
  // }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("\n Usage: %s <ip> <port>\n", argv[0]);
    return 1;
  }

  // Parsam port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[2], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtinem un socket TCP pentru conectarea la server
  const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  const int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(udpfd < 0, "socket");

  // Completăm in serv_addr adresa serverului, familia de adrese si portul
  // pentru conectare
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  // Ne conectăm la server
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  run_client(sockfd, udpfd, serv_addr);

  // Inchidem conexiunea si socketul creat
  close(sockfd);

  return 0;
}
