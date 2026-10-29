#ifndef CE_POSIX_SYS_SOCKET_H
#define CE_POSIX_SYS_SOCKET_H

#include <sys/types.h>

typedef unsigned short sa_family_t;
typedef int socklen_t;

#define AF_INET 2
#define AF_INET6 23
#define SOCK_STREAM 1

int socket(int domain, int type, int protocol);

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

#endif
