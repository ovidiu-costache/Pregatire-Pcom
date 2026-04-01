#include "common.h"

uint8_t simple_csum(uint8_t *buf, size_t len) {
    uint32_t sum = 0;
    uint8_t checksum;

    while (len > 0) {
        sum += *((uint8_t *) buf);
        buf += 1;
        len -= 1;
    }

    checksum = sum % 256;
    return checksum;
}
