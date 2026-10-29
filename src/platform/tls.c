#include <windows.h>
#include <wincrypt.h>

#include <bearssl.h>
#include <ce_posix.h>
#include <stdlib.h>
#include <string.h>

#include "tls.h"
#include "trust_anchors.h"

#define SESSION_CACHE_SIZE 16
#define HOST_LENGTH 256
#define MAX_CERTIFICATE_LENGTH 16384
#define UNIX_EPOCH_DAYS 719528

struct cached_session {
    char host[HOST_LENGTH];
    uint16_t port;
    br_ssl_session_parameters parameters;
    uint32_t last_used;
};

struct recording_validator {
    const br_x509_class *vtable;
    br_x509_minimal_context minimal;
    br_x509_decoder_context leaf_decoder;
    bool accept_any_certificate;
    unsigned error;
    size_t count;
    struct ce_http_certificate certificates[CE_HTTP_MAX_CERTIFICATES];
    size_t current_capacity;
    bool recording;
};

struct ce_tls {
    br_ssl_client_context client;
    struct recording_validator validator;
    br_sslio_context io;
    unsigned char buffer[BR_SSL_BUFSIZE_BIDI];
    char host[HOST_LENGTH];
    uint16_t port;
};

static CRITICAL_SECTION tls_lock;
static struct cached_session sessions[SESSION_CACHE_SIZE];
static uint32_t session_clock;

bool ce_tls_initialise(void) {
    InitializeCriticalSection(&tls_lock);
    return true;
}

void ce_tls_finalise(void) {
    DeleteCriticalSection(&tls_lock);
}

static struct cached_session *find_session(const char *host, uint16_t port) {
    for (size_t index = 0; index < SESSION_CACHE_SIZE; index++) {
        if (sessions[index].port == port && strcmp(sessions[index].host, host) == 0) {
            return &sessions[index];
        }
    }
    return NULL;
}

static bool load_session(const char *host, uint16_t port, br_ssl_session_parameters *parameters) {
    EnterCriticalSection(&tls_lock);
    struct cached_session *session = find_session(host, port);
    if (session) {
        *parameters = session->parameters;
        session->last_used = ++session_clock;
    }
    LeaveCriticalSection(&tls_lock);
    return session != NULL;
}

static void store_session(const char *host, uint16_t port, const br_ssl_session_parameters *parameters) {
    EnterCriticalSection(&tls_lock);
    struct cached_session *session = find_session(host, port);
    if (!session) {
        session = &sessions[0];
        for (size_t index = 1; index < SESSION_CACHE_SIZE; index++) {
            if (sessions[index].last_used < session->last_used) {
                session = &sessions[index];
            }
        }
    }
    strncpy(session->host, host, HOST_LENGTH - 1);
    session->host[HOST_LENGTH - 1] = 0;
    session->port = port;
    session->parameters = *parameters;
    session->last_used = ++session_clock;
    LeaveCriticalSection(&tls_lock);
}

static void forget_session(const char *host, uint16_t port) {
    EnterCriticalSection(&tls_lock);
    struct cached_session *session = find_session(host, port);
    if (session) {
        memset(session, 0, sizeof *session);
    }
    LeaveCriticalSection(&tls_lock);
}

static bool system_random(void *buffer, size_t length) {
    typedef BOOL(WINAPI * ce_gen_random_function)(DWORD, BYTE *);
    typedef BOOL(WINAPI * acquire_function)(HCRYPTPROV *, LPCWSTR, LPCWSTR, DWORD, DWORD);
    typedef BOOL(WINAPI * generate_function)(HCRYPTPROV, DWORD, BYTE *);
    typedef BOOL(WINAPI * release_function)(HCRYPTPROV, DWORD);
    HMODULE core = GetModuleHandleW(L"coredll.dll");
    if (!core) {
        return false;
    }
    ce_gen_random_function ce_gen_random = (ce_gen_random_function)GetProcAddressW(core, L"CeGenRandom");
    if (ce_gen_random) {
        return ce_gen_random((DWORD)length, buffer) != FALSE;
    }
    acquire_function acquire = (acquire_function)GetProcAddressW(core, L"CryptAcquireContextW");
    generate_function generate = (generate_function)GetProcAddressW(core, L"CryptGenRandom");
    release_function release = (release_function)GetProcAddressW(core, L"CryptReleaseContext");
    HCRYPTPROV provider = 0;
    if (!acquire || !generate || !release || !acquire(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }
    BOOL filled = generate(provider, (DWORD)length, buffer);
    release(provider, 0);
    return filled != FALSE;
}

static struct recording_validator *as_validator(const br_x509_class **context) {
    return (struct recording_validator *)context;
}

static void validator_start_chain(const br_x509_class **context, const char *server_name) {
    struct recording_validator *validator = as_validator(context);
    validator->minimal.vtable->start_chain(&validator->minimal.vtable, server_name);
    br_x509_decoder_init(&validator->leaf_decoder, NULL, NULL);
}

static void validator_start_cert(const br_x509_class **context, uint32_t length) {
    struct recording_validator *validator = as_validator(context);
    validator->minimal.vtable->start_cert(&validator->minimal.vtable, length);
    validator->recording = validator->count < CE_HTTP_MAX_CERTIFICATES && length <= MAX_CERTIFICATE_LENGTH;
    if (validator->recording) {
        struct ce_http_certificate *certificate = &validator->certificates[validator->count];
        certificate->der = malloc(length ? length : 1);
        certificate->length = 0;
        validator->recording = certificate->der != NULL;
    }
}

static void validator_append(const br_x509_class **context, const unsigned char *data, size_t length) {
    struct recording_validator *validator = as_validator(context);
    validator->minimal.vtable->append(&validator->minimal.vtable, data, length);
    if (validator->count == 0) {
        br_x509_decoder_push(&validator->leaf_decoder, data, length);
    }
    if (validator->recording) {
        struct ce_http_certificate *certificate = &validator->certificates[validator->count];
        memcpy(certificate->der + certificate->length, data, length);
        certificate->length += length;
    }
}

static void validator_end_cert(const br_x509_class **context) {
    struct recording_validator *validator = as_validator(context);
    validator->minimal.vtable->end_cert(&validator->minimal.vtable);
    if (validator->recording) {
        validator->count++;
    }
    validator->recording = false;
}

static unsigned validator_end_chain(const br_x509_class **context) {
    struct recording_validator *validator = as_validator(context);
    validator->error = validator->minimal.vtable->end_chain(&validator->minimal.vtable);
    if (validator->accept_any_certificate && br_x509_decoder_get_pkey(&validator->leaf_decoder)) {
        return 0;
    }
    return validator->error;
}

static const br_x509_pkey *validator_get_pkey(const br_x509_class *const *context, unsigned *usages) {
    struct recording_validator *validator = (struct recording_validator *)context;
    if (validator->accept_any_certificate && validator->error != 0) {
        if (usages) {
            *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
        }
        return br_x509_decoder_get_pkey(&validator->leaf_decoder);
    }
    return validator->minimal.vtable->get_pkey(&validator->minimal.vtable, usages);
}

static const br_x509_class recording_validator_class = {
    sizeof(struct recording_validator),
    validator_start_chain,
    validator_start_cert,
    validator_append,
    validator_end_cert,
    validator_end_chain,
    validator_get_pkey,
};

static void set_validation_time(br_x509_minimal_context *minimal) {
    SYSTEMTIME now;
    FILETIME file_time;
    GetSystemTime(&now);
    SystemTimeToFileTime(&now, &file_time);
    int64_t intervals = ((int64_t)file_time.dwHighDateTime << 32) | file_time.dwLowDateTime;
    int64_t seconds = intervals / 10000000 - 11644473600LL;
    int64_t days = seconds / 86400;
    br_x509_minimal_set_time(minimal, (uint32_t)(days + UNIX_EPOCH_DAYS), (uint32_t)(seconds - days * 86400));
}

struct ce_tls *ce_tls_connect(const char *host, uint16_t port, bool accept_any_certificate, ce_tls_read_function read,
                              ce_tls_write_function write, void *context, enum ce_tls_status *status) {
    struct ce_tls *tls = calloc(1, sizeof *tls);
    unsigned char seed[32];
    br_ssl_session_parameters cached;
    *status = CE_TLS_FAILED;
    if (!tls) {
        return NULL;
    }
    strncpy(tls->host, host, HOST_LENGTH - 1);
    tls->port = port;

    size_t anchor_count;
    const br_x509_trust_anchor *anchors = ce_trust_anchors(&anchor_count);

    br_ssl_client_init_full(&tls->client, &tls->validator.minimal, anchors, anchor_count);
    tls->validator.vtable = &recording_validator_class;
    tls->validator.accept_any_certificate = accept_any_certificate;
    br_ssl_engine_set_x509(&tls->client.eng, &tls->validator.vtable);
    br_ssl_engine_set_buffer(&tls->client.eng, tls->buffer, sizeof tls->buffer, 1);
    set_validation_time(&tls->validator.minimal);

    if (!system_random(seed, sizeof seed)) {
        return tls;
    }
    DWORD ticks = GetTickCount();
    br_ssl_engine_inject_entropy(&tls->client.eng, seed, sizeof seed);
    br_ssl_engine_inject_entropy(&tls->client.eng, &ticks, sizeof ticks);

    bool resuming = !accept_any_certificate && load_session(host, port, &cached);
    if (resuming) {
        br_ssl_engine_set_session_parameters(&tls->client.eng, &cached);
    }
    if (!br_ssl_client_reset(&tls->client, host, resuming ? 1 : 0)) {
        return tls;
    }
    br_sslio_init(&tls->io, &tls->client.eng, read, context, write, context);
    if (br_sslio_flush(&tls->io) != 0) {
        int error = br_ssl_engine_last_error(&tls->client.eng);
        if (resuming) {
            forget_session(host, port);
        }
        if (error >= BR_ERR_X509_OK && error < BR_ERR_X509_OK + 64) {
            *status = CE_TLS_CERTIFICATE_FAILED;
        } else if (error == BR_ERR_IO) {
            *status = CE_TLS_TRANSPORT_FAILED;
        }
        return tls;
    }
    br_ssl_session_parameters current;
    br_ssl_engine_get_session_parameters(&tls->client.eng, &current);
    if (current.session_id_len && !accept_any_certificate) {
        store_session(host, port, &current);
    }
    *status = CE_TLS_OK;
    return tls;
}

int ce_tls_read(struct ce_tls *tls, void *buffer, size_t capacity) {
    int count = br_sslio_read(&tls->io, buffer, capacity);
    if (count > 0) {
        return count;
    }
    int error = br_ssl_engine_last_error(&tls->client.eng);
    return error == BR_ERR_OK ? 0 : -1;
}

bool ce_tls_write_all(struct ce_tls *tls, const void *data, size_t length) {
    return br_sslio_write_all(&tls->io, data, length) == 0 && br_sslio_flush(&tls->io) == 0;
}

size_t ce_tls_take_certificates(struct ce_tls *tls, struct ce_http_certificate *certificates,
                                enum ce_http_certificate_problem *problem) {
    struct recording_validator *validator = &tls->validator;
    switch (validator->error) {
    case 0:
    case BR_ERR_X509_OK:
        *problem = CE_HTTP_CERTIFICATE_VALID;
        break;
    case BR_ERR_X509_NOT_TRUSTED:
        *problem = CE_HTTP_CERTIFICATE_UNTRUSTED;
        break;
    case BR_ERR_X509_EXPIRED:
        *problem = CE_HTTP_CERTIFICATE_EXPIRED;
        break;
    case BR_ERR_X509_BAD_SERVER_NAME:
        *problem = CE_HTTP_CERTIFICATE_NAME_MISMATCH;
        break;
    default:
        *problem = CE_HTTP_CERTIFICATE_INVALID;
        break;
    }
    size_t count = validator->count;
    memcpy(certificates, validator->certificates, count * sizeof *certificates);
    validator->count = 0;
    return count;
}

void ce_tls_close(struct ce_tls *tls) {
    for (size_t index = 0; index < tls->validator.count; index++) {
        free(tls->validator.certificates[index].der);
    }
    if (tls->validator.recording) {
        free(tls->validator.certificates[tls->validator.count].der);
    }
    free(tls);
}
