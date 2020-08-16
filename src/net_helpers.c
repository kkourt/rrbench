// vim: set expandtab softtabstop=4 tabstop=4 shiftwidth=4:
//
// Kornilios Kourtis <kkourt@kkourt.io>
//

#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h> // getopt

#include "net_helpers.h"

// set the service field of the URL
// might leak data, does not check/set ->serv
void url_set_service__(struct url *url, const char *service);

/*
 * (net) Helpers
 *
 * akourtis@inf.ethz.ch, kornilios@gmail.com
 * 2014
 */

// format xxx://yyyy:zzzz
//              yyyy:zzzz
int url_parse(struct url *url, const char *url_str)
{
    const char proto_sep[] = "://", service_sep[] = ":";
    const char *input;
    char *s;

    // sanity check
    if (url_str == NULL || url_str[0] == '\0')
        return -1;
    input = url_str;

    if ((s = strstr(input, proto_sep)) != NULL) {
        size_t len = s - input;
        url->prot = malloc(len + 1);
        if (!url->prot) {
            perror("malloc");
            exit(1);
        }
        memcpy(url->prot, input, len);
        url->prot[len] = '\0';
        input = s + strlen(proto_sep);
    } else
        url->prot = NULL;

    if ((s = strstr(input, service_sep)) != NULL) {
        size_t len = s - input;
        url->node = malloc(len + 1);
        if (!url->node) {
            perror("malloc");
            exit(1);
        }
        memcpy(url->node, input, len);
        url->node[len] = '\0';
        input = s + strlen(service_sep);
    } else {
        // could not find service_sep -- assume the whole string is the node
        url->serv = NULL;
        size_t len = strlen(input);
        url->node = malloc(len + 1);
        if (!url->node) {
            perror("malloc");
            exit(1);
        }
        memcpy(url->node, input, len);
        url->node[len] = '\0';
        return 0;
    }

    url_set_service__(url, input);
    return 0;
}

int url_from_peer(struct url *url, int sockfd,
                  struct sockaddr *peer_addr, socklen_t peer_addr_size) {


    const size_t prot_size = 4;   // TCP/UDP
    url->prot = malloc(prot_size);
    if (!url->prot) {
        perror("malloc");
        exit(1);
    }

    const size_t node_size = 16;  // xxx.xxx.xxx.xxx
    url->node = malloc(node_size);
    if (!url->node) {
        perror("malloc");
        exit(1);
    }

    const size_t serv_size = 5;   // 65535
    url->serv = malloc(serv_size);
    if (!url->serv) {
        perror("malloc");
        exit(1);
    }

    int opt, err;
    socklen_t opt_l = sizeof(opt);
    err = getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &opt, &opt_l);
    if (err == -1) {
        perror("getsockopt");
        goto fail_free;
    }

    if (opt == SOCK_STREAM) {
        strcpy(url->prot, "tcp");
    } else if (opt == SOCK_DGRAM) {
        strcpy(url->prot, "udp");
    } else {
        fprintf(stderr, "%s: Unknown socket type:%d\n", __FUNCTION__, opt);
        strcpy("???", url->prot);
    }

    struct sockaddr_in peer_addr__;
    if (peer_addr == NULL) {
        peer_addr_size = sizeof(peer_addr__);
        peer_addr = (struct sockaddr *)&peer_addr__;
        err = getpeername(sockfd, peer_addr, &peer_addr_size);
        if (err == -1) {
            perror("getpeername");
            goto fail_free;
        }
    }

    err = getnameinfo(peer_addr, peer_addr_size,
                      url->node, node_size,
                      url->serv, serv_size,
                      NI_NUMERICHOST | NI_NUMERICSERV);
    if (err == -1) {
        perror("getnameinfo");
        goto fail_free;
    }

    return 0;

fail_free:
    url_free_fields(url);
    return -1;
}

void
url_set_service_if_unset(struct url *url, const char *service) {
    if (url->serv == NULL)
        url_set_service__(url, service);
}

void
url_set_service__(struct url *url, const char *service) {
    size_t len = strlen(service);
    url->serv = malloc(len + 1);
    if (!url->serv) {
        perror("malloc");
        abort();
    }
    memcpy(url->serv, service, len);
    url->serv[len] = '\0';
}

struct url *url_alloc_parse(const char *url_str)
{
    struct url *url;

    url = malloc(sizeof(*url));
    if (!url) {
        perror("malloc");
        exit(1);
    }
    if (url_parse(url, url_str) < 0) {
        free(url);
        return NULL;
    }

    return url;
}

void url_copy(const struct url *src, struct url *dst)
{
    url_free_fields(dst);
    if (src->prot) {
        dst->prot = malloc(strlen(src->prot) + 1);
        if (!dst->prot)  {
            perror("malloc");
            exit(1);
        }
        strcpy(dst->prot, src->prot);
    } else dst->prot = NULL;

    if (src->node) {
        dst->node = malloc(strlen(src->node) + 1);
        if (!dst->node)  {
            perror("malloc");
            exit(1);
        }
        strcpy(dst->node, src->node);
    } else dst->node = NULL;

    if (src->serv) {
        dst->serv = malloc(strlen(src->serv) + 1);
        if (!dst->serv)  {
            perror("malloc");
            exit(1);
        }
        strcpy(dst->serv, src->serv);
    } else dst->serv = NULL;
}

void url_free_fields(struct url *url)
{
    if (url->prot) {
        free(url->prot);
        url->prot = NULL;
    }

    if (url->node) {
        free(url->node);
        url->node = NULL;
    }

    if (url->serv) {
        free(url->serv);
        url->serv = NULL;
    }
}

void url_free(struct url *url)
{
    url_free_fields(url);
    free(url);
}

// valid URLs:
//  {udp,tcp}://147.102.3.1:1234
//  {udp,tcp}://*:1234
//  147.102.3.1:1234
//  *:1234
struct addrinfo *url_getaddrinfo(struct url *url, bool srv)
{
    struct addrinfo hints = (struct addrinfo){0};
    struct addrinfo *ret;
    char *node;
    int err;

    if (url->prot == NULL) {
        // default: use TCP
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;
    } else if (strcmp(url->prot, "udp") == 0) {
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_family = AF_UNSPEC;
    } else if (strcmp(url->prot, "tcp") == 0) {
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;
    } else {
        fprintf(stderr, "Unknown protocol: %s\n", url->prot);
        return NULL;
    }

    // handle *:nnn
    if (srv && (url->node == NULL || strcmp(url->node, "*") == 0)) {
        hints.ai_flags = AI_PASSIVE;
        node = NULL;
    } else {
        hints.ai_flags = 0;
        node = url->node;
    }

    hints.ai_protocol = 0;

    if ((err = getaddrinfo(node, url->serv, &hints, &ret)) < 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return NULL;
    }

    return ret;
}

// add a number to the port of the sockaddr
void sa_add_to_port(struct sockaddr *sa, int add)
{
    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in *sa_in = (struct sockaddr_in *)sa;
        sa_in->sin_port = htons(ntohs(sa_in->sin_port) + add);
        break;
    }

    default:
        fprintf(stderr, "Unkown sa->sa_family\n");
        abort();
    }
}

// try to bind an addrinfo, but do not iterate
// returns -1 if bind fails
int do_ai_bind(struct addrinfo *ai)
{
    int fd;

    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        perror("socket call failed\n");
        return -1;
    }

    const int o = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) == -1) {
        close(fd);
        perror("setsockopt");
        return -1;
    }

    if (bind(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

// bind an addrinfo, returns fd
// if @addr_ptr is set, it places the addrinfo list element used to bind
int ai_bind(struct addrinfo *addr, struct addrinfo **addr_ptr)
{
    int fd;
    struct addrinfo *ai;
    for (ai = addr; ai != NULL; ai = ai->ai_next) {

        fd = do_ai_bind(ai);
        if (!(fd < 0))
            break;

        perror("bind failed (continuing)");
    }

    if (ai == NULL) {
        fprintf(stderr, "could not bind\n");
        abort();
    }

    if (addr_ptr)
        *addr_ptr = ai;

    return fd;
}

// try to connect to an addrinfo, but do not iterate
// returns -1 if connect fails
int do_ai_connect(struct addrinfo *ai)
{
    int fd;

    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1)
        perror("socket failed (continuing)");
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

// connect to an addrinfo, return fd
// if @addr_ptr is set, it places the addrinfo list element used to connect
// returns -1 if error
int ai_connect(struct addrinfo *addr, struct addrinfo **addr_ptr)
{
    int fd;

    struct addrinfo *ai;
    for (ai = addr; ai != NULL; ai = ai->ai_next) {
        fd = do_ai_connect(ai);
        if (!(fd < 0))
            break;
        perror("connect failed (continuing)");
    }
    if (ai == NULL) {
        fprintf(stderr, "could not connect\n");
        return -1;
    }

    if (addr_ptr)
        *addr_ptr = ai;
    return fd;
}

int ai_bind_connect(struct addrinfo *bind_ai, struct addrinfo *conn_ai)
{
    int fd;
    struct addrinfo *ai;

    // bind
    fd = ai_bind(bind_ai, NULL);
    // connect
    for (ai = conn_ai; ai != NULL; ai = ai->ai_next) {
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        perror("connect failed (continuing)");
    }
    if (ai == NULL) {
        fprintf(stderr, "could not connect\n");
        abort();
    }

    return fd;
}

uint16_t sock_getport(int sockfd)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);

    if (getsockname(sockfd, (struct sockaddr *)&addr, &addrlen) == -1) {
        perror("getsockname");
        abort();
    }

    switch (((struct sockaddr *)&addr)->sa_family) {
    case AF_INET:
        return ntohs(((struct sockaddr_in *)&addr)->sin_port);

    default:
        fprintf(stderr, "Unknown sin_family\n");
        abort();
        return -1;
    }
}
