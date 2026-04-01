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
    int sold;
};

int main(int argc,char** argv) {
	/* Don't modify this */
	init(HOST,PORT);

	struct account account[256];
	int nr_conturi = 0;

	while (1) {
		struct l3_msg t;
		memset(&t, 0, sizeof(struct l3_msg));

		int len = link_recv(&t, sizeof(struct l3_msg));
		DIE(len < 0, "Receive message");

		uint32_t recv_sum = ntohl(t.hdr.sum);
		t.hdr.sum = 0;
		int sum_ok = (simple_csum((void *) &t, sizeof(struct l3_msg)) == recv_sum);

		printf("[RECV] len=%d; sum(%s)=0x%04hx; payload=\"%s\";\n", t.hdr.len, sum_ok ? "GOOD" : "BAD", recv_sum, t.payload);

		int de_trimis;
		if (sum_ok) {
			de_trimis = 1;
			link_send(&de_trimis, sizeof(int));
		} else {
			de_trimis = 0;
			link_send(&de_trimis, sizeof(int));
			continue;
		}

		// daca am ajuns aici, pachetul e bun si trebuie sa fac cele 3 comenzi
		char user[50];
		int amount;

		if (t.hdr.type == TYPE_OPEN) {
			sscanf(t.payload, "%s", account[nr_conturi].user);
			account[nr_conturi].sold = 0;
			nr_conturi++;
			printf("[RECV] OPEN ok: '%s' opened (%d accounts total)\n", account[nr_conturi - 1].user, nr_conturi);
		} else if (t.hdr.type == TYPE_DEPOSIT) {
			sscanf(t.payload, "%s %d", user, &amount);

			// prima data caut userul
			int found = 0;
			int index;
			for (int i = 0; i < nr_conturi; i++) {
				if (strcmp(account[i].user, user) == 0) {
					found = 1;
					account[i].sold += amount;
					index = i;
					break;
				}
			}

			if (found == 0) {
				printf("[RECV] DEPOSIT failed: account '%s' not found\n", user);
				continue;
			} else {
				// daca am ajuns aici, inseamna ca am gasit contul si i am depus banii
				printf("[RECV] Deposited %d, Current balance: %d\n", amount, account[index].sold);
			}

		} else if (t.hdr.type == TYPE_BALANCE) {
			sscanf(t.payload, "%s", user);

			// prima data caut userl
			int found = 0;
			int index;
			for (int i = 0; i < nr_conturi; i++) {
				if (strcmp(account[i].user, user) == 0) {
					found = 1;
					index = i;
					break;
				}
			}

			if (found == 0) {
				printf("[RECV] BALANCE failed: account '%s' not found\n", user);
				continue;
			} else {
				// daca am ajuns aici, inseamna ca am gasit contul si ii afisez soldul
				printf("[RECV] Balance for '%s': %d\n", user, account[index].sold);
			}
		}
	}

	return 0;
}
