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

    char comanda[50];
    char user[50];
    char pass[50];

	while (scanf("%s", comanda) != EOF) {
        struct l3_msg t;
        memset(&t, 0, sizeof(t));

        if (strcmp(comanda, "REGISTER") == 0) {
            t.hdr.type = TYPE_REGISTER;

            // va mai primi username si password
            scanf("%s %s", user, pass);

            // trebuie trecute in payload
            char payload[256] = "";
            strcat(payload, user);
            strcat(payload, "|");
            strcat(payload, pass);

            strcpy(t.payload, payload);
            t.hdr.len = strlen(payload) + 1;

        } else if (strcmp(comanda, "LOGIN") == 0) {
            t.hdr.type = TYPE_LOGIN;

            scanf("%s %s", user, pass);

            char payload[256] = "";

            strcat(payload, user);
            strcat(payload, "|");
            strcat(payload, pass);

            strcpy(t.payload, payload);
            t.hdr.len = strlen(payload) + 1;
        } else if (strcmp(comanda, "CONNECT") == 0) {
            t.hdr.type = TYPE_CONNECT;
        }

        t.hdr.sum = 0;
        t.hdr.sum = htonl(simple_csum((uint8_t *)&t, sizeof(t)));

        int ack = 0;
        do {
            link_send(&t, sizeof(t));

            link_recv(&ack, sizeof(int));

            if (ack == 0) {
                printf("[SENDER] NACK primit. Retransmit...\n");
            } else {
                printf("[SENDER] ACK primit! Gata.\n");
            }
        } while (ack == 0);

    }

	return 0;
}
