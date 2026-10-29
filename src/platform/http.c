#include <windows.h>
#include <winsock.h>

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "http.h"
#include "tls.h"

#define WAIT_SLICE_MS 200
#define CONNECT_TIMEOUT_MS 30000
#define READ_TIMEOUT_MS 30000
#define HEAD_LIMIT 65536
#define BUFFER_SIZE 16384
#define WORKER_STACK_SIZE 65536
#define SHUTDOWN_WAIT_MS 3000
#define SHUTDOWN_POLL_MS 50

enum io_result {
    IO_OK = 0,
    IO_CLOSED = -1,
    IO_FAILED = -2,
    IO_TIMED_OUT = -3,
    IO_CANCELLED = -4,
};

struct ce_http {
    CRITICAL_SECTION lock;
    LONG references;
    volatile LONG cancelled;
    struct ce_http_request request;
    char *host;
    char *head;
    uint8_t *body;
    struct ce_http_event *first_event;
    struct ce_http_event *last_event;
    SOCKET socket;
    struct ce_tls *tls;
    enum io_result transport_error;
    uint8_t buffer[BUFFER_SIZE];
    size_t buffered;
    size_t buffer_start;
    z_stream inflater;
    bool inflating;
};

static void (*wake_owner)(void);
static volatile LONG running_workers;

static void release_reference(struct ce_http *transfer) {
    if (InterlockedDecrement(&transfer->references) != 0) {
        return;
    }
    ce_http_free_events(transfer->first_event);
    DeleteCriticalSection(&transfer->lock);
    free(transfer->host);
    free(transfer->head);
    free(transfer->body);
    free(transfer);
}

static struct ce_http_event *new_event(enum ce_http_event_type type) {
    struct ce_http_event *event = calloc(1, sizeof *event);
    if (event) {
        event->type = type;
    }
    return event;
}

static void push_event(struct ce_http *transfer, struct ce_http_event *event) {
    if (!event) {
        return;
    }
    EnterCriticalSection(&transfer->lock);
    if (transfer->last_event) {
        transfer->last_event->next = event;
    } else {
        transfer->first_event = event;
    }
    transfer->last_event = event;
    LeaveCriticalSection(&transfer->lock);
    if (wake_owner) {
        wake_owner();
    }
}

static void push_bytes(struct ce_http *transfer, enum ce_http_event_type type, const void *data, size_t length) {
    struct ce_http_event *event = new_event(type);
    if (!event) {
        return;
    }
    event->data = malloc(length + 1);
    if (!event->data) {
        free(event);
        return;
    }
    memcpy(event->data, data, length);
    event->data[length] = 0;
    event->length = length;
    push_event(transfer, event);
}

static void push_failure(struct ce_http *transfer, enum ce_http_event_type type, const char *message) {
    push_bytes(transfer, type, message, strlen(message));
}

static enum io_result wait_socket(struct ce_http *transfer, bool for_write, int timeout_ms) {
    for (int waited = 0; waited < timeout_ms; waited += WAIT_SLICE_MS) {
        if (transfer->cancelled) {
            return IO_CANCELLED;
        }
        fd_set ready;
        fd_set failed;
        FD_ZERO(&ready);
        FD_ZERO(&failed);
        FD_SET(transfer->socket, &ready);
        FD_SET(transfer->socket, &failed);
        struct timeval slice = { 0, WAIT_SLICE_MS * 1000 };
        int count = select(0, for_write ? NULL : &ready, for_write ? &ready : NULL, &failed, &slice);
        if (count < 0) {
            return IO_FAILED;
        }
        if (count > 0) {
            return FD_ISSET(transfer->socket, &ready) ? IO_OK : IO_FAILED;
        }
    }
    return IO_TIMED_OUT;
}

static int socket_read(void *context, unsigned char *data, size_t length) {
    struct ce_http *transfer = context;
    enum io_result result = wait_socket(transfer, false, READ_TIMEOUT_MS);
    if (result != IO_OK) {
        transfer->transport_error = result;
        return -1;
    }
    int count = recv(transfer->socket, (char *)data, (int)length, 0);
    if (count < 0) {
        transfer->transport_error = IO_FAILED;
        return -1;
    }
    if (count == 0) {
        transfer->transport_error = IO_CLOSED;
        return -1;
    }
    return count;
}

static int socket_write(void *context, const unsigned char *data, size_t length) {
    struct ce_http *transfer = context;
    enum io_result result = wait_socket(transfer, true, READ_TIMEOUT_MS);
    if (result != IO_OK) {
        transfer->transport_error = result;
        return -1;
    }
    int count = send(transfer->socket, (const char *)data, (int)length, 0);
    if (count <= 0) {
        transfer->transport_error = IO_FAILED;
        return -1;
    }
    return count;
}

static int transport_read(struct ce_http *transfer, void *buffer, size_t capacity) {
    if (transfer->tls) {
        int count = ce_tls_read(transfer->tls, buffer, capacity);
        if (count == 0 || (count < 0 && transfer->transport_error == IO_CLOSED)) {
            return IO_CLOSED;
        }
        return count < 0 ? (transfer->transport_error ? transfer->transport_error : IO_FAILED) : count;
    }
    int count = socket_read(transfer, buffer, capacity);
    return count < 0 ? transfer->transport_error : count;
}

static bool transport_write(struct ce_http *transfer, const void *data, size_t length) {
    if (transfer->tls) {
        return ce_tls_write_all(transfer->tls, data, length);
    }
    const unsigned char *bytes = data;
    while (length) {
        int count = socket_write(transfer, bytes, length);
        if (count < 0) {
            return false;
        }
        bytes += count;
        length -= (size_t)count;
    }
    return true;
}

static int fill_buffer(struct ce_http *transfer) {
    if (transfer->buffer_start) {
        memmove(transfer->buffer, transfer->buffer + transfer->buffer_start, transfer->buffered);
        transfer->buffer_start = 0;
    }
    if (transfer->buffered == BUFFER_SIZE) {
        return IO_FAILED;
    }
    int count = transport_read(transfer, transfer->buffer + transfer->buffered, BUFFER_SIZE - transfer->buffered);
    if (count > 0) {
        transfer->buffered += (size_t)count;
    }
    return count;
}

static void consume(struct ce_http *transfer, size_t length) {
    transfer->buffer_start += length;
    transfer->buffered -= length;
}

static int read_line(struct ce_http *transfer, char *line, size_t capacity) {
    for (;;) {
        uint8_t *start = transfer->buffer + transfer->buffer_start;
        uint8_t *end = memchr(start, '\n', transfer->buffered);
        if (end) {
            size_t length = (size_t)(end - start);
            size_t content = length && start[length - 1] == '\r' ? length - 1 : length;
            if (content >= capacity) {
                return IO_FAILED;
            }
            memcpy(line, start, content);
            line[content] = 0;
            consume(transfer, length + 1);
            return (int)content;
        }
        int count = fill_buffer(transfer);
        if (count <= 0) {
            return count == 0 ? IO_CLOSED : count;
        }
    }
}

static bool deliver(struct ce_http *transfer, const uint8_t *data, size_t length) {
    if (!transfer->inflating) {
        push_bytes(transfer, CE_HTTP_EVENT_DATA, data, length);
        return true;
    }
    uint8_t output[BUFFER_SIZE];
    transfer->inflater.next_in = (Bytef *)data;
    transfer->inflater.avail_in = (uInt)length;
    while (transfer->inflater.avail_in) {
        transfer->inflater.next_out = output;
        transfer->inflater.avail_out = sizeof output;
        int result = inflate(&transfer->inflater, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR) {
            return false;
        }
        size_t produced = sizeof output - transfer->inflater.avail_out;
        if (produced) {
            push_bytes(transfer, CE_HTTP_EVENT_DATA, output, produced);
        }
        if (result == Z_STREAM_END || (result == Z_BUF_ERROR && !produced)) {
            break;
        }
    }
    return true;
}

static int read_body_bytes(struct ce_http *transfer, size_t length) {
    while (length) {
        if (!transfer->buffered) {
            int count = fill_buffer(transfer);
            if (count <= 0) {
                return count == 0 ? IO_CLOSED : count;
            }
        }
        size_t take = transfer->buffered < length ? transfer->buffered : length;
        if (!deliver(transfer, transfer->buffer + transfer->buffer_start, take)) {
            return IO_FAILED;
        }
        consume(transfer, take);
        length -= take;
    }
    return IO_OK;
}

static int read_chunked_body(struct ce_http *transfer) {
    char line[256];
    for (;;) {
        int result = read_line(transfer, line, sizeof line);
        if (result < 0) {
            return result;
        }
        size_t length = strtoul(line, NULL, 16);
        if (length == 0) {
            do {
                result = read_line(transfer, line, sizeof line);
            } while (result > 0);
            return result == 0 || result == IO_CLOSED ? IO_OK : result;
        }
        result = read_body_bytes(transfer, length);
        if (result != IO_OK) {
            return result;
        }
        result = read_line(transfer, line, sizeof line);
        if (result < 0) {
            return result;
        }
    }
}

static int read_body_until_closed(struct ce_http *transfer) {
    for (;;) {
        if (transfer->buffered) {
            if (!deliver(transfer, transfer->buffer + transfer->buffer_start, transfer->buffered)) {
                return IO_FAILED;
            }
            consume(transfer, transfer->buffered);
        }
        int count = fill_buffer(transfer);
        if (count == 0 || count == IO_CLOSED) {
            return IO_OK;
        }
        if (count < 0) {
            return count;
        }
    }
}

static bool header_is(const char *line, const char *name) {
    size_t length = strlen(name);
    return _strnicmp(line, name, length) == 0 && line[length] == ':';
}

static const char *header_value(const char *line) {
    const char *value = strchr(line, ':') + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    return value;
}

static bool contains_token(const char *value, const char *token) {
    size_t length = strlen(token);
    for (const char *search = value; *search; search++) {
        if (_strnicmp(search, token, length) == 0) {
            return true;
        }
    }
    return false;
}

static int read_response(struct ce_http *transfer) {
    char line[8192];
    int status;
    int result;
    do {
        result = read_line(transfer, line, sizeof line);
        if (result < 0) {
            return result;
        }
        if (strncmp(line, "HTTP/", 5) != 0 || !strchr(line, ' ')) {
            return IO_FAILED;
        }
        status = atoi(strchr(line, ' ') + 1);
        if (status >= 100 && status < 200) {
            do {
                result = read_line(transfer, line, sizeof line);
            } while (result > 0);
            if (result < 0) {
                return result;
            }
        }
    } while (status >= 100 && status < 200);

    struct ce_http_event *status_event = new_event(CE_HTTP_EVENT_STATUS);
    if (status_event) {
        status_event->status = status;
        push_event(transfer, status_event);
    }

    bool chunked = false;
    bool gzip = false;
    long content_length = -1;
    char *deferred_length = NULL;
    size_t head_size = 0;
    for (;;) {
        result = read_line(transfer, line, sizeof line);
        if (result < 0) {
            free(deferred_length);
            return result;
        }
        if (result == 0) {
            break;
        }
        head_size += (size_t)result;
        if (head_size > HEAD_LIMIT) {
            free(deferred_length);
            return IO_FAILED;
        }
        if (header_is(line, "Transfer-Encoding")) {
            chunked = contains_token(header_value(line), "chunked");
            continue;
        }
        if (header_is(line, "Content-Encoding") && contains_token(header_value(line), "gzip")) {
            gzip = true;
            continue;
        }
        if (header_is(line, "Content-Length")) {
            content_length = atol(header_value(line));
            free(deferred_length);
            deferred_length = strdup(line);
            continue;
        }
        push_bytes(transfer, CE_HTTP_EVENT_HEADER, line, strlen(line));
    }
    if (deferred_length && !gzip) {
        push_bytes(transfer, CE_HTTP_EVENT_HEADER, deferred_length, strlen(deferred_length));
    }
    free(deferred_length);

    if (!transfer->request.expect_body || status == 204 || status == 304) {
        return IO_OK;
    }
    if (gzip) {
        memset(&transfer->inflater, 0, sizeof transfer->inflater);
        if (inflateInit2(&transfer->inflater, 32 + MAX_WBITS) != Z_OK) {
            return IO_FAILED;
        }
        transfer->inflating = true;
    }
    if (chunked) {
        result = read_chunked_body(transfer);
    } else if (content_length >= 0) {
        result = read_body_bytes(transfer, (size_t)content_length);
    } else {
        result = read_body_until_closed(transfer);
    }
    if (transfer->inflating) {
        inflateEnd(&transfer->inflater);
        transfer->inflating = false;
    }
    return result;
}

static void report_certificates(struct ce_http *transfer) {
    struct ce_http_event *event = new_event(CE_HTTP_EVENT_CERTIFICATES);
    if (!event) {
        return;
    }
    event->certificate_count = ce_tls_take_certificates(transfer->tls, event->certificates, &event->problem);
    push_event(transfer, event);
}

static bool open_connection(struct ce_http *transfer) {
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(transfer->request.port);
    unsigned long numeric = inet_addr(transfer->host);
    if (numeric != INADDR_NONE) {
        address.sin_addr.s_addr = numeric;
    } else {
        struct hostent *entry = gethostbyname(transfer->host);
        if (!entry || entry->h_addrtype != AF_INET || !entry->h_addr_list[0]) {
            push_failure(transfer, CE_HTTP_EVENT_FAILED, "Could not resolve the server's name.");
            return false;
        }
        memcpy(&address.sin_addr, entry->h_addr_list[0], sizeof address.sin_addr);
    }
    if (transfer->cancelled) {
        return false;
    }
    transfer->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (transfer->socket == INVALID_SOCKET) {
        push_failure(transfer, CE_HTTP_EVENT_FAILED, "Could not create a network connection.");
        return false;
    }
    int no_delay = 1;
    setsockopt(transfer->socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&no_delay, sizeof no_delay);
    if (connect(transfer->socket, (const struct sockaddr *)&address, sizeof address) != 0) {
        push_failure(transfer, CE_HTTP_EVENT_FAILED, "Could not connect to the server.");
        return false;
    }
    if (!transfer->request.secure) {
        return true;
    }
    enum ce_tls_status status;
    transfer->tls = ce_tls_connect(transfer->host, transfer->request.port, transfer->request.accept_any_certificate,
                                   socket_read, socket_write, transfer, &status);
    if (!transfer->tls) {
        push_failure(transfer, CE_HTTP_EVENT_TLS_FAILED, "Out of memory.");
        return false;
    }
    switch (status) {
    case CE_TLS_OK:
        report_certificates(transfer);
        return true;
    case CE_TLS_CERTIFICATE_FAILED:
        report_certificates(transfer);
        return false;
    case CE_TLS_TRANSPORT_FAILED:
        if (transfer->transport_error == IO_TIMED_OUT) {
            push_failure(transfer, CE_HTTP_EVENT_TIMED_OUT, "The server did not respond.");
        } else {
            push_failure(transfer, CE_HTTP_EVENT_FAILED, "The connection was closed.");
        }
        return false;
    default:
        push_failure(transfer, CE_HTTP_EVENT_TLS_FAILED, "The secure connection failed.");
        return false;
    }
}

static DWORD WINAPI run_transfer(LPVOID parameter) {
    struct ce_http *transfer = parameter;
    if (open_connection(transfer) && !transfer->cancelled) {
        int result = IO_FAILED;
        if (transport_write(transfer, transfer->head, transfer->request.head_length) &&
            transport_write(transfer, transfer->body, transfer->request.body_length)) {
            result = read_response(transfer);
        } else if (transfer->transport_error) {
            result = transfer->transport_error;
        }
        switch (result) {
        case IO_OK:
            push_event(transfer, new_event(CE_HTTP_EVENT_FINISHED));
            break;
        case IO_TIMED_OUT:
            push_failure(transfer, CE_HTTP_EVENT_TIMED_OUT, "The server did not respond.");
            break;
        case IO_CANCELLED:
            break;
        case IO_CLOSED:
            push_failure(transfer, CE_HTTP_EVENT_FAILED, "The connection was closed.");
            break;
        default:
            push_failure(transfer, CE_HTTP_EVENT_FAILED, "The server's response could not be read.");
            break;
        }
    }
    if (transfer->tls) {
        ce_tls_close(transfer->tls);
    }
    if (transfer->socket != INVALID_SOCKET) {
        closesocket(transfer->socket);
    }
    release_reference(transfer);
    InterlockedDecrement(&running_workers);
    return 0;
}

bool ce_http_initialise(void (*wake)(void)) {
    WSADATA data;
    wake_owner = wake;
    return WSAStartup(MAKEWORD(1, 1), &data) == 0 && ce_tls_initialise();
}

void ce_http_finalise(void) {
    for (int waited = 0; running_workers && waited < SHUTDOWN_WAIT_MS; waited += SHUTDOWN_POLL_MS) {
        Sleep(SHUTDOWN_POLL_MS);
    }
    if (running_workers) {
        return;
    }
    ce_tls_finalise();
    WSACleanup();
}

static void *copy_memory(const void *data, size_t length) {
    void *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, data, length);
        ((char *)copy)[length] = 0;
    }
    return copy;
}

struct ce_http *ce_http_start(const struct ce_http_request *request) {
    struct ce_http *transfer = calloc(1, sizeof *transfer);
    if (!transfer) {
        return NULL;
    }
    InitializeCriticalSection(&transfer->lock);
    transfer->references = 2;
    transfer->socket = INVALID_SOCKET;
    transfer->request = *request;
    transfer->host = copy_memory(request->host, strlen(request->host));
    transfer->head = copy_memory(request->head, request->head_length);
    transfer->body = copy_memory(request->body ? request->body : (const uint8_t *)"", request->body_length);
    if (!transfer->host || !transfer->head || !transfer->body) {
        transfer->references = 1;
        release_reference(transfer);
        return NULL;
    }
    InterlockedIncrement(&running_workers);
    HANDLE thread = CreateThread(NULL, WORKER_STACK_SIZE, run_transfer, transfer, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!thread) {
        InterlockedDecrement(&running_workers);
        transfer->references = 1;
        release_reference(transfer);
        return NULL;
    }
    CloseHandle(thread);
    return transfer;
}

struct ce_http_event *ce_http_take_events(struct ce_http *transfer) {
    EnterCriticalSection(&transfer->lock);
    struct ce_http_event *events = transfer->first_event;
    transfer->first_event = NULL;
    transfer->last_event = NULL;
    LeaveCriticalSection(&transfer->lock);
    return events;
}

void ce_http_free_events(struct ce_http_event *events) {
    while (events) {
        struct ce_http_event *next = events->next;
        for (size_t index = 0; index < events->certificate_count; index++) {
            free(events->certificates[index].der);
        }
        free(events->data);
        free(events);
        events = next;
    }
}

void ce_http_release(struct ce_http *transfer) {
    InterlockedExchange(&transfer->cancelled, 1);
    release_reference(transfer);
}
