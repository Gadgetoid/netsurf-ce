#ifndef NETSURF_CE_PLATFORM_TLS_H
#define NETSURF_CE_PLATFORM_TLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "http.h"

enum ce_tls_status {
    CE_TLS_OK,
    CE_TLS_TRANSPORT_FAILED,
    CE_TLS_FAILED,
    CE_TLS_CERTIFICATE_FAILED,
};

typedef int (*ce_tls_read_function)(void *context, unsigned char *data, size_t length);
typedef int (*ce_tls_write_function)(void *context, const unsigned char *data, size_t length);

struct ce_tls;

bool ce_tls_initialise(void);
void ce_tls_finalise(void);
struct ce_tls *ce_tls_connect(const char *host, uint16_t port, bool accept_any_certificate, ce_tls_read_function read,
                              ce_tls_write_function write, void *context, enum ce_tls_status *status);
int ce_tls_read(struct ce_tls *tls, void *buffer, size_t capacity);
bool ce_tls_write_all(struct ce_tls *tls, const void *data, size_t length);
size_t ce_tls_take_certificates(struct ce_tls *tls, struct ce_http_certificate *certificates,
                                enum ce_http_certificate_problem *problem);
void ce_tls_close(struct ce_tls *tls);

#endif
