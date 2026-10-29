#ifndef NETSURF_CE_PLATFORM_HTTP_H
#define NETSURF_CE_PLATFORM_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CE_HTTP_MAX_CERTIFICATES 10

enum ce_http_event_type {
    CE_HTTP_EVENT_CERTIFICATES,
    CE_HTTP_EVENT_STATUS,
    CE_HTTP_EVENT_HEADER,
    CE_HTTP_EVENT_DATA,
    CE_HTTP_EVENT_FINISHED,
    CE_HTTP_EVENT_FAILED,
    CE_HTTP_EVENT_TLS_FAILED,
    CE_HTTP_EVENT_TIMED_OUT,
};

enum ce_http_certificate_problem {
    CE_HTTP_CERTIFICATE_VALID,
    CE_HTTP_CERTIFICATE_UNTRUSTED,
    CE_HTTP_CERTIFICATE_EXPIRED,
    CE_HTTP_CERTIFICATE_NAME_MISMATCH,
    CE_HTTP_CERTIFICATE_INVALID,
};

struct ce_http_certificate {
    uint8_t *der;
    size_t length;
};

struct ce_http_event {
    struct ce_http_event *next;
    enum ce_http_event_type type;
    int status;
    uint8_t *data;
    size_t length;
    enum ce_http_certificate_problem problem;
    size_t certificate_count;
    struct ce_http_certificate certificates[CE_HTTP_MAX_CERTIFICATES];
};

struct ce_http_request {
    const char *host;
    uint16_t port;
    bool secure;
    bool accept_any_certificate;
    bool expect_body;
    const char *head;
    size_t head_length;
    const uint8_t *body;
    size_t body_length;
};

struct ce_http;

bool ce_http_initialise(void (*wake)(void));
void ce_http_finalise(void);
struct ce_http *ce_http_start(const struct ce_http_request *request);
struct ce_http_event *ce_http_take_events(struct ce_http *transfer);
void ce_http_free_events(struct ce_http_event *events);
void ce_http_release(struct ce_http *transfer);

#endif
