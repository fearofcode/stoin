#ifndef STOIN_PLATFORM_H
#define STOIN_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Stoin_Input_Event {
    uint16_t keycode;
    bool is_down;
    bool is_repeat;
    bool shift;
    bool control;
    bool option;
    bool command;
    char printable;
} Stoin_Input_Event;

// Return true to consume/drop the input event, false to pass it through.
typedef bool (*Stoin_Handle_Input_Fn)(const Stoin_Input_Event *event, void *userdata);

bool stoin_platform_init(Stoin_Handle_Input_Fn handler, void *userdata);
void stoin_platform_run(void);
void stoin_platform_shutdown(void);
bool stoin_platform_send_text_utf8(const char *utf8);
bool stoin_platform_keycode_from_name(const char *name, uint16_t *out_keycode);

#endif
