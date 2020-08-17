// vim: set expandtab softtabstop=4 tabstop=4 shiftwidth=4:
//
// Kornilios Kourtis <kkourt@kkourt.io>
//

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "rrbench.h"
#include "net_helpers.h"
#include "tsc.h"
#include "misc.h"

#define RR_MAGIC 0xfae1fae2
#define RR_MAX_SIZE 1024

static void
rr_init_helo(struct rr_hdr *hdr, uint16_t req_size, uint16_t res_size) {
    hdr->magic = RR_MAGIC;
    hdr->rrid = 0;
    hdr->type = RR_TYPE_HELO;
    hdr->helo.req_size = req_size;
    hdr->helo.res_size = res_size;
}

static void
rr_init_ping(struct rr_hdr *hdr, uint32_t rrid, uint16_t dlen) {
    hdr->magic = RR_MAGIC;
    hdr->rrid = rrid;
    hdr->type = RR_TYPE_PING;
    hdr->ping.dlen = dlen;
}

static void
rr_init_pong(struct rr_hdr *hdr, uint32_t rrid, uint16_t dlen) {
    hdr->magic = RR_MAGIC;
    hdr->rrid = rrid;
    hdr->type = RR_TYPE_PONG;
    hdr->pong.dlen = dlen;
}

/**
 * Server
 */

static void
srv_serve(struct url *cli_url, int fd) {

    struct rr_hdr rr_msg;
    int nreceived, nsent;
    struct rr_hdr *req, *res;
    unsigned req_buff_size, res_buff_size;
    size_t count;

    printf("connection from: %s//%s:%s\n", cli_url->prot, cli_url->node, cli_url->serv);

	nreceived = recv(fd, &rr_msg, sizeof(rr_msg), 0);
	if (nreceived != sizeof(rr_msg))
	    die_perr("recv");

    if (rr_msg.magic != RR_MAGIC || rr_msg.type != RR_TYPE_HELO)
        die("invalid protocol");

    unsigned req_size = rr_msg.helo.req_size;
    unsigned res_size = rr_msg.helo.res_size;
    printf("%s//%s:%s: req_size:%u res_size:%u\n", cli_url->prot, cli_url->node, cli_url->serv, req_size, res_size);

    rr_msg.type = RR_TYPE_OHHI;
    nsent = send(fd, &rr_msg, sizeof(rr_msg), 0);
	if (nsent != sizeof(rr_msg))
	    die_perr("sent");

    req_buff_size = req_size + sizeof(struct rr_hdr);
	req = xmalloc(req_buff_size);

    res_buff_size = res_size + sizeof(struct rr_hdr);
	res = xmalloc(res_buff_size);
	rr_init_pong(res, 0, res_size);


    for (count = 0;;) {
        nreceived = recv(fd, req, req_buff_size, 0);
        if (nreceived == -1)
            die_perr("recv");
        else if (nreceived == 0)
            break;
        else if (nreceived != (int)req_buff_size)
            die("received less than expected");
        else if (req->magic != RR_MAGIC || req->type != RR_TYPE_PING)
            die("invalid protocol");

        res->rrid = req->rrid;

        nsent = send(fd, res, res_buff_size, 0);
        if (nsent != (int)res_buff_size)
            die_perr("send");
        count++;
    }

    printf("done with: %s//%s:%s (served %zd messages)\n", cli_url->prot, cli_url->node, cli_url->serv, count);
    close(fd);
}

int
main_srv(const char *pname, int argc, char *argv[]) {

    struct addrinfo *ai_list, *ai_b;
    struct url srv_url;
    int lfd;

    if (argc < 2) {
        printf("Usage: %s srv <server address>\nn", pname);
        exit(1);
    }

    if (url_parse(&srv_url, argv[1]) < 0)
        die("cannot parse URL:%s\n", argv[1]);

    ai_list = url_getaddrinfo(&srv_url, true);
    lfd = ai_bind(ai_list, &ai_b);

    int o = 1;
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) == -1)
        die_perr("setsockopt");

    if (listen(lfd, 5) == -1)
        die_perr("listen");

    for (;;) {
        struct url cli_url;
        struct sockaddr_storage cli_addr;
        socklen_t cli_addr_size = sizeof(cli_addr);
        int afd = accept(lfd, (struct sockaddr *)&cli_addr, &cli_addr_size);
        if (afd == -1)
            die_perr("accept");

        if (url_from_peer(&cli_url, afd, (struct sockaddr *)&cli_addr, cli_addr_size) < 0)
            die("url_from_peer failed");
        srv_serve(&cli_url, afd);
    }

    return 0;
}

/**
 * Client
 */

struct cli_conf {
    unsigned burst;
    unsigned nmessages;
    unsigned req_size, res_size;
    struct url srv_url;
};

static void
cli_helo(struct cli_conf *conf, int fd) {

    struct rr_hdr rr_helo, rr_ohhi;
    unsigned helo_errs=0; // NB: should be >0 for UDP

    rr_init_helo(&rr_helo, conf->req_size, conf->res_size);

	for (unsigned i=0; ;) {
        int nsent = send(fd, &rr_helo, sizeof(rr_helo), 0);
		if (nsent == sizeof(rr_helo)) {
            break;
        }
        perror("send");
	    if (++i == helo_errs)
	        die("bailing out after %d helo attempts\n", helo_errs);
	}

	int nreceived = recv(fd, &rr_ohhi, sizeof(rr_ohhi), 0);
	if (nreceived != sizeof(rr_ohhi))
	    die_perr("recv");

    if (rr_ohhi.magic != RR_MAGIC || rr_ohhi.type != RR_TYPE_OHHI)
        die("invalid protocol");
}


static int
comp_ticks(const void *t1p, const void *t2p) {
    uint64_t t1 = *((const uint64_t *)t1p);
    uint64_t t2 = *((const uint64_t *)t2p);

    if (t1 < t2)
        return -1;
    else if (t1 == t2)
        return 0;
    else
        return 1;
}

static void
report_ticks(uint64_t *ticks, size_t nticks) {
    qsort(ticks, nticks, sizeof(uint64_t), comp_ticks);

    uint64_t med;
    size_t mid = nticks / 2;
    if (nticks % 2 == 0) {
        med = (ticks[mid] + ticks[mid-1]) / 2;
    } else {
        med = ticks[mid];
    }

    uint64_t sum = 0;
    for (size_t i=0; i < nticks; i++) {
        sum += ticks[i];
    }
    uint64_t avg = sum / nticks;
    uint64_t max = ticks[nticks - 1];
    uint64_t min = ticks[0];

    printf("TICKS: avg:%lu (%lf usecs) med:%lu (%lf usecs) min:%lu (%lf usecs) max:%lu (%lf usecs)\n",
            avg, __tsc_getusecs(avg),
            med, __tsc_getusecs(med),
            min, __tsc_getusecs(min),
            max, __tsc_getusecs(max));
}

static void
cli_ping_pong(struct cli_conf *conf, int fd) {

	size_t errors, sent, received;
	const size_t nmessages = conf->nmessages;
	size_t req_buff_size, res_buff_size;
	const int burst = conf->burst;
	struct rr_hdr *req, *res;
	int in_flight;
	uint32_t idx = 0;
	uint32_t sum1, sum2;
	uint64_t *ticks;

	req_buff_size = sizeof(struct rr_hdr) + conf->req_size;
	req = xmalloc(req_buff_size);
	rr_init_ping(req, idx, conf->req_size);

	res_buff_size = sizeof(struct rr_hdr) + conf->res_size;
	res = xmalloc(res_buff_size);

    ticks = xcalloc(nmessages, sizeof(uint64_t));

	sum1 = sum2 = 0;
	in_flight = errors = received = sent = 0;
	while (received < nmessages) {
        // try to send as many as possible without blocking or overcomming the
        // in-flight limit
		while ((in_flight < burst) && (nmessages >= sent)) {
			req->rrid = idx;
			//printf("SENDING %u\n", req->rrid);
			int ret = send(fd, req, req_buff_size, MSG_DONTWAIT);
			if (ret == -1) {
				errors++;
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					break;
			    die_perr("send");
			} else if (ret != (int)req_buff_size) {
				die("Unexpected ret: %d (expecting:%zd)\n", ret, req_buff_size);
			}

			sum1 += idx;
			in_flight++;
			ticks[idx] = get_ticks();
			idx++;
		}
		// try to receive as many as possible
		bool recv_one = false;
		while (in_flight > 0) {

			// do not block if there are more messages we can send
			int noblock = ( (nmessages > sent) && recv_one) ? MSG_DONTWAIT : 0;

			int ret = recv(fd, res, res_buff_size, noblock);
			if (ret == -1) {
				errors++;
				if (noblock && (errno == EAGAIN || errno == EWOULDBLOCK))
					break;
				die_perr("recv");
			}

            if (res->magic != RR_MAGIC || res->type != RR_TYPE_PONG || res->pong.dlen != conf->res_size)
                die("invalid protocol");

			recv_one = true;
			received++;
			sum2 += res->rrid;
			ticks[req->rrid] = get_ticks() - ticks[req->rrid];
			in_flight--;
		}
	}

	if (sum1 != sum2)
		die("checksum failed: %ul =/= %ul\n", sum1, sum2);

	report_ticks(ticks, nmessages);
	free(ticks);

	free(req);
	free(res);
	return;
}

static void
cli_run(struct cli_conf *conf, int fd) {
    cli_helo(conf, fd);
    cli_ping_pong(conf, fd);
}

static int
main_cli(const char *pname, int argc, char *argv[]) {

	struct addrinfo *connect_ai;
	struct cli_conf cli_conf;
    unsigned connect_errs=10;
	extern char *optarg;
    int fd;
    char c;

    cli_conf.burst = 1;
    cli_conf.nmessages = 1024;
    cli_conf.req_size = 0;
    cli_conf.res_size = 0;

    if (argc < 2) {
        printf("Usage: %s cli <server address> [-b burst] [-n nmessages] [-q req_size] [-s res_size]\n", pname);
        printf("\tburst: packets in-flight (default: %u)\n", cli_conf.burst);
        printf("\tnmessages: total number of messages to send (default: %u)\n", cli_conf.nmessages);
        printf("\treq_size: request payload size (default: %u)\n", cli_conf.req_size);
        printf("\tres_size: response payload size (default: %u)\n", cli_conf.req_size);
        exit(1);
    }

	if (url_parse(&cli_conf.srv_url, argv[1]) < 0)
		die("cannot parse URL:%s\n", argv[1]);

	while ( (c = getopt(argc-1, &argv[1], "n:b:q:s:")) != -1) {
		switch (c) {

			case 'b':
			if ((cli_conf.burst = atol(optarg)) < 1)
				die("burst specified is < 1\n");
			break;

			case 'n':
			if ((cli_conf.nmessages = atol(optarg)) < 1)
				die("nmessages specified is < 1\n");
			break;

            case 'q':
            cli_conf.req_size = atol(optarg);
            break;

            case 's':
            cli_conf.res_size = atol(optarg);
            break;

            default:
            die("Unexpected option: %c\n", c);
		}
	}

	connect_ai = url_getaddrinfo(&cli_conf.srv_url, false);
	for (unsigned i=0; ;) {
	    fd = ai_connect(connect_ai, NULL);
	    if (fd != -1)
	        break;
        perror("connect");
	    if (++i == connect_errs)
	        die("bailing out after %d connection attempts\n", connect_errs);
	}
	cli_run(&cli_conf, fd);

    return 0;
}

int
main(int argc, char *argv[])
{
    const char *pname = argv[0];

    if (argc > 1) {
        if (strcmp("srv", argv[1]) == 0)
            return main_srv(pname, argc - 1, argv + 1);
        if (strcmp("cli", argv[1]) == 0)
            return main_cli(pname, argc - 1, argv + 1);
    }

    printf("Usage: %s (srv|cli) [srv or cli options]\n", pname);
    return 1;
}
