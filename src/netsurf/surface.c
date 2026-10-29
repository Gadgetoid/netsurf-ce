#include <stdbool.h>
#include <stdlib.h>

#include <libnsfb.h>
#include <libnsfb_event.h>
#include <libnsfb_plot.h>

#include "nsfb.h"
#include "plot.h"
#include "surface.h"

#include "platform/display.h"

#define PENDING_EVENT_LIMIT 8

static struct ce_display display;
static nsfb_event_t pending[PENDING_EVENT_LIMIT];
static int pending_count;
static int pending_next;
static bool control_down;

static void queue_event(enum nsfb_event_type_e type, int value) {
    if (pending_count == PENDING_EVENT_LIMIT) {
        return;
    }
    nsfb_event_t *event = &pending[pending_count++];
    event->type = type;
    event->value.keycode = value;
}

static void queue_pointer(int x, int y) {
    if (pending_count == PENDING_EVENT_LIMIT) {
        return;
    }
    nsfb_event_t *event = &pending[pending_count++];
    event->type = NSFB_EVENT_MOVE_ABSOLUTE;
    event->value.vector.x = x;
    event->value.vector.y = y;
    event->value.vector.z = 0;
}

static void queue_key_press(int keycode) {
    queue_event(NSFB_EVENT_KEY_DOWN, keycode);
    queue_event(NSFB_EVENT_KEY_UP, keycode);
}

static int keycode_for(enum ce_key key) {
    switch (key) {
    case CE_KEY_BACKSPACE:
        return NSFB_KEY_BACKSPACE;
    case CE_KEY_TAB:
        return NSFB_KEY_TAB;
    case CE_KEY_RETURN:
        return NSFB_KEY_RETURN;
    case CE_KEY_ESCAPE:
        return NSFB_KEY_ESCAPE;
    case CE_KEY_DELETE:
        return NSFB_KEY_DELETE;
    case CE_KEY_UP:
        return NSFB_KEY_UP;
    case CE_KEY_DOWN:
        return NSFB_KEY_DOWN;
    case CE_KEY_LEFT:
        return NSFB_KEY_LEFT;
    case CE_KEY_RIGHT:
        return NSFB_KEY_RIGHT;
    case CE_KEY_HOME:
        return NSFB_KEY_HOME;
    case CE_KEY_END:
        return NSFB_KEY_END;
    case CE_KEY_PAGE_UP:
        return NSFB_KEY_PAGEUP;
    case CE_KEY_PAGE_DOWN:
        return NSFB_KEY_PAGEDOWN;
    case CE_KEY_CONTROL:
        return NSFB_KEY_LCTRL;
    default:
        return NSFB_KEY_UNKNOWN;
    }
}

static void translate(const struct ce_input *input) {
    switch (input->type) {
    case CE_INPUT_KEY_DOWN:
    case CE_INPUT_KEY_UP:
        if (input->key == CE_KEY_CONTROL) {
            control_down = input->type == CE_INPUT_KEY_DOWN;
        }
        queue_event(input->type == CE_INPUT_KEY_DOWN ? NSFB_EVENT_KEY_DOWN : NSFB_EVENT_KEY_UP,
                    keycode_for(input->key));
        break;
    case CE_INPUT_CHARACTER:
        queue_key_press(NSFB_KEY_UNICODE + (int)input->character);
        break;
    case CE_INPUT_CONTROL_LETTER:
        if (!control_down) {
            queue_event(NSFB_EVENT_KEY_DOWN, NSFB_KEY_LCTRL);
        }
        queue_key_press((int)input->character);
        if (!control_down) {
            queue_event(NSFB_EVENT_KEY_UP, NSFB_KEY_LCTRL);
        }
        break;
    case CE_INPUT_POINTER_DOWN:
        queue_pointer(input->x, input->y);
        queue_event(NSFB_EVENT_KEY_DOWN, NSFB_KEY_MOUSE_1);
        break;
    case CE_INPUT_POINTER_MOVE:
        queue_pointer(input->x, input->y);
        break;
    case CE_INPUT_POINTER_UP:
        queue_pointer(input->x, input->y);
        queue_event(NSFB_EVENT_KEY_UP, NSFB_KEY_MOUSE_1);
        break;
    case CE_INPUT_QUIT:
        pending[pending_count].type = NSFB_EVENT_CONTROL;
        pending[pending_count++].value.controlcode = NSFB_CONTROL_QUIT;
        break;
    }
}

static int ce_defaults(nsfb_t *nsfb) {
    nsfb->width = 0;
    nsfb->height = 0;
    nsfb->format = NSFB_FMT_RGB565;
    select_plotters(nsfb);
    return 0;
}

static int ce_initialise(nsfb_t *nsfb) {
    if (!ce_display_open(&display)) {
        return -1;
    }
    nsfb->width = display.width;
    nsfb->height = display.height;
    nsfb->format = NSFB_FMT_RGB565;
    nsfb->ptr = display.pixels;
    nsfb->linelen = display.stride;
    select_plotters(nsfb);
    return 0;
}

static int ce_finalise(nsfb_t *nsfb) {
    ce_display_close();
    nsfb->ptr = NULL;
    return 0;
}

static int ce_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format) {
    if (nsfb->ptr == NULL) {
        if (width > 0) {
            nsfb->width = width;
        }
        if (height > 0) {
            nsfb->height = height;
        }
    }
    nsfb->format = NSFB_FMT_RGB565;
    select_plotters(nsfb);
    return 0;
}

static bool ce_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout) {
    struct ce_input input;
    while (pending_next == pending_count) {
        pending_next = 0;
        pending_count = 0;
        if (!ce_display_next_input(&input, timeout)) {
            return false;
        }
        translate(&input);
        timeout = 0;
    }
    *event = pending[pending_next++];
    return true;
}

static int ce_claim(nsfb_t *nsfb, nsfb_bbox_t *box) {
    return 0;
}

static int ce_update(nsfb_t *nsfb, nsfb_bbox_t *box) {
    ce_display_present(box->x0, box->y0, box->x1, box->y1);
    return 0;
}

static int ce_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor) {
    return true;
}

static const nsfb_surface_rtns_t ce_rtns = {
    .defaults = ce_defaults,
    .initialise = ce_initialise,
    .finalise = ce_finalise,
    .input = ce_input,
    .claim = ce_claim,
    .update = ce_update,
    .cursor = ce_cursor,
    .geometry = ce_geometry,
};

NSFB_SURFACE_DEF(ce, NSFB_SURFACE_WINCE, &ce_rtns)
