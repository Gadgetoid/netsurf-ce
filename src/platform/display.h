#ifndef NETSURF_CE_PLATFORM_DISPLAY_H
#define NETSURF_CE_PLATFORM_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

enum ce_input_type {
    CE_INPUT_KEY_DOWN,
    CE_INPUT_KEY_UP,
    CE_INPUT_CHARACTER,
    CE_INPUT_CONTROL_LETTER,
    CE_INPUT_POINTER_DOWN,
    CE_INPUT_POINTER_MOVE,
    CE_INPUT_POINTER_UP,
    CE_INPUT_QUIT,
};

enum ce_key {
    CE_KEY_OTHER,
    CE_KEY_BACKSPACE,
    CE_KEY_TAB,
    CE_KEY_RETURN,
    CE_KEY_ESCAPE,
    CE_KEY_DELETE,
    CE_KEY_UP,
    CE_KEY_DOWN,
    CE_KEY_LEFT,
    CE_KEY_RIGHT,
    CE_KEY_HOME,
    CE_KEY_END,
    CE_KEY_PAGE_UP,
    CE_KEY_PAGE_DOWN,
    CE_KEY_CONTROL,
};

struct ce_input {
    enum ce_input_type type;
    enum ce_key key;
    uint32_t character;
    int x;
    int y;
};

struct ce_display {
    int width;
    int height;
    int stride;
    uint8_t *pixels;
};

extern volatile int ce_display_waiting;

bool ce_display_open(struct ce_display *display);
void ce_display_close(void);
void ce_display_present(int x0, int y0, int x1, int y1);
bool ce_display_next_input(struct ce_input *input, int timeout_ms);
void ce_display_wake(void);

#endif
