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
#include <time.h>

#define HOST "127.0.0.1"
#define PORT 10000


int main(int argc,char** argv) {
	init(HOST,PORT);

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Numele fisierului este invalid!\n");
        return -1;
    }

    // timestamp
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local);

    /* Look in common.h for the definition of l3_msg */
	struct l3_msg t;

    memset(&t, 0, sizeof(t));

    t.hdr.type = TYPE_META;
    t.hdr.seq = 0;

    sprintf(t.payload, "%s|%s", argv[1], buf);

    t.hdr.len = strlen(t.payload) + 1;

    int ack;
    do {
        t.hdr.sum = 0;
        t.hdr.sum = htonl(crc32((uint8_t *)&t, sizeof(t)));

        link_send(&t, sizeof(struct l3_msg));

        link_recv(&ack, sizeof(int));
    } while (ack == 0); // ack = 1 daca s a trimis si se iese din bucla

    int secventa = 1;
    int bytes_cititi;

    while ((bytes_cititi = read(fd, t.payload, sizeof(t.payload))) > 0) {
        t.hdr.type = TYPE_DATA;

        t.hdr.sum = 0;
        t.hdr.seq = secventa;
        t.hdr.len = bytes_cititi;

        int ack;
        int incercari = 0;
        do {
            t.hdr.sum = 0;
            t.hdr.sum = htonl(crc32((uint8_t *)&t, sizeof(t)));

            if (incercari == 0) {
                printf("[CLIENT] Trimit pachet seq=%d (%d bytes)\n", t.hdr.seq, bytes_cititi);
            } else {
                printf("[CLIENT] RE-TRIMIT pachet seq=%d (incercarea %d)\n", t.hdr.seq, incercari);
            }

            link_send(&t, sizeof(t));

            link_recv(&ack, sizeof(int));

            incercari++;
        } while (ack == 0);

        secventa++;
    }

    do {
        t.hdr.type = TYPE_EOF;
        t.hdr.seq = secventa;
        t.hdr.len = 0;
        t.hdr.sum = 0;

        t.hdr.sum = htonl(crc32((uint8_t *)&t, sizeof(t)));

        link_send(&t, sizeof(t));

        link_recv(&ack, sizeof(int));
    } while (ack == 0);

    close(fd);

	return 0;
}
