#include <stddef.h>
#include <stdint.h>

#define WORD_SIZE sizeof(uint32_t)
#define WORD_MASK (WORD_SIZE - 1)

static void copy_forward(unsigned char *output, const unsigned char *input, size_t count) {
    if ((((uintptr_t)output ^ (uintptr_t)input) & WORD_MASK) == 0) {
        while (count && ((uintptr_t)output & WORD_MASK)) {
            *output++ = *input++;
            count--;
        }
        uint32_t *output_words = (uint32_t *)output;
        const uint32_t *input_words = (const uint32_t *)input;
        while (count >= 4 * WORD_SIZE) {
            uint32_t first = input_words[0];
            uint32_t second = input_words[1];
            uint32_t third = input_words[2];
            uint32_t fourth = input_words[3];
            output_words[0] = first;
            output_words[1] = second;
            output_words[2] = third;
            output_words[3] = fourth;
            output_words += 4;
            input_words += 4;
            count -= 4 * WORD_SIZE;
        }
        while (count >= WORD_SIZE) {
            *output_words++ = *input_words++;
            count -= WORD_SIZE;
        }
        output = (unsigned char *)output_words;
        input = (const unsigned char *)input_words;
    }
    while (count--) {
        *output++ = *input++;
    }
}

static void copy_backward(unsigned char *output, const unsigned char *input, size_t count) {
    output += count;
    input += count;
    if ((((uintptr_t)output ^ (uintptr_t)input) & WORD_MASK) == 0) {
        while (count && ((uintptr_t)output & WORD_MASK)) {
            *--output = *--input;
            count--;
        }
        uint32_t *output_words = (uint32_t *)output;
        const uint32_t *input_words = (const uint32_t *)input;
        while (count >= WORD_SIZE) {
            *--output_words = *--input_words;
            count -= WORD_SIZE;
        }
        output = (unsigned char *)output_words;
        input = (const unsigned char *)input_words;
    }
    while (count--) {
        *--output = *--input;
    }
}

void *memcpy(void *destination, const void *source, size_t count) {
    copy_forward(destination, source, count);
    return destination;
}

void *memmove(void *destination, const void *source, size_t count) {
    unsigned char *output = destination;
    const unsigned char *input = source;
    if (output <= input || output >= input + count) {
        copy_forward(output, input, count);
    } else {
        copy_backward(output, input, count);
    }
    return destination;
}

void *memset(void *destination, int value, size_t count) {
    unsigned char *output = destination;
    unsigned char byte = (unsigned char)value;
    while (count && ((uintptr_t)output & WORD_MASK)) {
        *output++ = byte;
        count--;
    }
    uint32_t word = byte * 0x01010101u;
    uint32_t *output_words = (uint32_t *)output;
    while (count >= 4 * WORD_SIZE) {
        output_words[0] = word;
        output_words[1] = word;
        output_words[2] = word;
        output_words[3] = word;
        output_words += 4;
        count -= 4 * WORD_SIZE;
    }
    while (count >= WORD_SIZE) {
        *output_words++ = word;
        count -= WORD_SIZE;
    }
    output = (unsigned char *)output_words;
    while (count--) {
        *output++ = byte;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t count) {
    const unsigned char *first = left;
    const unsigned char *second = right;
    if ((((uintptr_t)first | (uintptr_t)second) & WORD_MASK) == 0) {
        while (count >= WORD_SIZE && *(const uint32_t *)first == *(const uint32_t *)second) {
            first += WORD_SIZE;
            second += WORD_SIZE;
            count -= WORD_SIZE;
        }
    }
    for (; count; count--, first++, second++) {
        if (*first != *second) {
            return *first - *second;
        }
    }
    return 0;
}
