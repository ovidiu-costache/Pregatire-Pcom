#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "common.h"
#include "link_emulator/lib.h"
#include "include/utils.h"

/**
 * You can change these to communicate with another colleague.
 * There are several factors that could stop this from working over the
 * internet, but if you're on the same network it should work.
 * Just fill in their IP here and make sure that you use the same port.
 */
#define HOST "127.0.0.1"
#define PORT 10001


int main(int argc,char** argv) {
	/* Don't modify this */
	init(HOST,PORT);

    int fd = -1;
    uint8_t seq = 0;
    struct l3_msg t;

	while (1) {
        memset(&t, 0, sizeof(t));

        link_recv(&t, sizeof(struct l3_msg));

        int old_sum = t.hdr.sum;
        t.hdr.sum = 0;
        t.hdr.sum = ntohl(crc32((uint8_t *)&t, sizeof(t)));

        if (old_sum != t.hdr.sum) {
            printf("[SERVER] Pachet corupt detectat (seq=%d)! Trimit NACK.\n", t.hdr.seq);
            int nack = 0;
            link_send(&nack, sizeof(int));
            continue;
        }

        printf("[SERVER] Pachet OK (seq=%d, tip=%d). Trimit ACK.\n", t.hdr.seq, t.hdr.type);
        int ack = 1; // e bun
        link_send(&ack, sizeof(int));

        if (t.hdr.seq != seq) {
            printf("[SERVER] Duplicat detectat (seq=%d). Ignor datele.\n", t.hdr.seq);
            continue;
        }

        seq++;

        if (t.hdr.type == TYPE_META) {
            // numele si timpul sunt in t.payload
            char *p = strtok(t.payload, "|");
            char time[64];

            while (p != NULL) {
                strcpy(time, p);
                p = strtok(NULL, "|");
            }

            // in t.payload ramane primul cuvant, adica numele fisierului
            char file_name[256];
            strcpy(file_name, t.payload);
            strcat(file_name, ".");
            strcat(file_name, time);
            strcat(file_name, ".recv");
            fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else if (t.hdr.type == TYPE_DATA) {
            // trebuie sa scriu in fisier ce e in t.payload
            write(fd, t.payload, t.hdr.len);
        } else if (t.hdr.type == TYPE_EOF) {
            close(fd);
            break;
        }
    }

	return 0;
}
