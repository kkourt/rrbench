
// #include <linux/skbuff.h>
// #include <linux/netdevice.h>

#include <bpf/ctx/skb.h>
#include <bpf/api.h>

#include "rrbench.h"

#ifndef TC_ACT_PIPE
#define TC_ACT_PIPE 3
#endif

# define printk(fmt, ...)				\
	({						\
		const char ____fmt[] = fmt;		\
		trace_printk(____fmt, sizeof(____fmt),	\
			     ##__VA_ARGS__);		\
	})

__section("rr-prepare")
int rr_pull(struct __sk_buff *skb) {
	(void)skb;

	printk("Hello!\n");
	ctx_pull_data(skb, sizeof(struct rr_hdr));
	//return TC_ACT_SHOT;
	return TC_ACT_PIPE;
}

BPF_LICENSE("GPL");
