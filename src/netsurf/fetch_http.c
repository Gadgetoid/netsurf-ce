#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libwapcaplet/libwapcaplet.h>
#include <nsutils/base64.h>

#include "content/fetch.h"
#include "content/fetchers.h"
#include "content/fetchers/curl.h"
#include "content/urldb.h"
#include "netsurf/ssl_certs.h"
#include "utils/corestrings.h"
#include "utils/http.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/useragent.h"

#include "platform/http.h"

struct buffer {
    char *data;
    size_t length;
    size_t capacity;
    bool failed;
};

struct http_fetch {
    struct http_fetch *next;
    struct fetch *parent;
    nsurl *url;
    bool only_2xx;
    bool post;
    bool aborted;
    bool locked;
    bool completed;
    bool headers_handled;
    char *host;
    uint16_t port;
    bool secure;
    struct buffer request_headers;
    struct buffer body;
    char *content_type;
    struct ce_http *transfer;
    int status;
    char *location;
    char *realm;
};

static struct http_fetch *active_fetches;
static bool polling;

static void append(struct buffer *buffer, const void *data, size_t length) {
    if (buffer->failed) {
        return;
    }
    if (buffer->length + length + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 256;
        while (capacity < buffer->length + length + 1) {
            capacity *= 2;
        }
        char *grown = realloc(buffer->data, capacity);
        if (!grown) {
            buffer->failed = true;
            return;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    buffer->data[buffer->length] = 0;
}

static void append_text(struct buffer *buffer, const char *text) {
    append(buffer, text, strlen(text));
}

static void append_header(struct buffer *buffer, const char *name, const char *value) {
    append_text(buffer, name);
    append_text(buffer, ": ");
    append_text(buffer, value);
    append_text(buffer, "\r\n");
}

static void append_file(struct buffer *buffer, const char *path) {
    FILE *file = fopen(path, "rb");
    char chunk[1024];
    size_t length;
    if (!file) {
        return;
    }
    while ((length = fread(chunk, 1, sizeof chunk, file)) > 0) {
        append(buffer, chunk, length);
    }
    fclose(file);
}

static char *build_multipart(struct buffer *body, const struct fetch_multipart_data *parts) {
    char boundary[48];
    char *content_type = NULL;
    snprintf(boundary, sizeof boundary, "----NetSurfCE%08x%08x", (unsigned)rand(), (unsigned)(uintptr_t)parts);
    for (const struct fetch_multipart_data *part = parts; part; part = part->next) {
        append_text(body, "--");
        append_text(body, boundary);
        append_text(body, "\r\nContent-Disposition: form-data; name=\"");
        append_text(body, part->name);
        append_text(body, "\"");
        if (part->file) {
            append_text(body, "; filename=\"");
            append_text(body, part->value);
            append_text(body, "\"\r\nContent-Type: application/octet-stream\r\n\r\n");
            append_file(body, part->rawfile);
        } else {
            append_text(body, "\r\n\r\n");
            append_text(body, part->value);
        }
        append_text(body, "\r\n");
    }
    append_text(body, "--");
    append_text(body, boundary);
    append_text(body, "--\r\n");
    if (asprintf(&content_type, "multipart/form-data; boundary=%s", boundary) < 0) {
        return NULL;
    }
    return content_type;
}

static void free_fetch(void *context) {
    struct http_fetch *fetch = context;
    if (fetch->transfer) {
        ce_http_release(fetch->transfer);
    }
    nsurl_unref(fetch->url);
    free(fetch->host);
    free(fetch->request_headers.data);
    free(fetch->body.data);
    free(fetch->content_type);
    free(fetch->location);
    free(fetch->realm);
    free(fetch);
}

static void unlink_fetch(struct http_fetch *fetch) {
    for (struct http_fetch **link = &active_fetches; *link; link = &(*link)->next) {
        if (*link == fetch) {
            *link = fetch->next;
            return;
        }
    }
}

static void finish(struct http_fetch *fetch) {
    unlink_fetch(fetch);
    if (fetch->transfer) {
        ce_http_release(fetch->transfer);
        fetch->transfer = NULL;
    }
    fetch_remove_from_queues(fetch->parent);
    fetch_free(fetch->parent);
}

static void send_message(struct http_fetch *fetch, const fetch_msg *message) {
    fetch->locked = true;
    fetch_send_callback(message, fetch->parent);
    fetch->locked = false;
}

static bool initialise(lwc_string *scheme) {
    return true;
}

static void finalise(lwc_string *scheme) {
}

static bool acceptable(const struct nsurl *url) {
    return true;
}

static void *setup(struct fetch *parent, struct nsurl *url, bool only_2xx, bool downgrade_tls, const char *post_urlenc,
                   const struct fetch_multipart_data *post_multipart, const char **headers) {
    struct http_fetch *fetch = calloc(1, sizeof *fetch);
    if (!fetch) {
        return NULL;
    }
    fetch->parent = parent;
    fetch->url = nsurl_ref(url);
    fetch->only_2xx = only_2xx;

    lwc_string *scheme = nsurl_get_component(url, NSURL_SCHEME);
    lwc_string *host = nsurl_get_component(url, NSURL_HOST);
    lwc_string *port = nsurl_get_component(url, NSURL_PORT);
    bool matched = false;
    fetch->secure = scheme && lwc_string_caseless_isequal(scheme, corestring_lwc_https, &matched) == lwc_error_ok && matched;
    fetch->port = port ? (uint16_t)atoi(lwc_string_data(port)) : fetch->secure ? 443 : 80;
    fetch->host = host ? strdup(lwc_string_data(host)) : NULL;
    if (scheme) {
        lwc_string_unref(scheme);
    }
    if (host) {
        lwc_string_unref(host);
    }
    if (port) {
        lwc_string_unref(port);
    }
    if (!fetch->host) {
        free_fetch(fetch);
        return NULL;
    }

    if (post_urlenc) {
        fetch->post = true;
        append_text(&fetch->body, post_urlenc);
        fetch->content_type = strdup("application/x-www-form-urlencoded");
    } else if (post_multipart) {
        fetch->post = true;
        fetch->content_type = build_multipart(&fetch->body, post_multipart);
    }

    if (nsoption_charp(accept_language) && nsoption_charp(accept_language)[0]) {
        char value[96];
        snprintf(value, sizeof value, "%s, *;q=0.1", nsoption_charp(accept_language));
        append_header(&fetch->request_headers, "Accept-Language", value);
    }
    if (nsoption_bool(do_not_track)) {
        append_header(&fetch->request_headers, "DNT", "1");
    }
    for (size_t index = 0; headers[index]; index++) {
        append_text(&fetch->request_headers, headers[index]);
        append_text(&fetch->request_headers, "\r\n");
    }
    if (fetch->request_headers.failed || fetch->body.failed || (fetch->post && !fetch->content_type)) {
        free_fetch(fetch);
        return NULL;
    }
    return fetch;
}

static bool build_request(struct http_fetch *fetch, struct buffer *head) {
    char *path = NULL;
    size_t path_length = 0;
    if (nsurl_get(fetch->url, NSURL_PATH | NSURL_QUERY, &path, &path_length) != NSERROR_OK) {
        return false;
    }
    append_text(head, fetch->post ? "POST " : "GET ");
    append(head, path, path_length);
    append_text(head, " HTTP/1.1\r\n");
    free(path);

    char host_value[300];
    bool default_port = fetch->port == (fetch->secure ? 443 : 80);
    snprintf(host_value, sizeof host_value, default_port ? "%s" : "%s:%u", fetch->host, (unsigned)fetch->port);
    append_header(head, "Host", host_value);
    append_header(head, "User-Agent", user_agent_string());
    append_header(head, "Accept", "*/*");
    append_header(head, "Accept-Encoding", "gzip");
    append_header(head, "Connection", "close");
    append(head, fetch->request_headers.data ? fetch->request_headers.data : "", fetch->request_headers.length);

    char *cookies = urldb_get_cookie(fetch->url, true);
    if (cookies) {
        append_header(head, "Cookie", cookies);
        free(cookies);
    }
    const char *credentials = urldb_get_auth_details(fetch->url, NULL);
    if (credentials) {
        uint8_t encoded[512];
        size_t encoded_length = sizeof encoded - 1;
        if (nsu_base64_encode((const uint8_t *)credentials, strlen(credentials), encoded, &encoded_length) == NSUERROR_OK) {
            encoded[encoded_length] = 0;
            append_text(head, "Authorization: Basic ");
            append_text(head, (const char *)encoded);
            append_text(head, "\r\n");
        }
    }
    if (fetch->post) {
        char length[24];
        snprintf(length, sizeof length, "%zu", fetch->body.length);
        append_header(head, "Content-Type", fetch->content_type);
        append_header(head, "Content-Length", length);
    }
    append_text(head, "\r\n");
    return !head->failed;
}

static bool start(void *context) {
    struct http_fetch *fetch = context;
    struct buffer head = { 0 };
    if (!build_request(fetch, &head)) {
        free(head.data);
        return false;
    }
    struct ce_http_request request = {
        .host = fetch->host,
        .port = fetch->port,
        .secure = fetch->secure,
        .accept_any_certificate = fetch->secure && urldb_get_cert_permissions(fetch->url),
        .expect_body = true,
        .head = head.data,
        .head_length = head.length,
        .body = (const uint8_t *)fetch->body.data,
        .body_length = fetch->body.length,
    };
    fetch->transfer = ce_http_start(&request);
    free(head.data);
    if (!fetch->transfer) {
        return false;
    }
    fetch->next = active_fetches;
    active_fetches = fetch;
    return true;
}

static void abort_fetch(void *context) {
    struct http_fetch *fetch = context;
    if (fetch->transfer && (fetch->locked || polling)) {
        fetch->aborted = true;
        return;
    }
    finish(fetch);
}

static ssl_cert_err certificate_error(enum ce_http_certificate_problem problem) {
    switch (problem) {
    case CE_HTTP_CERTIFICATE_VALID:
        return SSL_CERT_ERR_OK;
    case CE_HTTP_CERTIFICATE_UNTRUSTED:
        return SSL_CERT_ERR_BAD_ISSUER;
    case CE_HTTP_CERTIFICATE_EXPIRED:
        return SSL_CERT_ERR_TOO_OLD;
    case CE_HTTP_CERTIFICATE_NAME_MISMATCH:
        return SSL_CERT_ERR_HOSTNAME_MISMATCH;
    default:
        return SSL_CERT_ERR_UNKNOWN;
    }
}

static bool report_certificates(struct http_fetch *fetch, struct ce_http_event *event) {
    struct cert_chain *chain = NULL;
    if (event->certificate_count && cert_chain_alloc(event->certificate_count, &chain) == NSERROR_OK) {
        for (size_t index = 0; index < event->certificate_count; index++) {
            chain->certs[index].der = event->certificates[index].der;
            chain->certs[index].der_length = event->certificates[index].length;
            chain->certs[index].err = SSL_CERT_ERR_OK;
            event->certificates[index].der = NULL;
        }
        chain->certs[0].err = certificate_error(event->problem);
        fetch_msg message = { .type = FETCH_CERTS, .data.chain = chain };
        send_message(fetch, &message);
        cert_chain_free(chain);
    }
    if (event->problem != CE_HTTP_CERTIFICATE_VALID && !fetch->aborted) {
        fetch_msg message = { .type = FETCH_CERT_ERR };
        send_message(fetch, &message);
        return true;
    }
    return false;
}

static void remember_header(struct http_fetch *fetch, const char *line) {
    const char *colon = strchr(line, ':');
    if (!colon) {
        return;
    }
    const char *value = colon + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    size_t name_length = (size_t)(colon - line);
    if (name_length == 8 && strncasecmp(line, "Location", 8) == 0) {
        free(fetch->location);
        fetch->location = strdup(value);
    } else if (name_length == 10 && strncasecmp(line, "Set-Cookie", 10) == 0) {
        fetch_set_cookie(fetch->parent, value);
    } else if (name_length == 16 && strncasecmp(line, "WWW-Authenticate", 16) == 0) {
        const char *realm = strstr(value, "realm=\"");
        if (realm) {
            realm += 7;
            const char *end = strchr(realm, '"');
            free(fetch->realm);
            fetch->realm = end ? strndup(realm, (size_t)(end - realm)) : strdup(realm);
        }
    }
}

static bool handle_headers(struct http_fetch *fetch) {
    fetch_msg message;
    fetch->headers_handled = true;
    if (fetch->status == 304 && !fetch->post) {
        message.type = FETCH_NOTMODIFIED;
    } else if (fetch->status >= 300 && fetch->status < 400 && fetch->location) {
        message.type = FETCH_REDIRECT;
        message.data.redirect = fetch->location;
    } else if (fetch->status == 401) {
        message.type = FETCH_AUTH;
        message.data.auth.realm = fetch->realm ? fetch->realm : "";
    } else if (fetch->only_2xx && (fetch->status < 200 || fetch->status >= 300)) {
        message.type = FETCH_ERROR;
        message.data.error = messages_get("Not2xx");
    } else {
        return false;
    }
    send_message(fetch, &message);
    return true;
}

static bool process_event(struct http_fetch *fetch, struct ce_http_event *event) {
    fetch_msg message;
    switch (event->type) {
    case CE_HTTP_EVENT_CERTIFICATES:
        return report_certificates(fetch, event);
    case CE_HTTP_EVENT_STATUS:
        fetch->status = event->status;
        fetch_set_http_code(fetch->parent, event->status);
        return false;
    case CE_HTTP_EVENT_HEADER:
        remember_header(fetch, (const char *)event->data);
        message.type = FETCH_HEADER;
        message.data.header_or_data.buf = event->data;
        message.data.header_or_data.len = event->length;
        send_message(fetch, &message);
        return false;
    case CE_HTTP_EVENT_DATA:
        if (!fetch->headers_handled && handle_headers(fetch)) {
            return true;
        }
        message.type = FETCH_DATA;
        message.data.header_or_data.buf = event->data;
        message.data.header_or_data.len = event->length;
        send_message(fetch, &message);
        return false;
    case CE_HTTP_EVENT_FINISHED:
        if (!fetch->headers_handled && handle_headers(fetch)) {
            return true;
        }
        message.type = FETCH_FINISHED;
        send_message(fetch, &message);
        return true;
    case CE_HTTP_EVENT_TLS_FAILED:
        message.type = FETCH_SSL_ERR;
        send_message(fetch, &message);
        return true;
    case CE_HTTP_EVENT_TIMED_OUT:
        message.type = FETCH_TIMEDOUT;
        message.data.error = (const char *)event->data;
        send_message(fetch, &message);
        return true;
    case CE_HTTP_EVENT_FAILED:
        message.type = FETCH_ERROR;
        message.data.error = (const char *)event->data;
        send_message(fetch, &message);
        return true;
    }
    return false;
}

static void poll(lwc_string *scheme) {
    if (polling) {
        return;
    }
    polling = true;
    for (struct http_fetch *fetch = active_fetches; fetch; fetch = fetch->next) {
        if (fetch->locked || fetch->aborted || fetch->completed) {
            continue;
        }
        struct ce_http_event *events = ce_http_take_events(fetch->transfer);
        for (struct ce_http_event *event = events; event && !fetch->completed && !fetch->aborted; event = event->next) {
            fetch->completed = process_event(fetch, event);
        }
        ce_http_free_events(events);
    }
    polling = false;
    struct http_fetch *fetch = active_fetches;
    while (fetch) {
        struct http_fetch *next = fetch->next;
        if (fetch->aborted || fetch->completed) {
            finish(fetch);
        }
        fetch = next;
    }
}

static int fdset(lwc_string *scheme, fd_set *read_set, fd_set *write_set, fd_set *error_set) {
    return -1;
}

static const struct fetcher_operation_table http_fetcher = {
    .initialise = initialise,
    .acceptable = acceptable,
    .setup = setup,
    .start = start,
    .abort = abort_fetch,
    .free = free_fetch,
    .poll = poll,
    .fdset = fdset,
    .finalise = finalise,
};

nserror fetch_curl_register(void) {
    nserror error = fetcher_add(lwc_string_ref(corestring_lwc_http), &http_fetcher);
    if (error != NSERROR_OK) {
        return error;
    }
    return fetcher_add(lwc_string_ref(corestring_lwc_https), &http_fetcher);
}
