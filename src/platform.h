#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
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

typedef enum Platform_Serial_Read_Result {
    PLATFORM_SERIAL_READ_NONE,
    PLATFORM_SERIAL_READ_BYTE,
    PLATFORM_SERIAL_READ_ERROR,
} Platform_Serial_Read_Result;

typedef struct Platform_Serial_Port Platform_Serial_Port;

bool platform_output_init(void);
bool platform_init(Handle_Input_Fn handler, void *userdata);
void platform_run(void);
void platform_shutdown(void);
bool platform_send_text_utf8(const char *utf8);
bool platform_delete_text_utf8(const char *utf8);
bool platform_keycode_from_name(const char *name, uint16_t *out_keycode);
bool platform_find_serial_device(char *out_path, size_t out_size);
bool platform_find_gemini_pr_device(char *out_path, size_t out_size);
bool platform_serial_open(Platform_Serial_Port **out_port, const char *port_path, int baud_rate);
void platform_serial_close(Platform_Serial_Port *port);
const char *platform_serial_port_path(const Platform_Serial_Port *port);
bool platform_serial_had_error(const Platform_Serial_Port *port);
Platform_Serial_Read_Result platform_serial_read_byte(
    Platform_Serial_Port *port,
    uint8_t *out_byte,
    unsigned int timeout_ms
);
bool platform_user_session_is_active(void);
void platform_sleep_ms(unsigned int milliseconds);

#endif
