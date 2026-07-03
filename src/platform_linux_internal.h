#ifndef PLATFORM_LINUX_INTERNAL_H
#define PLATFORM_LINUX_INTERNAL_H

#include "platform.h"

#include <stdbool.h>
#include <stdint.h>

#define STOIN_LINUX_UINPUT_NAME "stoin virtual keyboard"

typedef struct Linux_Keyboard_Device {
    int fd;
    char *path;
    char name[256];
} Linux_Keyboard_Device;

typedef struct Linux_File_Watch_Target {
    int wd;
    char *path;
} Linux_File_Watch_Target;

typedef struct Linux_State {
    Handle_Input_Fn handler;
    void *userdata;
    Platform_File_Watch_Fn file_watcher_callback;
    void *file_watcher_userdata;
    Linux_Keyboard_Device *keyboards;
    char **file_watcher_paths;
    Linux_File_Watch_Target *file_watcher_targets;
    int uinput_fd;
    int file_watcher_fd;
    uint64_t translation_timing_start_ns;
    uint64_t translation_timing_sequence;
    bool running;
    bool shift_down;
    bool control_down;
    bool option_down;
    bool command_down;
    bool file_watcher_active;
    bool translation_timing_enabled;
    bool translation_timing_active;
} Linux_State;

extern Linux_State g_linux;

bool linux_evdev_keycode_from_logical(uint16_t logical_keycode, unsigned int *out_evdev_keycode);
bool linux_logical_keycode_from_evdev(unsigned int evdev_keycode, uint16_t *out_logical_keycode);
char linux_printable_from_logical(uint16_t logical_keycode);

bool linux_uinput_emit_key_event(unsigned int evdev_keycode, int value);
bool linux_uinput_tap_key(unsigned int evdev_keycode);
void linux_output_shutdown(void);
void linux_report_translation_timing_before_output(const char *operation);

int linux_file_watcher_fd(void);

#endif
