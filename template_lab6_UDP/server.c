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

#include "common.h"
#include "utils.h"

#define SAVED_FILENAME "received_file.bin"

int recv_seq_udp(int sockfd, struct seq_udp *seq_packet, int expected_seq) {
  struct sockaddr_in client_addr;
  socklen_t clen = sizeof(client_addr);

  // Receive a segment with seq_number seq_packet->seq
  int rc = recvfrom(sockfd, seq_packet, sizeof(struct seq_udp), 0,
                    (struct sockaddr *)&client_addr, &clen);

  // TODO: Check if the sequence number is the expected one.

  // TODO: If we got the expected packet (by seq) send ACK for the seq.packet
  // and return the number of bytes read. 
  // We will increase expected_seq in the calling function (recv_a_file(...))

  // TODO: If segment is not with the expected number, send ACK
  // for the last well received packet (expected_seq - 1) and return -1
}

void recv_a_file(int sockfd, char *filename) {
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  DIE(fd < 0, "open");
  struct seq_udp p;
  int expected_seq = 0;
  int rc;

  while (1) {
    // Receive a chunk
    rc = recv_seq_udp(sockfd, &p, expected_seq);

    // TODO: If rc == -1 => we didn't receive the expected segment. We continue (retry to receive the same chunk).

    // TODO: If rc >=0 => we receive the expected segment. We increase expected_seq

    // An empty payload means the file ended.
    if (p.len == 0)
      // Break if file ended
      break;

    // Write the chunk to the file
    write(fd, p.payload, p.len);
  }

  close(fd);
}

void recv_a_message(int sockfd) {
  // Receive a datagram and send an ACK
  // The info of the who sent the datagram (PORT and IP)
  struct sockaddr_in client_addr;
  struct seq_udp p;
  socklen_t clen = sizeof(client_addr);
  int rc = recvfrom(sockfd, &p, sizeof(struct seq_udp), 0,
                    (struct sockaddr *)&client_addr, &clen);

  // We know it's a string so we print it
  printf("[Server] Received: %s\n", p.payload);

  int ack = 0;
  // Sending ACK. We model ACK as datagrams with only an int of value 0.
  rc = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr,
              clen);
  DIE(rc < 0, "send");
}

int main(void) {
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
  servaddr.sin_port = htons(PORT);

  // Bind the socket with the server address. The OS networking
  // implementation will redirect to us the contents of all UDP
  // datagrams that have our port as destination
  int rc = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
  DIE(rc < 0, "bind failed");

  // TODO 1.0: Study the code. Uncoment this to receive a file chuck by chuck
  // and save it locally
  recv_a_message(sockfd);
  // recv_a_file(sockfd, SAVED_FILENAME);

  return 0;
}
