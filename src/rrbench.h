// vim: set expandtab softtabstop=4 tabstop=4 shiftwidth=4:
//
// Kornilios Kourtis <kkourt@kkourt.io>
//

#ifndef RRBENCH_H__
#define RRBENCH_H__

typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

_Static_assert(sizeof(u64) == 8, "Invalid u64 size");
_Static_assert(sizeof(u32) == 4, "Invalid u32 size");
_Static_assert(sizeof(u16) == 2, "Invalid u16 size");
_Static_assert(sizeof(u8)  == 1, "Invalid u8 size");

enum rr_type {
    RR_TYPE_HELO  = 0,
    RR_TYPE_OHHI  = 1,
    RR_TYPE_PING  = 10,
    RR_TYPE_PONG  = 11,
};

struct  rr_hdr {
    u32 magic;
    u32 rrid;
    u8  type;
    union {
        struct {
            u16 req_size;
            u16 res_size;
        } helo;
        struct {
            u16 dlen;
        } ping;
        struct {
            u16 dlen;
        } pong;
    };
    char      data[];
} __attribute__((packed));


#endif /* RRBENCH_H__ */
