// vim: set expandtab softtabstop=4 tabstop=4 shiftwidth=4:
//
// Kornilios Kourtis <kkourt@kkourt.io>
//
#ifndef NET_HELPERS_H__
#define NET_HELPERS_H__

#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#if defined(__cplusplus)
extern "C" {
#endif

// URL
// members are NULL-terminated strings or NULL;
// prot://node:service
//
struct url {
    char *prot;
    char *node;
    char *serv;
};

// returns 0 or error
//
// valid URLs:
//  {udp,tcp}://147.102.3.1:1234
//  {udp,tcp}://*:1234
//  147.102.3.1:1234
//  *:1234
int url_parse(struct url *url, const char *url_str);
// returns url or null
struct url *url_alloc_parse(const char *url_str);

// generate a URL from a sockset
//
// If peer_addr is NULL, the function will call getpeername()
// Otherwise, it will use peer_addr, and peer_addr_size to call getnameinfo()
//
int url_from_peer(struct url *url, int sockfd,
                  struct sockaddr *peer_addr, socklen_t peer_addr_size);

// set URL service if it is unset
void url_set_service_if_unset(struct url *url, const char *service);

// free url, and its fields
void url_free(struct url *url);
// just free the fields (e.g., for struct url's allocated in the stack)
void url_free_fields(struct url *url);

void url_copy(const struct url *url_src, struct url *url_dst);

// Uses getaddrinfo().
// Returns NULL if getaddrinfo() failed
// Return addrinfo can be freed with freeaddrinfo().
struct addrinfo *url_getaddrinfo(struct url *url, bool srv);

// bind an addrinfo, returns fd
// if @addr_ptr is set, it places the addrinfo list element used to bind
int ai_bind(struct addrinfo *addr, struct addrinfo **addr_ptr);

// connect to an addrinfo, return fd
// if @addr_ptr is set, it places the addrinfo list element used to connect
// returns -1 if error
int ai_connect(struct addrinfo *addr, struct addrinfo **addr_ptr);

#if defined(__cplusplus)
} // end  extern "C"
#endif


#endif /* NET_HELPERS_H__ */
