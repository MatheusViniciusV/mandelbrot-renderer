#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "platform.h"

static b32 global_running = 1;
static OffscreenBuffer global_backbuffer;
static XImage *global_ximage = NULL;

static void LinuxResizeBuffer(Display *display, int width, int height)
{
    if (global_ximage) {
        XDestroyImage(global_ximage);
        global_ximage = NULL;
    }

    if (global_backbuffer.memory) {
        free(global_backbuffer.memory);
        global_backbuffer.memory = NULL;
    }

    global_backbuffer.width = width;
    global_backbuffer.height = height;
    global_backbuffer.bytes_per_pixel = 4;
    global_backbuffer.pitch = width * global_backbuffer.bytes_per_pixel;

    int size = width * height * global_backbuffer.bytes_per_pixel;
    global_backbuffer.memory = malloc(size);
    if (global_backbuffer.memory) memset(global_backbuffer.memory, 0, size);

    int screen = DefaultScreen(display);
    int depth = DefaultDepth(display, screen);

    global_ximage = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0,
                                    (char *)global_backbuffer.memory, width, height, 32, 0);
}

static void LinuxProcessKeyboard(ButtonState *new_state, b32 is_down)
{
    if (new_state->is_ended_down != is_down) {
        new_state->is_ended_down = is_down;
        new_state->half_transition_count++;
    }
}

static KeyId LinuxKeysymToKeyCode(KeySym keysym)
{
    switch (keysym) {
        case XK_Up:    return KEY_UP;
        case XK_Down:  return KEY_DOWN;
        case XK_Left:  return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        case XK_a: case XK_A: return KEY_A;
        case XK_b: case XK_B: return KEY_B;
        case XK_c: case XK_C: return KEY_C;
        case XK_d: case XK_D: return KEY_D;
        case XK_e: case XK_E: return KEY_E;
        case XK_f: case XK_F: return KEY_F;
        case XK_g: case XK_G: return KEY_G;
        case XK_h: case XK_H: return KEY_H;
        case XK_i: case XK_I: return KEY_I;
        case XK_j: case XK_J: return KEY_J;
        case XK_k: case XK_K: return KEY_K;
        case XK_l: case XK_L: return KEY_L;
        case XK_m: case XK_M: return KEY_M;
        case XK_n: case XK_N: return KEY_N;
        case XK_o: case XK_O: return KEY_O;
        case XK_p: case XK_P: return KEY_P;
        case XK_q: case XK_Q: return KEY_Q;
        case XK_r: case XK_R: return KEY_R;
        case XK_s: case XK_S: return KEY_S;
        case XK_t: case XK_T: return KEY_T;
        case XK_u: case XK_U: return KEY_U;
        case XK_v: case XK_V: return KEY_V;
        case XK_w: case XK_W: return KEY_W;
        case XK_x: case XK_X: return KEY_X;
        case XK_y: case XK_Y: return KEY_Y;
        case XK_z: case XK_Z: return KEY_Z;
        case XK_0: return KEY_0;
        case XK_1: return KEY_1;
        case XK_2: return KEY_2;
        case XK_3: return KEY_3;
        case XK_4: return KEY_4;
        case XK_5: return KEY_5;
        case XK_6: return KEY_6;
        case XK_7: return KEY_7;
        case XK_8: return KEY_8;
        case XK_9: return KEY_9;
        case XK_space:  return KEY_SPACE;
        case XK_Return: return KEY_ENTER;
        case XK_Escape: return KEY_ESC;
        case XK_Shift_L: case XK_Shift_R: return KEY_SHIFT;
        case XK_Control_L: case XK_Control_R: return KEY_CTRL;
        case XK_Alt_L: case XK_Alt_R: return KEY_ALT;
        case XK_Tab: return KEY_TAB;
        case XK_plus: case XK_equal: return KEY_PLUS;
        case XK_minus: case XK_underscore: return KEY_MINUS;
        default: return KEY_COUNT;
    }
}

FileData PlatformFileRead(const char *path)
{
    FileData result = {0};
    if (!path) return result;

    FILE *f = fopen(path, "rb");
    if (!f) return result;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > 0) {
        result.data = malloc(size);
        if (result.data) {
            size_t read = fread(result.data, 1, size, f);
            result.size = read;
            if (read != (size_t)size) {
                free(result.data);
                result.data = NULL;
                result.size = 0;
            }
        }
    }

    fclose(f);
    return result;
}

void PlatformFileFree(FileData *fd)
{
    if (fd && fd->data) {
        free(fd->data);
        fd->data = NULL;
        fd->size = 0;
    }
}

int main(void)
{
    Display *display = XOpenDisplay(NULL);
    if (!display) return 1;

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        10, 10, 800, 600, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen));

    XSelectInput(display, window, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask
                 | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(display, window);

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    Input input = {0};
    input.mouse_wheel = 0.0f;
    LinuxResizeBuffer(display, 800, 600);

    struct timespec prev_time;
    clock_gettime(CLOCK_MONOTONIC, &prev_time);

    while (global_running) {
        input.mouse_wheel = 0.0f;
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);

            if (event.type == ClientMessage)
            {
                if ((Atom)event.xclient.data.l[0] == wm_delete_window) global_running = 0;
            }
            else if (event.type == ConfigureNotify)
            {
                XConfigureEvent ce = event.xconfigure;
                if (ce.width != global_backbuffer.width || ce.height != global_backbuffer.height) {
                    LinuxResizeBuffer(display, ce.width, ce.height);
                }
            }
            else if (event.type == KeyPress || event.type == KeyRelease)
            {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                b32 is_down = (event.type == KeyPress);
                KeyCode kc = LinuxKeysymToKeyCode(keysym);
                if (kc != KEY_COUNT) LinuxProcessKeyboard(&input.keys[kc], is_down);
            }
            else if (event.type == MotionNotify)
            {
                XMotionEvent *me = &event.xmotion;
                input.mouse_x = me->x;
                input.mouse_y = me->y;
            } else if (event.type == ButtonPress || event.type == ButtonRelease)
            {
                XButtonEvent *be = &event.xbutton;
                input.mouse_x = be->x;
                input.mouse_y = be->y;
                b32 pressed = (event.type == ButtonPress);
                if (be->button == Button1) LinuxProcessKeyboard(&input.mouse_buttons[0], pressed);
                else if (be->button == Button2) LinuxProcessKeyboard(&input.mouse_buttons[1], pressed);
                else if (be->button == Button3) LinuxProcessKeyboard(&input.mouse_buttons[2], pressed);
                else if (be->button == Button4 && pressed) input.mouse_wheel += 1.0f;
                else if (be->button == Button5 && pressed) input.mouse_wheel -= 1.0f;
            }
        }

        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long dt_ns = (current_time.tv_sec - prev_time.tv_sec) * 1000000000LL +
                     (current_time.tv_nsec - prev_time.tv_nsec);
        input.time_delta = (float)dt_ns / 1000000000.0f;
        prev_time = current_time;

        UpdateAndRender(&input, &global_backbuffer);

        if (global_ximage) {
            XPutImage(display, window, DefaultGC(display, screen), global_ximage,
                        0, 0, 0, 0, global_backbuffer.width, global_backbuffer.height);
        }
    }

    XCloseDisplay(display);
    return 0;
}