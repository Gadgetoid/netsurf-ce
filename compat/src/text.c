#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

int ce_utf8_to_wide(const char *text, wchar_t *output, size_t capacity) {
    const unsigned char *input = (const unsigned char *)text;
    size_t written = 0;
    while (*input) {
        uint32_t code = *input++;
        int extra = 0;
        if (code >= 0xF0) {
            code &= 0x07;
            extra = 3;
        } else if (code >= 0xE0) {
            code &= 0x0F;
            extra = 2;
        } else if (code >= 0xC0) {
            code &= 0x1F;
            extra = 1;
        } else if (code >= 0x80) {
            code = 0xFFFD;
        }
        while (extra-- > 0 && (*input & 0xC0) == 0x80) {
            code = (code << 6) | (*input++ & 0x3F);
        }
        if (code >= 0x10000) {
            if (written + 2 >= capacity) {
                return -1;
            }
            code -= 0x10000;
            output[written++] = (wchar_t)(0xD800 | (code >> 10));
            output[written++] = (wchar_t)(0xDC00 | (code & 0x3FF));
        } else {
            if (written + 1 >= capacity) {
                return -1;
            }
            output[written++] = (wchar_t)code;
        }
    }
    output[written] = 0;
    return (int)written;
}

int ce_wide_to_utf8(const wchar_t *text, char *output, size_t capacity) {
    size_t written = 0;
    while (*text) {
        uint32_t code = (uint16_t)*text++;
        if (code >= 0xD800 && code < 0xDC00 && (uint16_t)*text >= 0xDC00 && (uint16_t)*text < 0xE000) {
            code = 0x10000 + ((code - 0xD800) << 10) + ((uint16_t)*text++ - 0xDC00);
        }
        char bytes[4];
        size_t length;
        if (code < 0x80) {
            bytes[0] = (char)code;
            length = 1;
        } else if (code < 0x800) {
            bytes[0] = (char)(0xC0 | (code >> 6));
            bytes[1] = (char)(0x80 | (code & 0x3F));
            length = 2;
        } else if (code < 0x10000) {
            bytes[0] = (char)(0xE0 | (code >> 12));
            bytes[1] = (char)(0x80 | ((code >> 6) & 0x3F));
            bytes[2] = (char)(0x80 | (code & 0x3F));
            length = 3;
        } else {
            bytes[0] = (char)(0xF0 | (code >> 18));
            bytes[1] = (char)(0x80 | ((code >> 12) & 0x3F));
            bytes[2] = (char)(0x80 | ((code >> 6) & 0x3F));
            bytes[3] = (char)(0x80 | (code & 0x3F));
            length = 4;
        }
        if (written + length >= capacity) {
            return -1;
        }
        memcpy(output + written, bytes, length);
        written += length;
    }
    output[written] = 0;
    return (int)written;
}

int strcasecmp(const char *first, const char *second) {
    return _stricmp(first, second);
}

int strncasecmp(const char *first, const char *second, size_t count) {
    return _strnicmp(first, second, count);
}

void *memmem(const void *haystack, size_t haystack_length, const void *needle, size_t needle_length) {
    const char *start = haystack;
    if (needle_length == 0) {
        return (void *)haystack;
    }
    while (haystack_length >= needle_length) {
        if (memcmp(start, needle, needle_length) == 0) {
            return (void *)start;
        }
        start++;
        haystack_length--;
    }
    return NULL;
}

char *strtok_r(char *text, const char *delimiters, char **state) {
    if (!text) {
        text = *state;
    }
    text += strspn(text, delimiters);
    if (!*text) {
        *state = text;
        return NULL;
    }
    char *end = text + strcspn(text, delimiters);
    if (*end) {
        *end++ = 0;
    }
    *state = end;
    return text;
}

size_t strlcpy(char *destination, const char *source, size_t size) {
    size_t length = strlen(source);
    if (size) {
        size_t copied = length < size - 1 ? length : size - 1;
        memcpy(destination, source, copied);
        destination[copied] = 0;
    }
    return length;
}
