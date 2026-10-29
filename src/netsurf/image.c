#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/errors.h"
#include "content/content.h"
#include "content/content_factory.h"
#include "content/content_protected.h"
#include "content/llcache.h"
#include "desktop/bitmap.h"
#include "desktop/gui_internal.h"
#include "image/image_cache.h"
#include "image/jpeg.h"
#include "image/png.h"
#include "netsurf/bitmap.h"
#include "utils/messages.h"
#include "utils/nsurl.h"

#include <stb_image.h>

static struct bitmap *decode_bitmap(struct content *c) {
    size_t size;
    const uint8_t *data = content__get_source_data(c, &size);
    int width;
    int height;
    int channels;
    if (!data || size > INT32_MAX) {
        return NULL;
    }
    uint8_t *pixels = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4);
    if (!pixels) {
        return NULL;
    }
    bool opaque = channels != 2 && channels != 4;
    struct bitmap *bitmap = guit->bitmap->create(width, height, opaque ? BITMAP_OPAQUE : BITMAP_NONE);
    uint8_t *buffer = bitmap ? guit->bitmap->get_buffer(bitmap) : NULL;
    if (!buffer) {
        if (bitmap) {
            guit->bitmap->destroy(bitmap);
        }
        stbi_image_free(pixels);
        return NULL;
    }
    size_t stride = guit->bitmap->get_rowstride(bitmap);
    for (int row = 0; row < height; row++) {
        memcpy(buffer + stride * (size_t)row, pixels + (size_t)width * 4 * (size_t)row, (size_t)width * 4);
    }
    stbi_image_free(pixels);
    if (!opaque) {
        opaque = bitmap_test_opaque(bitmap);
    }
    guit->bitmap->set_opaque(bitmap, opaque);
    bitmap_format_to_client(bitmap, &(bitmap_fmt_t){
                                        .layout = BITMAP_LAYOUT_R8G8B8A8,
                                        .pma = false,
                                    });
    guit->bitmap->modified(bitmap);
    return bitmap;
}

static nserror image_create(const content_handler *handler, lwc_string *mime_type, const struct http_parameter *params,
                            llcache_handle *llcache, const char *fallback_charset, bool quirks, struct content **c) {
    struct content *image = calloc(1, sizeof *image);
    if (!image) {
        return NSERROR_NOMEM;
    }
    nserror error = content__init(image, handler, mime_type, params, llcache, fallback_charset, quirks);
    if (error != NSERROR_OK) {
        free(image);
        return error;
    }
    *c = image;
    return NSERROR_OK;
}

static bool image_convert(struct content *c) {
    size_t size;
    const uint8_t *data = content__get_source_data(c, &size);
    int width;
    int height;
    int channels;
    if (!data || size > INT32_MAX || !stbi_info_from_memory(data, (int)size, &width, &height, &channels)) {
        union content_msg_data message = { .errordata = { .errorcode = NSERROR_UNKNOWN, .errormsg = "Image unreadable" } };
        content_broadcast(c, CONTENT_MSG_ERROR, &message);
        return false;
    }
    c->width = width;
    c->height = height;
    c->size = (size_t)width * (size_t)height * 4;
    image_cache_add(c, NULL, decode_bitmap);

    const char *title_key = size >= 4 && memcmp(data, "\x89PNG", 4) == 0 ? "PNGTitle" : "JPEGTitle";
    char *title = messages_get_buff(title_key, nsurl_access_leaf(llcache_handle_get_url(c->llcache)), c->width,
                                    c->height);
    if (title) {
        content__set_title(c, title);
        free(title);
    }
    content_set_ready(c);
    content_set_done(c);
    content_set_status(c, "");
    return true;
}

static nserror image_clone(const struct content *old, struct content **copy) {
    struct content *image = calloc(1, sizeof *image);
    if (!image) {
        return NSERROR_NOMEM;
    }
    nserror error = content__clone(old, image);
    if (error != NSERROR_OK) {
        content_destroy(image);
        return error;
    }
    if ((old->status == CONTENT_STATUS_READY || old->status == CONTENT_STATUS_DONE) && !image_convert(image)) {
        content_destroy(image);
        return NSERROR_CLONE_FAILED;
    }
    *copy = image;
    return NSERROR_OK;
}

static const content_handler image_content_handler = {
    .create = image_create,
    .data_complete = image_convert,
    .destroy = image_cache_destroy,
    .redraw = image_cache_redraw,
    .clone = image_clone,
    .get_internal = image_cache_get_internal,
    .type = image_cache_content_type,
    .is_opaque = image_cache_is_opaque,
    .no_share = false,
};

static const char *png_types[] = { "image/png", "image/x-png" };
static const char *jpeg_types[] = { "image/jpeg", "image/jpg", "image/pjpeg" };

nserror nspng_init(void) {
    for (size_t index = 0; index < sizeof png_types / sizeof png_types[0]; index++) {
        nserror error = content_factory_register_handler(png_types[index], &image_content_handler);
        if (error != NSERROR_OK) {
            return error;
        }
    }
    return NSERROR_OK;
}

nserror nsjpeg_init(void) {
    for (size_t index = 0; index < sizeof jpeg_types / sizeof jpeg_types[0]; index++) {
        nserror error = content_factory_register_handler(jpeg_types[index], &image_content_handler);
        if (error != NSERROR_OK) {
            return error;
        }
    }
    return NSERROR_OK;
}
