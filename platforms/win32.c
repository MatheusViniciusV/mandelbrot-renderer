#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"

static b32 global_running = 1;
static OffscreenBuffer global_backbuffer;
static BITMAPINFO global_bitmap_info;

void Win32ResizeDIBSection(OffscreenBuffer *buffer, int width, int height)
{
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytes_per_pixel = 4;
    buffer->pitch = width * buffer->bytes_per_pixel;

    global_bitmap_info.bmiHeader.biSize = sizeof(global_bitmap_info.bmiHeader);
    global_bitmap_info.bmiHeader.biWidth = width;
    global_bitmap_info.bmiHeader.biHeight = -height; 
    global_bitmap_info.bmiHeader.biPlanes = 1;
    global_bitmap_info.bmiHeader.biBitCount = 32;
    global_bitmap_info.bmiHeader.biCompression = BI_RGB;

    int bitmap_memory_size = (width * height) * buffer->bytes_per_pixel;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void Win32DisplayBufferInWindow(HDC device_context, int window_width, int window_height, OffscreenBuffer *buffer)
{
    StretchDIBits(device_context,
                  0, 0, window_width, window_height,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory,
                  &global_bitmap_info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    LRESULT result = 0;
    switch (message) {
        case WM_CLOSE:   global_running = 0; break;
        case WM_DESTROY: global_running = 0; break;
        case WM_SIZE: {
            RECT client_rect;
            GetClientRect(window, &client_rect);
            int width = client_rect.right - client_rect.left;
            int height = client_rect.bottom - client_rect.top;
            Win32ResizeDIBSection(&global_backbuffer, width, height);
        } break;
        default: result = DefWindowProcA(window, message, w_param, l_param); break;
    }
    return result;
}

void Win32ProcessKeyboard(ButtonState *new_state, b32 is_down)
{
    if (new_state->is_ended_down != is_down)
    {
        new_state->is_ended_down = is_down;
        new_state->half_transition_count++;
    }
}

static KeyId Win32VKToKeyCode(u32 vk)
{
    if (vk >= 'A' && vk <= 'Z') {
        return (KeyId)(KEY_A + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return (KeyId)(KEY_0 + (vk - '0'));
    }
    switch (vk) {
        case VK_LEFT:  return KEY_LEFT;
        case VK_RIGHT: return KEY_RIGHT;
        case VK_UP:    return KEY_UP;
        case VK_DOWN:  return KEY_DOWN;

        case VK_SPACE: return KEY_SPACE;
        case VK_RETURN: return KEY_ENTER;
        case VK_ESCAPE: return KEY_ESC;
        case VK_SHIFT: return KEY_SHIFT;
        case VK_CONTROL: return KEY_CTRL;
        case VK_MENU: return KEY_ALT;
        case VK_TAB: return KEY_TAB;

        case VK_OEM_PLUS: case VK_ADD: return KEY_PLUS;
        case VK_OEM_MINUS: case VK_SUBTRACT: return KEY_MINUS;

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

    if (size > 0)
    {
        result.data = malloc(size);
        if (result.data) {
            size_t read = fread(result.data, 1, size, f);
            result.size = read;
            if (read != (size_t)size)
            {
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
    if (fd && fd->data)
    {
        free(fd->data);
        fd->data = NULL;
        fd->size = 0;
    }
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_code)
{
    (void)prev_instance;
    (void)cmd_line;
    (void)show_code;
    
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = instance;
    window_class.lpszClassName = "WindowClass";

    if (RegisterClassA(&window_class))
    {
        HWND window = CreateWindowExA(0, window_class.lpszClassName, "Protecto Win32",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                                      0, 0, instance, 0);
        if (window)
        {
            Input input = {0};
            ULONGLONG prev_tick = GetTickCount64();
            Win32ResizeDIBSection(&global_backbuffer, 800, 600);
            
            while (global_running)
            {
                MSG message;
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
                    {
                        if (message.message == WM_QUIT) global_running = 0;

                        if (message.message == WM_KEYDOWN || message.message == WM_KEYUP)
                        {
                            u32 vk_code = (u32)message.wParam;
                            b32 was_down = ((message.lParam & (1 << 30)) != 0);
                            b32 is_down = ((message.lParam & (1 << 31)) == 0);

                            if (was_down != is_down) {
                                KeyCode kc = Win32VKToKeyCode(vk_code);
                                if (kc != KEY_COUNT) {
                                    Win32ProcessKeyboard(&input.keys[kc], is_down);
                                }
                            }
                        }
                        else if (message.message == WM_MOUSEMOVE)
                        {
                            int mx = (int)(short)LOWORD(message.lParam);
                            int my = (int)(short)HIWORD(message.lParam);
                            input.mouse_x = mx;
                            input.mouse_y = my;
                        }
                        else if (message.message == WM_LBUTTONDOWN || message.message == WM_LBUTTONUP)
                        {
                            b32 pressed = (message.message == WM_LBUTTONDOWN);
                            Win32ProcessKeyboard(&input.mouse_buttons[0], pressed);
                        }
                        else if (message.message == WM_RBUTTONDOWN || message.message == WM_RBUTTONUP)
                        {
                            b32 pressed = (message.message == WM_RBUTTONDOWN);
                            Win32ProcessKeyboard(&input.mouse_buttons[1], pressed);
                        }
                        else if (message.message == WM_MBUTTONDOWN || message.message == WM_MBUTTONUP)
                        {
                            b32 pressed = (message.message == WM_MBUTTONDOWN);
                            Win32ProcessKeyboard(&input.mouse_buttons[2], pressed);
                        }
                        else if (message.message == WM_MOUSEWHEEL)
                        {
                            int wheel_delta = (short)HIWORD(message.wParam);
                            input.mouse_wheel += (float)wheel_delta / 120.0f;
                        }
                    
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                ULONGLONG current_tick = GetTickCount64();
                input.time_delta = (float)(current_tick - prev_tick) / 1000.0f;
                prev_tick = current_tick;

                UpdateAndRender(&input, &global_backbuffer);

                HDC device_context = GetDC(window);
                RECT client_rect;
                GetClientRect(window, &client_rect);
                int window_width = client_rect.right - client_rect.left;
                int window_height = client_rect.bottom - client_rect.top;
                
                Win32DisplayBufferInWindow(device_context, window_width, window_height, &global_backbuffer);
                ReleaseDC(window, device_context);
            }
        }
    }
    
    return 0;
}