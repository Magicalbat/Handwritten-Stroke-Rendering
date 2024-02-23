#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__

#include "base/base.h"
#include "gfx/gfx.h"
#include "gfx/opengl/opengl.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

typedef struct _gfx_win_backend {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
    b32 new_scroll;
} _gfx_win_backend;

static EM_BOOL on_mouse_event(int event_type, const EmscriptenMouseEvent* e, void* win_ptr);
static EM_BOOL on_wheel_event(int event_type, const EmscriptenWheelEvent* e, void* win_ptr);
// Right now, touch events are translated to mouse events
static EM_BOOL on_touch_event(int event_type, const EmscriptenTouchEvent* e, void* win_ptr);
static EM_BOOL on_key_event(int event_type, const EmscriptenKeyboardEvent* e, void* win_ptr);
static EM_BOOL on_ui_event(int event_type, const EmscriptenUiEvent *e, void *win_ptr);

#define CANVAS_ID "wasm_canvas"

gfx_window* gfx_win_create(mg_arena* arena, u32 width, u32 height, string8 title) {
    UNUSED(title);

    gfx_window* win = MGA_PUSH_ZERO_STRUCT(arena, gfx_window);
    win->backend = MGA_PUSH_ZERO_STRUCT(arena, _gfx_win_backend);

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = true;
    attr.depth = 0;
    attr.stencil = 0;
    attr.explicitSwapControl = true;
    attr.renderViaOffscreenBackBuffer = true;

    win->backend->ctx = emscripten_webgl_create_context("#" CANVAS_ID, &attr);

    win->width = width;
    win->height = height;
    emscripten_set_canvas_element_size("#" CANVAS_ID, width, height);
    //emscripten_get_canvas_element_size("#" CANVAS_ID, (int*)&win->width, (int*)&win->height);

    emscripten_set_mousemove_callback("#" CANVAS_ID, win, true, on_mouse_event);
    emscripten_set_mousedown_callback("#" CANVAS_ID, win, true, on_mouse_event);
    emscripten_set_mouseup_callback("#" CANVAS_ID, win, true, on_mouse_event);

    emscripten_set_touchstart_callback("#" CANVAS_ID, win, true, on_touch_event);
    emscripten_set_touchmove_callback("#" CANVAS_ID, win, true, on_touch_event);
    emscripten_set_touchend_callback("#" CANVAS_ID, win, true, on_touch_event);
    emscripten_set_touchcancel_callback("#" CANVAS_ID, win, true, on_touch_event);

    emscripten_set_wheel_callback("#" CANVAS_ID, win, true, on_wheel_event);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, win, true, on_key_event);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, win, true, on_key_event);

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, win, true, on_ui_event);

    return win;
}
void gfx_win_make_current(gfx_window* win) {
    emscripten_webgl_make_context_current(win->backend->ctx);
    
    glViewport(0, 0, win->width, win->height);
}
void gfx_win_destroy(gfx_window* win) {
    emscripten_webgl_destroy_context(win->backend->ctx);
}

void gfx_win_clear(gfx_window* win) {
    UNUSED(win);

    glClear(GL_COLOR_BUFFER_BIT);
}
void gfx_win_swap_buffers(gfx_window* win) {
    UNUSED(win);
    
    emscripten_webgl_commit_frame();
}
void gfx_win_process_events(gfx_window* win) {
    memcpy(win->prev_mouse_buttons, win->mouse_buttons, GFX_NUM_MOUSE_BUTTONS);
    memcpy(win->prev_keys, win->keys, GFX_NUM_KEYS);

    if (!win->backend->new_scroll) {
        win->mouse_scroll = 0;
    }
    win->backend->new_scroll = 0;
}

void gfx_win_set_size(gfx_window* win, u32 width, u32 height) {
    win->width = width;
    win->height = height;
    
    emscripten_set_canvas_element_size("#canvas", width, height);
    glViewport(0, 0, win->width, win->height);
}

static EM_BOOL on_mouse_event(int event_type, const EmscriptenMouseEvent* e, void* win_ptr) {
    gfx_window* win = (gfx_window*)win_ptr;

    switch (event_type) { 
        case EMSCRIPTEN_EVENT_MOUSEDOWN: {
            win->mouse_buttons[e->button] = true;
        } break;
        case EMSCRIPTEN_EVENT_MOUSEUP: {
            win->mouse_buttons[e->button] = false;
        } break;
        case EMSCRIPTEN_EVENT_MOUSEMOVE: {
            win->mouse_pos.x = (f32)e->targetX;
            win->mouse_pos.y = (f32)e->targetY;
        } break;
        default: break;
    }

    return true;
}

static EM_BOOL on_wheel_event(int event_type, const EmscriptenWheelEvent* e, void* win_ptr) {
    UNUSED(event_type);

    gfx_window* win = (gfx_window*)win_ptr;

    win->mouse_scroll = -SIGN(e->deltaY);
    win->backend->new_scroll = true;

    return true;
}

static EM_BOOL on_touch_event(int event_type, const EmscriptenTouchEvent* e, void* win_ptr) {
    gfx_window* win = (gfx_window*)win_ptr;

    switch (event_type) {
        case EMSCRIPTEN_EVENT_TOUCHSTART: {
            win->mouse_buttons[GFX_MB_LEFT] = true;
        } // fallthrough
        case EMSCRIPTEN_EVENT_TOUCHMOVE: {
            win->mouse_buttons[GFX_MB_LEFT] = true;
            win->mouse_pos.x = (f32)e->touches[0].targetX;
            win->mouse_pos.y = (f32)e->touches[0].targetY;
        } break;
        case EMSCRIPTEN_EVENT_TOUCHEND: // fallthrough
        case EMSCRIPTEN_EVENT_TOUCHCANCEL:{
            win->mouse_buttons[GFX_MB_LEFT] = false;
        } break;
    }

    return true;
}

static gfx_key em_translate_keycode(const char* code_str);

static EM_BOOL on_key_event(int event_type, const EmscriptenKeyboardEvent* e, void* win_ptr) {
    gfx_window* win = (gfx_window*)win_ptr;

    switch(event_type) {
        case EMSCRIPTEN_EVENT_KEYDOWN: {
            gfx_key keydown = em_translate_keycode(e->code);
            win->keys[keydown] = true;
        } break;
        case EMSCRIPTEN_EVENT_KEYUP: {
            gfx_key keyup = em_translate_keycode(e->code);
            win->keys[keyup] = false;
        } break;
    }

    return true;
}

static EM_BOOL on_ui_event(int event_type, const EmscriptenUiEvent *e, void *win_ptr) {
    gfx_window* win = (gfx_window*)win_ptr;

    switch (event_type) {
        case EMSCRIPTEN_EVENT_RESIZE: {
            gfx_win_set_size(win, e->windowInnerWidth, e->windowInnerHeight);
        } break;
        default: break;
    }
    
    return true;
}

// Keycode translation adapted from sokol_app.h
// https://github.com/floooh/sokol/blob/master/sokol_app.h#L5218

// Also, it is really annoying to have to use strings,
// but keycodes are deprecated

static struct {
    const char* str;
    gfx_key code;
} em_keymap[] = {
    { "Backspace",      GFX_KEY_BACKSPACE },
    { "Tab",            GFX_KEY_TAB },
    { "Enter",          GFX_KEY_ENTER },
    { "ShiftLeft",      GFX_KEY_LSHIFT },
    { "ShiftRight",     GFX_KEY_RSHIFT },
    { "ControlLeft",    GFX_KEY_LCONTROL },
    { "ControlRight",   GFX_KEY_RCONTROL },
    { "AltLeft",        GFX_KEY_LALT },
    { "AltRight",       GFX_KEY_RALT },
    { "CapsLock",       GFX_KEY_CAPSLOCK },
    { "Escape",         GFX_KEY_ESCAPE },
    { "Space",          GFX_KEY_SPACE },
    { "PageUp",         GFX_KEY_PAGEUP },
    { "PageDown",       GFX_KEY_PAGEDOWN },
    { "End",            GFX_KEY_END },
    { "Home",           GFX_KEY_HOME },
    { "ArrowLeft",      GFX_KEY_LEFT },
    { "ArrowUp",        GFX_KEY_UP },
    { "ArrowRight",     GFX_KEY_RIGHT },
    { "ArrowDown",      GFX_KEY_DOWN },
    { "Insert",         GFX_KEY_INSERT },
    { "Delete",         GFX_KEY_DELETE },
    { "Digit0",         GFX_KEY_0 },
    { "Digit1",         GFX_KEY_1 },
    { "Digit2",         GFX_KEY_2 },
    { "Digit3",         GFX_KEY_3 },
    { "Digit4",         GFX_KEY_4 },
    { "Digit5",         GFX_KEY_5 },
    { "Digit6",         GFX_KEY_6 },
    { "Digit7",         GFX_KEY_7 },
    { "Digit8",         GFX_KEY_8 },
    { "Digit9",         GFX_KEY_9 },
    { "KeyA",           GFX_KEY_A },
    { "KeyB",           GFX_KEY_B },
    { "KeyC",           GFX_KEY_C },
    { "KeyD",           GFX_KEY_D },
    { "KeyE",           GFX_KEY_E },
    { "KeyF",           GFX_KEY_F },
    { "KeyG",           GFX_KEY_G },
    { "KeyH",           GFX_KEY_H },
    { "KeyI",           GFX_KEY_I },
    { "KeyJ",           GFX_KEY_J },
    { "KeyK",           GFX_KEY_K },
    { "KeyL",           GFX_KEY_L },
    { "KeyM",           GFX_KEY_M },
    { "KeyN",           GFX_KEY_N },
    { "KeyO",           GFX_KEY_O },
    { "KeyP",           GFX_KEY_P },
    { "KeyQ",           GFX_KEY_Q },
    { "KeyR",           GFX_KEY_R },
    { "KeyS",           GFX_KEY_S },
    { "KeyT",           GFX_KEY_T },
    { "KeyU",           GFX_KEY_U },
    { "KeyV",           GFX_KEY_V },
    { "KeyW",           GFX_KEY_W },
    { "KeyX",           GFX_KEY_X },
    { "KeyY",           GFX_KEY_Y },
    { "KeyZ",           GFX_KEY_Z },
    { "Numpad0",        GFX_KEY_NUMPAD_0 },
    { "Numpad1",        GFX_KEY_NUMPAD_1 },
    { "Numpad2",        GFX_KEY_NUMPAD_2 },
    { "Numpad3",        GFX_KEY_NUMPAD_3 },
    { "Numpad4",        GFX_KEY_NUMPAD_4 },
    { "Numpad5",        GFX_KEY_NUMPAD_5 },
    { "Numpad6",        GFX_KEY_NUMPAD_6 },
    { "Numpad7",        GFX_KEY_NUMPAD_7 },
    { "Numpad8",        GFX_KEY_NUMPAD_8 },
    { "Numpad9",        GFX_KEY_NUMPAD_9 },
    { "NumpadMultiply", GFX_KEY_NUMPAD_MULTIPLY },
    { "NumpadAdd",      GFX_KEY_NUMPAD_ADD },
    { "NumpadSubtract", GFX_KEY_NUMPAD_SUBTRACT },
    { "NumpadDecimal",  GFX_KEY_NUMPAD_DECIMAL },
    { "NumpadDivide",   GFX_KEY_NUMPAD_DIVIDE },
    { "F1",             GFX_KEY_F1 },
    { "F2",             GFX_KEY_F2 },
    { "F3",             GFX_KEY_F3 },
    { "F4",             GFX_KEY_F4 },
    { "F5",             GFX_KEY_F5 },
    { "F6",             GFX_KEY_F6 },
    { "F7",             GFX_KEY_F7 },
    { "F8",             GFX_KEY_F8 },
    { "F9",             GFX_KEY_F9 },
    { "F10",            GFX_KEY_F10 },
    { "F11",            GFX_KEY_F11 },
    { "F12",            GFX_KEY_F12 },
    { "NumLock",        GFX_KEY_NUM_LOCK },
    { "ScrollLock",     GFX_KEY_SCROLL_LOCK },
    { "Semicolon",      GFX_KEY_SEMICOLON },
    { "Equal",          GFX_KEY_EQUAL },
    { "Comma",          GFX_KEY_COMMA },
    { "Minus",          GFX_KEY_MINUS },
    { "Period",         GFX_KEY_PERIOD },
    { "Slash",          GFX_KEY_FORWARDSLASH },
    { "Backquote",      GFX_KEY_BACKTICK },
    { "BracketLeft",    GFX_KEY_LBRACKET },
    { "Backslash",      GFX_KEY_BACKSLASH },
    { "BracketRight",   GFX_KEY_RBRACKET },
    { "Quote",          GFX_KEY_APOSTROPHE },
    { 0, GFX_KEY_NONE },
};

static gfx_key em_translate_keycode(const char* code_str) {
    u32 i = 0;
    const char* key_str;
    while ((key_str = em_keymap[i].str)) {
        if (strcmp(key_str, code_str) == 0) {
            return em_keymap[i].code;
        }
        i++;
    }

    return GFX_KEY_NONE;
}

#endif // __EMSCRIPTEN__
