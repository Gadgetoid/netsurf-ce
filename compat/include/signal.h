#ifndef CE_POSIX_SIGNAL_H
#define CE_POSIX_SIGNAL_H

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)
#define SIGINT 2
#define SIGPIPE 13
#define SIGTERM 15


#endif
