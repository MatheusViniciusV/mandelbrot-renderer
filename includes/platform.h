#ifndef PROTECTO_H
#define PROTECTO_H

#include <stdint.h>
#include <stddef.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;
typedef int b32;

typedef struct {
    void *memory;
    i32 width;
    i32 height;
    i32 pitch;
    i32 bytes_per_pixel;
} OffscreenBuffer;

typedef struct {
    int is_ended_down;
    int half_transition_count;
} ButtonState;

typedef enum {
    KEY_UNKNOWN = -1,
    
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,

    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,

    KEY_SPACE, KEY_ENTER, KEY_ESC, KEY_SHIFT, KEY_CTRL, KEY_ALT, KEY_TAB,
    KEY_PLUS, KEY_MINUS,
    KEY_COUNT
} KeyId;

typedef struct {
    ButtonState keys[KEY_COUNT];
    i32 mouse_x;
    i32 mouse_y;
    ButtonState mouse_buttons[3];
    f32 time_delta;
    f32 mouse_wheel;
} Input;

typedef struct {
    void *data;
    size_t size;
} FileData;

FileData PlatformFileRead(const char *path);
void PlatformFileFree(FileData *fd);
void UpdateAndRender(Input *input, OffscreenBuffer *buffer);

#endif
