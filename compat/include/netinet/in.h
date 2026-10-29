#ifndef CE_POSIX_NETINET_IN_H
#define CE_POSIX_NETINET_IN_H

#include <sys/socket.h>

struct in_addr {
    uint32_t s_addr;
};

struct in6_addr {
    uint8_t s6_addr[16];
};

#endif
