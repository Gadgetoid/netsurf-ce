#include <windows.h>

#include <errno.h>
#include <ctype.h>
#include <iconv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum encoding_kind {
    ENCODING_UTF8,
    ENCODING_UTF16LE,
    ENCODING_UTF16BE,
    ENCODING_UTF16,
    ENCODING_UCS4LE,
    ENCODING_UCS4BE,
    ENCODING_LATIN1,
    ENCODING_ASCII,
    ENCODING_CODE_PAGE,
};

struct encoding {
    enum encoding_kind kind;
    UINT code_page;
};

struct ce_iconv {
    struct encoding from;
    struct encoding to;
};

struct named_encoding {
    const char *name;
    enum encoding_kind kind;
    UINT code_page;
};

static const struct named_encoding encodings[] = {
    { "UTF-8", ENCODING_UTF8, 0 },
    { "UTF8", ENCODING_UTF8, 0 },
    { "UTF-16", ENCODING_UTF16, 0 },
    { "UTF-16LE", ENCODING_UTF16LE, 0 },
    { "UTF-16BE", ENCODING_UTF16BE, 0 },
    { "UCS-2", ENCODING_UTF16BE, 0 },
    { "UCS-2LE", ENCODING_UTF16LE, 0 },
    { "UCS-4", ENCODING_UCS4BE, 0 },
    { "UCS-4BE", ENCODING_UCS4BE, 0 },
    { "UCS-4LE", ENCODING_UCS4LE, 0 },
    { "UTF-32", ENCODING_UCS4BE, 0 },
    { "UTF-32BE", ENCODING_UCS4BE, 0 },
    { "UTF-32LE", ENCODING_UCS4LE, 0 },
    { "ISO-8859-1", ENCODING_LATIN1, 0 },
    { "ISO8859-1", ENCODING_LATIN1, 0 },
    { "LATIN1", ENCODING_LATIN1, 0 },
    { "US-ASCII", ENCODING_ASCII, 0 },
    { "ASCII", ENCODING_ASCII, 0 },
    { "SHIFT_JIS", ENCODING_CODE_PAGE, 932 },
    { "SHIFT-JIS", ENCODING_CODE_PAGE, 932 },
    { "SJIS", ENCODING_CODE_PAGE, 932 },
    { "WINDOWS-31J", ENCODING_CODE_PAGE, 932 },
    { "CP932", ENCODING_CODE_PAGE, 932 },
    { "EUC-JP", ENCODING_CODE_PAGE, 20932 },
    { "ISO-2022-JP", ENCODING_CODE_PAGE, 50220 },
    { "GBK", ENCODING_CODE_PAGE, 936 },
    { "GB2312", ENCODING_CODE_PAGE, 936 },
    { "CP936", ENCODING_CODE_PAGE, 936 },
    { "BIG5", ENCODING_CODE_PAGE, 950 },
    { "EUC-KR", ENCODING_CODE_PAGE, 949 },
    { "KOI8-R", ENCODING_CODE_PAGE, 20866 },
    { "WINDOWS-1250", ENCODING_CODE_PAGE, 1250 },
    { "WINDOWS-1251", ENCODING_CODE_PAGE, 1251 },
    { "WINDOWS-1252", ENCODING_CODE_PAGE, 1252 },
    { "WINDOWS-1253", ENCODING_CODE_PAGE, 1253 },
    { "WINDOWS-1254", ENCODING_CODE_PAGE, 1254 },
    { "WINDOWS-1255", ENCODING_CODE_PAGE, 1255 },
    { "WINDOWS-1256", ENCODING_CODE_PAGE, 1256 },
    { "WINDOWS-1257", ENCODING_CODE_PAGE, 1257 },
    { "WINDOWS-1258", ENCODING_CODE_PAGE, 1258 },
    { "CP1252", ENCODING_CODE_PAGE, 1252 },
};

static int find_encoding(const char *name, struct encoding *result) {
    char upper[32];
    size_t length = strcspn(name, "/");
    if (length >= sizeof upper) {
        return 0;
    }
    for (size_t index = 0; index < length; index++) {
        upper[index] = (char)toupper((unsigned char)name[index]);
    }
    upper[length] = 0;
    for (size_t index = 0; index < sizeof encodings / sizeof encodings[0]; index++) {
        if (strcmp(encodings[index].name, upper) == 0) {
            result->kind = encodings[index].kind;
            result->code_page = encodings[index].code_page;
            return result->kind != ENCODING_CODE_PAGE || IsValidCodePage(result->code_page);
        }
    }
    if (strncmp(upper, "ISO-8859-", 9) == 0) {
        result->kind = ENCODING_CODE_PAGE;
        result->code_page = 28590 + (UINT)atoi(upper + 9);
        return IsValidCodePage(result->code_page);
    }
    return 0;
}

iconv_t iconv_open(const char *to, const char *from) {
    struct ce_iconv *converter = calloc(1, sizeof *converter);
    if (!converter) {
        errno = ENOMEM;
        return (iconv_t)-1;
    }
    if (!find_encoding(from, &converter->from) || !find_encoding(to, &converter->to)) {
        free(converter);
        errno = EINVAL;
        return (iconv_t)-1;
    }
    return converter;
}

int iconv_close(iconv_t converter) {
    free(converter);
    return 0;
}

static int decode(struct ce_iconv *converter, const uint8_t *input, size_t available, uint32_t *code) {
    switch (converter->from.kind) {
    case ENCODING_UTF8: {
        uint8_t lead = input[0];
        int length = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : -1;
        if (length < 0) {
            return -EILSEQ;
        }
        if ((size_t)length > available) {
            return -EINVAL;
        }
        uint32_t value = length == 1 ? lead : lead & (0xFF >> (length + 1));
        for (int index = 1; index < length; index++) {
            if ((input[index] & 0xC0) != 0x80) {
                return -EILSEQ;
            }
            value = (value << 6) | (input[index] & 0x3F);
        }
        *code = value;
        return length;
    }
    case ENCODING_UTF16:
    case ENCODING_UTF16LE:
    case ENCODING_UTF16BE: {
        if (available < 2) {
            return -EINVAL;
        }
        int little = converter->from.kind == ENCODING_UTF16LE;
        uint32_t first = little ? input[0] | input[1] << 8 : input[0] << 8 | input[1];
        if (converter->from.kind == ENCODING_UTF16 && (first == 0xFEFF || first == 0xFFFE)) {
            converter->from.kind = first == 0xFEFF ? ENCODING_UTF16BE : ENCODING_UTF16LE;
            *code = 0xFEFF;
            return 2;
        }
        if (first >= 0xD800 && first < 0xDC00) {
            if (available < 4) {
                return -EINVAL;
            }
            uint32_t second = little ? input[2] | input[3] << 8 : input[2] << 8 | input[3];
            *code = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
            return 4;
        }
        *code = first;
        return 2;
    }
    case ENCODING_UCS4LE:
    case ENCODING_UCS4BE:
        if (available < 4) {
            return -EINVAL;
        }
        *code = converter->from.kind == ENCODING_UCS4LE
                    ? (uint32_t)input[0] | (uint32_t)input[1] << 8 | (uint32_t)input[2] << 16 | (uint32_t)input[3] << 24
                    : (uint32_t)input[0] << 24 | (uint32_t)input[1] << 16 | (uint32_t)input[2] << 8 | (uint32_t)input[3];
        return 4;
    case ENCODING_LATIN1:
    case ENCODING_ASCII:
        *code = input[0];
        return 1;
    case ENCODING_CODE_PAGE: {
        int length = IsDBCSLeadByteEx(converter->from.code_page, input[0]) ? 2 : 1;
        WCHAR wide[2];
        if ((size_t)length > available) {
            return -EINVAL;
        }
        if (MultiByteToWideChar(converter->from.code_page, 0, (LPCSTR)input, length, wide, 2) != 1) {
            return -EILSEQ;
        }
        *code = (uint16_t)wide[0];
        return length;
    }
    }
    return -EILSEQ;
}

static int encode(struct ce_iconv *converter, uint32_t code, uint8_t *output, size_t space) {
    switch (converter->to.kind) {
    case ENCODING_UTF8: {
        int length = code < 0x80 ? 1 : code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
        if ((size_t)length > space) {
            return -E2BIG;
        }
        if (length == 1) {
            output[0] = (uint8_t)code;
            return 1;
        }
        for (int index = length - 1; index > 0; index--) {
            output[index] = (uint8_t)(0x80 | (code & 0x3F));
            code >>= 6;
        }
        output[0] = (uint8_t)((0xF00 >> length) | code);
        return length;
    }
    case ENCODING_UTF16:
    case ENCODING_UTF16LE:
    case ENCODING_UTF16BE: {
        int little = converter->to.kind == ENCODING_UTF16LE;
        uint16_t units[2];
        int count = 1;
        if (code >= 0x10000) {
            code -= 0x10000;
            units[0] = (uint16_t)(0xD800 | (code >> 10));
            units[1] = (uint16_t)(0xDC00 | (code & 0x3FF));
            count = 2;
        } else {
            units[0] = (uint16_t)code;
        }
        if ((size_t)count * 2 > space) {
            return -E2BIG;
        }
        for (int index = 0; index < count; index++) {
            output[index * 2] = (uint8_t)(little ? units[index] : units[index] >> 8);
            output[index * 2 + 1] = (uint8_t)(little ? units[index] >> 8 : units[index]);
        }
        return count * 2;
    }
    case ENCODING_UCS4LE:
    case ENCODING_UCS4BE:
        if (space < 4) {
            return -E2BIG;
        }
        for (int index = 0; index < 4; index++) {
            int shift = converter->to.kind == ENCODING_UCS4LE ? index * 8 : (3 - index) * 8;
            output[index] = (uint8_t)(code >> shift);
        }
        return 4;
    case ENCODING_LATIN1:
    case ENCODING_ASCII:
        if (code > (converter->to.kind == ENCODING_ASCII ? 0x7Fu : 0xFFu)) {
            return -EILSEQ;
        }
        if (!space) {
            return -E2BIG;
        }
        output[0] = (uint8_t)code;
        return 1;
    case ENCODING_CODE_PAGE: {
        WCHAR wide = (WCHAR)code;
        char bytes[4];
        BOOL lossy = FALSE;
        if (code >= 0x10000) {
            return -EILSEQ;
        }
        int length = WideCharToMultiByte(converter->to.code_page, 0, &wide, 1, bytes, sizeof bytes, NULL, &lossy);
        if (length <= 0 || lossy) {
            return -EILSEQ;
        }
        if ((size_t)length > space) {
            return -E2BIG;
        }
        memcpy(output, bytes, (size_t)length);
        return length;
    }
    }
    return -EILSEQ;
}

size_t iconv(iconv_t converter, char **input, size_t *input_left, char **output, size_t *output_left) {
    if (!input || !*input) {
        return 0;
    }
    size_t irreversible = 0;
    while (*input_left) {
        uint32_t code;
        int consumed = decode(converter, (const uint8_t *)*input, *input_left, &code);
        if (consumed < 0) {
            errno = -consumed;
            return (size_t)-1;
        }
        int produced = encode(converter, code, (uint8_t *)*output, *output_left);
        if (produced < 0) {
            errno = -produced;
            return (size_t)-1;
        }
        *input += consumed;
        *input_left -= (size_t)consumed;
        *output += produced;
        *output_left -= (size_t)produced;
    }
    return irreversible;
}
