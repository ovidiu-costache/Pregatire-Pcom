#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"
#include "link_emulator/lib.h"
#include <arpa/inet.h>
#include "include/utils.h"
#include <string.h>
#include <stdlib.h>

#define HOST "127.0.0.1"
#define PORT 10000


int main(int argc,char** argv) {
	init(HOST,PORT);

    char line[256];

    // citirea de la stdin
    while (fgets(line, sizeof(line), stdin)) {
        struct l3_msg t;
        memset(&t, 0, sizeof(struct l3_msg));

        line[strcspn(line, "\n")] = 0;

        if (line[0] == '\0') {
            continue;
        }

        char user[50];
        int amount;

        if (sscanf(line, "OPEN %s", user) == 1) {
            snprintf(t.payload, sizeof(t.payload), "%s", user);
            t.hdr.type = TYPE_OPEN;
        } else if (sscanf(line, "DEPOSIT %s %d", user, &amount) == 2) {
            snprintf(t.payload, sizeof(t.payload), "%s %d", user, amount);
            t.hdr.type = TYPE_DEPOSIT;
        } else if (sscanf(line, "BALANCE %s", user) == 1) {
            snprintf(t.payload, sizeof(t.payload), "%s", user);
            t.hdr.type = TYPE_BALANCE;
        } else {
            printf("Comanda gresita!\n");
            continue;
        }

        t.hdr.len = strlen(t.payload) + 1;

        t.hdr.sum = 0;
        t.hdr.sum = htonl(simple_csum((uint8_t *)&t, sizeof(struct l3_msg)));

        int ack = 0;
        do {
            link_send(&t, sizeof(struct l3_msg));

            link_recv(&ack, sizeof(int));

            if (ack == 0) {
                printf("[SEND] Got NACK (frame was corrupted at receiver), retransmiting...\n");
            } else {
                printf("[SEND] Got ACK (frame was sent)\n");
            }
        } while (ack == 0);
    }

	return 0;
}
