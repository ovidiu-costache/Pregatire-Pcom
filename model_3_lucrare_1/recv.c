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

struct account {
    char user[50];
    char pass[50];
};

int main(int argc,char** argv) {
	/* Don't modify this */
	init(HOST,PORT);

	struct l3_msg t;
    struct account db[100];
    int total_users = 0;
    int is_logged = 0;

    while (1) {
        /* Receive the frame from the link */
        int len = link_recv(&t, sizeof(struct l3_msg));
        DIE(len < 0, "Receive message");

        /* We have to convert it to host order */
        uint32_t recv_sum = ntohl(t.hdr.sum);
        t.hdr.sum = 0;
        int new_sum = simple_csum((uint8_t *) &t, sizeof(struct l3_msg));
        
        int nack;
        if (new_sum == recv_sum)
            nack = 1; // ack = 1 adica
        else
            nack = 0; // ack = 0 si trebuie retrimis

        link_send(&nack, sizeof(int));

        if (nack == 0) {
            printf("[SERVER] Am primit gunoi. Astept retransmisia...\n");
            continue;
        }

        // daca am ajuns aici, pachetul este bun

        char *user = strtok(t.payload, "|");
        char *pass = strtok(NULL, "|");

        if (t.hdr.type == TYPE_REGISTER) {
            strcpy(db[total_users].user, user);
            strcpy(db[total_users].pass, pass);
            total_users++;
            printf("[SERVER] User %s inregistrat cu succes!\n", user);
        } else if (t.hdr.type == TYPE_LOGIN) {
            int found = 0;
            for (int i = 0; i < total_users; i++)
                if (strcmp(db[i].user, user) == 0 && strcmp(db[i].pass, pass) == 0) {
                    found = 1;
                    break;
                }

            if (found == 0)
                printf("[SERVER] Credentiale gresite pentru %s!\n", user);
            else {
                is_logged = 1;
                printf("[SERVER] %s s-a logat cu succes!\n", user);
            }
        } else if (t.hdr.type == TYPE_CONNECT) {
            if (is_logged == 1) {
                printf("[SERVER] Esti conectat la internet!\n");
            } else {
                printf("[SERVER] Eroare: Trebuie sa te loghezi intai!\n");
            }
        }
    }

	return 0;
}
