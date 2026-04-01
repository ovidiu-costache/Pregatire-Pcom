#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "link_emulator/lib.h"
#define TYPE_OPEN 0
#define TYPE_DEPOSIT 1
#define TYPE_BALANCE 2

uint8_t simple_csum(uint8_t *buf, size_t len);

/* Layer 3 header */
struct l3_msg_hdr {
	uint16_t len;
	uint32_t sum;
    uint16_t type;
};

/* Layer 3 frame */
struct l3_msg {
	struct l3_msg_hdr hdr;
	char payload[1400];
};
