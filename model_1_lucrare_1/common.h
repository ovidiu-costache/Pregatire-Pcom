#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "link_emulator/lib.h"

#define TYPE_META 0
#define TYPE_DATA 1
#define TYPE_EOF 2

uint8_t simple_csum(uint8_t *buf, size_t len);

uint32_t crc32(uint8_t *buf, size_t len);

/* Layer 3 header */
struct l3_msg_hdr {
	uint16_t len;
	uint32_t sum;
    uint8_t seq;
    uint8_t type;
}__attribute__((packed));

/* Layer 3 frame */
struct l3_msg {
	struct l3_msg_hdr hdr;
	/* Data */

	char payload[1400];
}__attribute__((packed));
