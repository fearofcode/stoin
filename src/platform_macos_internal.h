#ifndef PLATFORM_MACOS_INTERNAL_H
#define PLATFORM_MACOS_INTERNAL_H

#include "platform.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>

#define STOIN_GENERATED_EVENT_USER_DATA 0x73746f696eULL

typedef struct Mac_File_Watch_Target {
    int fd;
    char *path;
} Mac_File_Watch_Target;

typedef struct Mac_State {
    CFMachPortRef tap;
    CFRunLoopSourceRef run_loop_source;
    CFRunLoopRef run_loop;
    CFFileDescriptorRef file_watcher_descriptor;
    CFRunLoopSourceRef file_watcher_source;
    CFRunLoopRef file_watcher_run_loop;
    CGEventSourceRef output_source;
    Handle_Input_Fn handler;
    void *userdata;
    Platform_File_Watch_Fn file_watcher_callback;
    void *file_watcher_userdata;
    char **file_watcher_paths;
    Mac_File_Watch_Target *file_watcher_targets;
    int file_watcher_kq;
    int screen_lock_notify_token;
    bool file_watcher_active;
    bool screen_lock_notify_registered;
} Mac_State;

extern Mac_State g_macos;

void macos_mark_generated_event(CGEventRef event);
bool macos_event_was_generated_by_stoin(CGEventRef event);

#endif
