// vim: set expandtab softtabstop=4 tabstop=4 shiftwidth=4:
//
// Kornilios Kourtis <kkourt@kkourt.io>
//

#ifndef RRBENCH_H__
#define RRBENCH_H__

#include <inttypes.h>

enum rr_type {
    RR_TYPE_HELO  = 0,
    RR_TYPE_OHHI  = 1,
    RR_TYPE_PING  = 10,
    RR_TYPE_PONG  = 11,
};

struct  rr_hdr {
    uint32_t magic;
    uint32_t rrid;
    uint8_t  type;
    union {
        struct {
            uint16_t req_size;
            uint16_t res_size;
        } helo;
        struct {
            uint16_t dlen;
        } ping;
        struct {
            uint16_t dlen;
        } pong;
    };
    char      data[];
} __attribute__((packed));


#endif /* RRBENCH_H__ */
