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

typedef struct Platform_File_Stamp {
    uint64_t modified_time_ns;
    uint64_t size;
    bool exists;
} Platform_File_Stamp;

#define PLATFORM_SERIAL_PATH_MAX 256

typedef void (*Platform_File_Watch_Fn)(void *userdata);

typedef struct Platform_Serial_Port Platform_Serial_Port;

typedef enum Platform_Pedal_Role {
    PLATFORM_PEDAL_ROLE_NONE,
    PLATFORM_PEDAL_ROLE_PHRASE_CORE,
    PLATFORM_PEDAL_ROLE_PHRASE_NONVERB,
    PLATFORM_PEDAL_ROLE_COUNT,
} Platform_Pedal_Role;

typedef void (*Platform_Pedal_Event_Fn)(Platform_Pedal_Role role, bool is_down, void *userdata);

bool platform_output_init(void);
bool platform_init(Handle_Input_Fn handler, void *userdata);
void platform_run(void);
void platform_shutdown(void);
bool platform_send_text_utf8(const char *utf8);
bool platform_delete_text_utf8(const char *utf8);
bool platform_send_key_combination(const char *combo);
bool platform_keycode_from_name(const char *name, uint16_t *out_keycode);
bool platform_find_serial_device(char *out_path, size_t out_size);
size_t platform_find_serial_devices(char out_paths[][PLATFORM_SERIAL_PATH_MAX], size_t max_paths);
bool platform_find_gemini_pr_device(char *out_path, size_t out_size);
bool platform_serial_open(Platform_Serial_Port **out_port, const char *port_path, int baud_rate);
void platform_serial_close(Platform_Serial_Port *port);
const char *platform_serial_port_path(const Platform_Serial_Port *port);
bool platform_serial_had_error(const Platform_Serial_Port *port);
void platform_serial_flush(Platform_Serial_Port *port);
Platform_Serial_Read_Result platform_serial_read_byte(
    Platform_Serial_Port *port,
    uint8_t *out_byte,
    unsigned int timeout_ms
);
bool platform_serial_write_all(
    Platform_Serial_Port *port,
    const uint8_t *bytes,
    size_t byte_count,
    unsigned int timeout_ms
);
bool platform_user_session_is_active(void);
bool platform_file_stamp(const char *path, Platform_File_Stamp *out_stamp);
bool platform_file_watcher_start(const char *const *paths, size_t path_count, Platform_File_Watch_Fn callback, void *userdata);
void platform_file_watcher_poll(void);
void platform_file_watcher_stop(void);
void platform_translation_timing_set_enabled(bool enabled);
void platform_translation_timing_begin(uint64_t start_ns);
void platform_translation_timing_cancel(void);
uint64_t platform_monotonic_ns(void);
uint64_t platform_monotonic_ms(void);
void platform_sleep_ms(unsigned int milliseconds);
bool platform_pedals_init(
    const char *config_path,
    Platform_Pedal_Role register_role,
    Platform_Pedal_Event_Fn handler,
    void *userdata
);
void platform_pedals_poll(void);
void platform_pedals_shutdown(void);

#endif
