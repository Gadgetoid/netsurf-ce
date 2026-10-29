#include <windows.h>

#include <string.h>

#include "display.h"

#define INPUT_QUEUE_SIZE 128
#define WM_NETSURF_WAKE (WM_APP + 1)

struct rgb565_bitmap_info {
    BITMAPINFOHEADER header;
    DWORD masks[3];
};

static HWND window;
static HDC bitmap_context;
static HBITMAP bitmap;
static HBITMAP previous_bitmap;
static struct ce_input queue[INPUT_QUEUE_SIZE];
static unsigned queue_head;
static unsigned queue_tail;
static bool pointer_down;
static uint32_t control_letter_sent;
volatile int ce_display_waiting;

static void push_input(const struct ce_input *input) {
    unsigned next = (queue_tail + 1) % INPUT_QUEUE_SIZE;
    if (next == queue_head) {
        return;
    }
    queue[queue_tail] = *input;
    queue_tail = next;
}

static void push_pointer(enum ce_input_type type, LPARAM position) {
    struct ce_input input = { .type = type, .x = (short)LOWORD(position), .y = (short)HIWORD(position) };
    push_input(&input);
}

static enum ce_key key_from_virtual_key(WPARAM virtual_key) {
    switch (virtual_key) {
    case VK_BACK:
        return CE_KEY_BACKSPACE;
    case VK_TAB:
        return CE_KEY_TAB;
    case VK_RETURN:
        return CE_KEY_RETURN;
    case VK_ESCAPE:
        return CE_KEY_ESCAPE;
    case VK_DELETE:
        return CE_KEY_DELETE;
    case VK_UP:
        return CE_KEY_UP;
    case VK_DOWN:
        return CE_KEY_DOWN;
    case VK_LEFT:
        return CE_KEY_LEFT;
    case VK_RIGHT:
        return CE_KEY_RIGHT;
    case VK_HOME:
        return CE_KEY_HOME;
    case VK_END:
        return CE_KEY_END;
    case VK_PRIOR:
        return CE_KEY_PAGE_UP;
    case VK_NEXT:
        return CE_KEY_PAGE_DOWN;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return CE_KEY_CONTROL;
    default:
        return CE_KEY_OTHER;
    }
}

static bool control_held(void) {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

static void push_control_letter(uint32_t letter) {
    struct ce_input input = { .type = CE_INPUT_CONTROL_LETTER, .character = letter };
    push_input(&input);
    control_letter_sent = letter;
}

static void push_key(enum ce_input_type type, WPARAM virtual_key) {
    struct ce_input input = { .type = type, .key = key_from_virtual_key(virtual_key) };
    if (type == CE_INPUT_KEY_DOWN) {
        control_letter_sent = 0;
        if (virtual_key >= 'A' && virtual_key <= 'Z' && control_held()) {
            push_control_letter((uint32_t)(virtual_key - 'A' + 'a'));
            return;
        }
    }
    if (input.key == CE_KEY_OTHER) {
        return;
    }
    push_input(&input);
}

static LRESULT CALLBACK window_procedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_LBUTTONDOWN:
        SetCapture(handle);
        pointer_down = true;
        push_pointer(CE_INPUT_POINTER_DOWN, lparam);
        return 0;
    case WM_MOUSEMOVE:
        if (pointer_down) {
            push_pointer(CE_INPUT_POINTER_MOVE, lparam);
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        pointer_down = false;
        push_pointer(CE_INPUT_POINTER_UP, lparam);
        return 0;
    case WM_KEYDOWN:
        push_key(CE_INPUT_KEY_DOWN, wparam);
        return 0;
    case WM_KEYUP:
        push_key(CE_INPUT_KEY_UP, wparam);
        return 0;
    case WM_CHAR:
        if (wparam >= 1 && wparam <= 26 && control_held()) {
            uint32_t letter = (uint32_t)(wparam - 1 + 'a');
            if (control_letter_sent != letter) {
                push_control_letter(letter);
            }
            control_letter_sent = 0;
        } else if (wparam >= 0x20 && wparam != 0x7F && !control_held()) {
            struct ce_input input = { .type = CE_INPUT_CHARACTER, .character = (uint32_t)wparam };
            push_input(&input);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC context = BeginPaint(handle, &paint);
        BitBlt(context, paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left,
               paint.rcPaint.bottom - paint.rcPaint.top, bitmap_context, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
        EndPaint(handle, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_NETSURF_WAKE:
        return 0;
    case WM_CLOSE: {
        struct ce_input input = { .type = CE_INPUT_QUIT };
        push_input(&input);
        return 0;
    }
    }
    return DefWindowProcW(handle, message, wparam, lparam);
}

bool ce_display_open(struct ce_display *display) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW window_class;
    memset(&window_class, 0, sizeof window_class);
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.lpszClassName = L"NetSurf";
    if (!RegisterClassW(&window_class)) {
        return false;
    }
    RECT area;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0)) {
        SetRect(&area, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }
    display->width = area.right - area.left;
    display->height = area.bottom - area.top;
    display->stride = ((display->width * 2) + 3) & ~3;

    struct rgb565_bitmap_info info;
    memset(&info, 0, sizeof info);
    info.header.biSize = sizeof info.header;
    info.header.biWidth = display->width;
    info.header.biHeight = -display->height;
    info.header.biPlanes = 1;
    info.header.biBitCount = 16;
    info.header.biCompression = BI_BITFIELDS;
    info.masks[0] = 0xF800;
    info.masks[1] = 0x07E0;
    info.masks[2] = 0x001F;

    HDC screen = GetDC(NULL);
    void *pixels = NULL;
    bitmap = CreateDIBSection(screen, (BITMAPINFO *)&info, DIB_RGB_COLORS, &pixels, NULL, 0);
    bitmap_context = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!bitmap || !bitmap_context || !pixels) {
        return false;
    }
    previous_bitmap = SelectObject(bitmap_context, bitmap);
    display->pixels = pixels;

    window = CreateWindowExW(0, L"NetSurf", L"NetSurf", WS_POPUP | WS_VISIBLE, area.left, area.top, display->width, display->height,
                             NULL, NULL, instance, NULL);
    if (!window) {
        return false;
    }
    HICON small_icon = LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, 0);
    if (small_icon) {
        SendMessageW(window, WM_SETICON, ICON_SMALL, (LPARAM)small_icon);
    }
    SetForegroundWindow(window);
    return true;
}

void ce_display_close(void) {
    if (window) {
        DestroyWindow(window);
        window = NULL;
    }
    if (bitmap_context) {
        SelectObject(bitmap_context, previous_bitmap);
        DeleteDC(bitmap_context);
        bitmap_context = NULL;
    }
    if (bitmap) {
        DeleteObject(bitmap);
        bitmap = NULL;
    }
}

void ce_display_present(int x0, int y0, int x1, int y1) {
    HDC context = GetDC(window);
    BitBlt(context, x0, y0, x1 - x0, y1 - y0, bitmap_context, x0, y0, SRCCOPY);
    ReleaseDC(window, context);
}

static void pump_messages(void) {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool ce_display_next_input(struct ce_input *input, int timeout_ms) {
    if (queue_head == queue_tail) {
        pump_messages();
    }
    if (queue_head == queue_tail && timeout_ms != 0) {
        ce_display_waiting = 1;
        MsgWaitForMultipleObjectsEx(0, NULL, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms, QS_ALLINPUT, 0);
        ce_display_waiting = 0;
        pump_messages();
    }
    if (queue_head == queue_tail) {
        return false;
    }
    *input = queue[queue_head];
    queue_head = (queue_head + 1) % INPUT_QUEUE_SIZE;
    return true;
}

void ce_display_wake(void) {
    if (window) {
        PostMessageW(window, WM_NETSURF_WAKE, 0, 0);
    }
}
