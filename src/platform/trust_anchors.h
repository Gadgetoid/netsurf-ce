#ifndef NETSURF_CE_PLATFORM_TRUST_ANCHORS_H
#define NETSURF_CE_PLATFORM_TRUST_ANCHORS_H

#include <bearssl.h>
#include <stddef.h>

const br_x509_trust_anchor *ce_trust_anchors(size_t *count);

#endif
