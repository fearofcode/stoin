#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Input_Event {
    uint16_t keycode;
    bool is_down;
    bool is_repeat;
    bool shift;
    bool control;
    bool option;
    bool command;
    char printable;
} Input_Event;

// Return true to consume/drop the input event, false to pass it through.
typedef bool (*Handle_Input_Fn)(const Input_Event *event, void *userdata);

bool platform_output_init(void);
bool platform_init(Handle_Input_Fn handler, void *userdata);
void platform_run(void);
void platform_shutdown(void);
bool platform_send_text_utf8(const char *utf8);
bool platform_delete_text_utf8(const char *utf8);
bool platform_keycode_from_name(const char *name, uint16_t *out_keycode);

#endif
