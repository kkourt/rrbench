#!/usr/bin/env python3

import ctypes
from pprint import pprint

from bcc import BPF
from bcc.utils import printb



prog = """

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/udp.h>
#include <uapi/linux/tcp.h>

#define RR_MAGIC 0xfae1fae2

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


enum ev_type {
    EV_TYPE_NETIF_RECEIVE_SKB  = 10,
};

struct net_dev_event {
    char dev[16];
};

struct event {
    uint32_t rrid;
    uint8_t ev_ty;
    union {
        struct net_dev_event net_dev;
    } ev_data;
} __attribute__((packed));

/*
BPF_PERF_OUTPUT(events);

struct netif_receive_skb_args  {
    // from /sys/kernel/tracing/events/net/netif_receive_skb/format
    u64 __unused__;
    u64 skbaddr;
    u32 len;
    u64 name__;
};

int
trace_netif_receive_skb(struct netif_receive_skb_args *args)
{
    //struct event ev = {0};
    //struct net_device *netdev;
    //bpf_probe_read(&netdev, sizeof(netdev), &skb->dev);
    //bpf_probe_read_str(&ev.ev_data.net_dev.dev, sizeof(ev.ev_data.net_dev.dev), &netdev->name);
    //ev.ev_ty = EV_TYPE_NETIF_RECEIVE_SKB;
    //events.perf_submit(ctx, &ev, sizeof(ev));
    char devname[16];
    unsigned short name_off = args->name__ & 0xFFFF;
    // unsigned short name_len = args->name__ >> 16;
    bpf_probe_read_str(devname, 16, (char *)args + name_off);

    bpf_trace_printk("len=%u name_len=%lu\\n", args->len, args->name__);
    return 0;
}
*/

static inline struct udphdr *skb_to_udphdr(const struct sk_buff *skb)
{
    // unstable API. verify logic in udp_hdr() -> skb_transport_header().
    return (struct udphdr *)(skb->head + skb->transport_header);
}
static inline struct tcphdr *skb_to_tcphdr(const struct sk_buff *skb)
{
    // unstable API. verify logic in tcp_hdr() -> skb_transport_header().
    return (struct tcphdr *)(skb->head + skb->transport_header);
}
static inline struct iphdr *skb_to_iphdr(const struct sk_buff *skb)
{
    // unstable API. verify logic in ip_hdr() -> skb_network_header().
    return (struct iphdr *)(skb->head + skb->network_header);
}


TRACEPOINT_PROBE(net, netif_receive_skb) {
    char devname[16];
    TP_DATA_LOC_READ_CONST(devname, name, 16);
    struct sk_buff *skb = args->skbaddr;
    bpf_trace_printk("skb->len=%lu skb->data_len=%lu diff=%lu\\n", skb->len, skb->data_len, skb->len - skb->data_len);

    u8 iphdr_byte0;
    struct iphdr *ip = skb_to_iphdr(skb);
    bpf_probe_read(&iphdr_byte0, 1, ip);
    if ((iphdr_byte0>>4) == 4) {
        u8 v4_prot;
        bpf_probe_read(&v4_prot, 1, &ip->protocol);
        if (v4_prot == 0x06) { // TCP
            struct tcphdr *tcp_ = skb_to_tcphdr(skb);
            struct tcphdr tcp;
            bpf_probe_read(&tcp, sizeof(tcp), tcp_);
            struct rr_hdr rr_hdr;
            bpf_probe_read(&rr_hdr, sizeof(rr_hdr), (char *)tcp_ + (tcp.doff<<2));
            bpf_trace_printk("len=%u name=%s magic=0x%lx\\n", args->len, devname, rr_hdr.magic);
        }
    }

    return 0;
}
"""

class NetDevEv(ctypes.Structure):
    _fields_ = [
        ('dev', ctypes.c_char*16)
    ]


class Ev(ctypes.Union):
    _pack_ = 1
    _fields_ = [
        ('net_dev', NetDevEv),
    ]

class Event(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
            ('rrid', ctypes.c_uint32),
            ('ev_ty', ctypes.c_ubyte),
            ('ev_data', Ev),
    ]


def handle_kprobe_event(cpu, data, size):
    event = ctypes.cast(data, ctypes.POINTER(Event)).contents
    print(event.ev_ty, event.ev_data.net_dev.dev)


import os

if __name__ == '__main__':
    path = os.path.dirname(os.path.abspath( __file__ ))
    b = BPF(text=prog, hdr_file="%s/rrbench.h")
    # NB: use a tracepoint instead of a kprobe, because netif_receive_skb_list
    # is typically used.
    #b.attach_tracepoint(tp="net:netif_receive_skb", fn_name="trace_netif_receive_skb")
    #b["events"].open_perf_buffer(handle_kprobe_event)
    #while 1:
    #    b.perf_buffer_poll()
    # format output
    while 1:
        try:
            (task, pid, cpu, flags, ts, msg) = b.trace_fields()
        except ValueError:
            continue
        except KeyboardInterrupt:
            exit()
        printb(b"%-18.9f %-16s %-6d %s" % (ts, task, pid, msg))
