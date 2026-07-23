#include "platform_windows_internal.h"

#include "json_util.h"
#include "util.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/stb_ds.h"

#define WINDOWS_PEDAL_CLASS_NAME "StoinPedalRawInputWindow"
#define WINDOWS_PEDAL_MAX_RAW_INPUT_SIZE 8192
#define HID_PAGE_KEYBOARD 0x07

typedef struct Windows_Pedal_Binding {
    char *device_name;
    uint8_t *report;
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t usage_page;
    uint32_t usage;
    uint32_t scan_code;
    uint32_t vk;
    uint32_t report_size;
    bool valid;
    bool is_down;
} Windows_Pedal_Binding;

typedef struct Windows_Pedal_State {
    HWND pedal_window;
    Windows_Pedal_Binding pedal_bindings[PLATFORM_PEDAL_ROLE_COUNT];
    const char *pedal_config_path;
    Platform_Pedal_Event_Fn pedal_handler;
    void *pedal_userdata;
    Platform_Pedal_Role pedal_register_role;
    bool pedals_started;
    bool pedal_registering;
    bool pedal_window_class_registered;
} Windows_Pedal_State;

static Windows_Pedal_State g_windows;

static char *copy_nullable_cstring(const char *s)
{
    return s == NULL ? NULL : copy_cstring(s);
}

static const char *pedal_role_name(Platform_Pedal_Role role)
{
    switch (role) {
    case PLATFORM_PEDAL_ROLE_INITIAL_VERB:
        return "initial_verb";
    case PLATFORM_PEDAL_ROLE_FINAL_VERB:
        return "final_verb";
    case PLATFORM_PEDAL_ROLE_NONVERB:
        return "nonverb";
    case PLATFORM_PEDAL_ROLE_NONE:
    case PLATFORM_PEDAL_ROLE_COUNT:
    default:
        return "none";
    }
}

static const char *pedal_role_legacy_name(Platform_Pedal_Role role)
{
    switch (role) {
    case PLATFORM_PEDAL_ROLE_INITIAL_VERB:
        return "phrase_core";
    case PLATFORM_PEDAL_ROLE_NONVERB:
        return "phrase_nonverb";
    case PLATFORM_PEDAL_ROLE_NONE:
    case PLATFORM_PEDAL_ROLE_FINAL_VERB:
    case PLATFORM_PEDAL_ROLE_COUNT:
    default:
        return NULL;
    }
}

static const char *pedal_role_label(Platform_Pedal_Role role)
{
    switch (role) {
    case PLATFORM_PEDAL_ROLE_INITIAL_VERB:
        return "initial verb";
    case PLATFORM_PEDAL_ROLE_FINAL_VERB:
        return "final verb";
    case PLATFORM_PEDAL_ROLE_NONVERB:
        return "non-verb phrase";
    case PLATFORM_PEDAL_ROLE_NONE:
    case PLATFORM_PEDAL_ROLE_COUNT:
    default:
        return "unassigned";
    }
}

static bool pedal_role_is_bindable(Platform_Pedal_Role role)
{
    return role > PLATFORM_PEDAL_ROLE_NONE && role < PLATFORM_PEDAL_ROLE_COUNT;
}

static void free_pedal_binding(Windows_Pedal_Binding *binding)
{
    if (binding == NULL) {
        return;
    }
    free(binding->device_name);
    free(binding->report);
    memset(binding, 0, sizeof(*binding));
}

static void clear_pedal_bindings(void)
{
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        free_pedal_binding(&g_windows.pedal_bindings[role]);
    }
}

static int hex_digit_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool decode_hex_bytes(const char *hex, uint8_t **out_bytes, uint32_t *out_size)
{
    if (hex == NULL || out_bytes == NULL || out_size == NULL) {
        return false;
    }

    const size_t hex_length = strlen(hex);
    if (hex_length == 0 || (hex_length % 2) != 0 || hex_length / 2 > UINT32_MAX) {
        return false;
    }

    uint8_t *bytes = malloc(hex_length / 2);
    if (bytes == NULL) {
        return false;
    }

    for (size_t i = 0; i < hex_length; i += 2) {
        const int high = hex_digit_value(hex[i]);
        const int low = hex_digit_value(hex[i + 1]);
        if (high < 0 || low < 0) {
            free(bytes);
            return false;
        }
        bytes[i / 2] = (uint8_t)((high << 4) | low);
    }

    *out_bytes = bytes;
    *out_size = (uint32_t)(hex_length / 2);
    return true;
}

static void write_hex_bytes(FILE *file, const uint8_t *bytes, uint32_t size)
{
    static const char HEX[] = "0123456789abcdef";
    fputc('"', file);
    for (uint32_t i = 0; i < size; ++i) {
        fputc(HEX[(bytes[i] >> 4) & 0x0f], file);
        fputc(HEX[bytes[i] & 0x0f], file);
    }
    fputc('"', file);
}

static bool load_pedal_binding_from_json(
    const char *json,
    Platform_Pedal_Role role,
    Windows_Pedal_Binding *binding
)
{
    const char *end = NULL;
    const char *start = json_find_object(json, pedal_role_name(role), &end);
    if (start == NULL) {
        const char *legacy_name = pedal_role_legacy_name(role);
        if (legacy_name != NULL) {
            start = json_find_object(json, legacy_name, &end);
        }
    }
    if (start == NULL) {
        return false;
    }

    Windows_Pedal_Binding loaded;
    memset(&loaded, 0, sizeof(loaded));
    if (!json_parse_uint_field(start, end, "usage_page", &loaded.usage_page)
        || !json_parse_uint_field(start, end, "usage", &loaded.usage)
        || !json_parse_string_field(start, end, "device_name", &loaded.device_name)) {
        fprintf(stderr,
            "stoin: warning: ignoring incomplete %s pedal binding in %s\n",
            pedal_role_label(role),
            g_windows.pedal_config_path);
        free_pedal_binding(&loaded);
        return false;
    }

    (void)json_parse_uint_field(start, end, "vendor_id", &loaded.vendor_id);
    (void)json_parse_uint_field(start, end, "product_id", &loaded.product_id);
    (void)json_parse_uint_field(start, end, "scan_code", &loaded.scan_code);
    (void)json_parse_uint_field(start, end, "vk", &loaded.vk);

    char *report_hex = NULL;
    if (json_parse_string_field(start, end, "raw_report", &report_hex)) {
        if (!decode_hex_bytes(report_hex, &loaded.report, &loaded.report_size)) {
            fprintf(stderr,
                "stoin: warning: ignoring invalid %s pedal raw report in %s\n",
                pedal_role_label(role),
                g_windows.pedal_config_path);
            free(report_hex);
            free_pedal_binding(&loaded);
            return false;
        }
        free(report_hex);
    } else if (loaded.scan_code == 0 || loaded.vk == 0) {
        fprintf(stderr,
            "stoin: warning: ignoring incomplete %s pedal binding in %s\n",
            pedal_role_label(role),
            g_windows.pedal_config_path);
        free_pedal_binding(&loaded);
        return false;
    }

    loaded.valid = true;
    *binding = loaded;
    return true;
}

static bool load_pedal_config(const char *path)
{
    size_t size = 0;
    char *json = read_entire_file(path, &size);
    (void)size;
    if (json == NULL) {
        return false;
    }

    bool loaded_any = false;
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Windows_Pedal_Binding binding;
        memset(&binding, 0, sizeof(binding));
        if (load_pedal_binding_from_json(json, role, &binding)) {
            free_pedal_binding(&g_windows.pedal_bindings[role]);
            g_windows.pedal_bindings[role] = binding;
            loaded_any = true;
        }
    }

    free(json);
    if (loaded_any) {
        printf("stoin: loaded pedal config from %s\n", path);
    }
    return loaded_any;
}

static bool save_pedal_config(const char *path)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"version\": 1");
    bool wrote_binding = false;
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        const Windows_Pedal_Binding *binding = &g_windows.pedal_bindings[role];
        if (!binding->valid) {
            continue;
        }

        fprintf(file, ",\n");
        fprintf(file, "  \"%s\": {\n", pedal_role_name(role));
        fprintf(file, "    \"backend\": \"windows_raw_input\",\n");
        fprintf(file, "    \"device_name\": ");
        json_write_string(file, binding->device_name);
        fprintf(file, ",\n");
        fprintf(file, "    \"vendor_id\": %u,\n", binding->vendor_id);
        fprintf(file, "    \"product_id\": %u,\n", binding->product_id);
        fprintf(file, "    \"usage_page\": %u,\n", binding->usage_page);
        fprintf(file, "    \"usage\": %u", binding->usage);
        if (binding->report != NULL && binding->report_size > 0) {
            fprintf(file, ",\n");
            fprintf(file, "    \"raw_report\": ");
            write_hex_bytes(file, binding->report, binding->report_size);
            fputc('\n', file);
        } else {
            fprintf(file, ",\n");
            fprintf(file, "    \"scan_code\": %u,\n", binding->scan_code);
            fprintf(file, "    \"vk\": %u\n", binding->vk);
        }
        fprintf(file, "  }");
        wrote_binding = true;
    }
    fprintf(file, "\n}\n");

    if (fclose(file) != 0) {
        return false;
    }

    if (wrote_binding) {
        printf("stoin: saved pedal config to %s\n", path);
    }
    return true;
}

static void print_pedal_binding(const Windows_Pedal_Binding *binding, Platform_Pedal_Role role)
{
    printf("stoin: pedal %s = vid=0x%04x pid=0x%04x page=0x%02x usage=0x%02x",
        pedal_role_label(role),
        binding->vendor_id,
        binding->product_id,
        binding->usage_page,
        binding->usage);
    if (binding->report != NULL && binding->report_size > 0) {
        printf(" report_bytes=%u", binding->report_size);
    } else {
        printf(" scan=0x%04x vk=0x%02x", binding->scan_code, binding->vk);
    }
    printf(" device=");
    json_write_string(stdout, binding->device_name);
    fputc('\n', stdout);
}

static bool hex_u32_after_marker(const char *text, const char *marker, uint32_t *out_value)
{
    const char *found = strstr(text, marker);
    if (found == NULL || out_value == NULL) {
        return false;
    }

    found += strlen(marker);
    char *end = NULL;
    const unsigned long parsed = strtoul(found, &end, 16);
    if (end == found || parsed > UINT32_MAX) {
        return false;
    }

    *out_value = (uint32_t)parsed;
    return true;
}

static void parse_vid_pid_from_device_name(
    const char *device_name,
    uint32_t *out_vendor_id,
    uint32_t *out_product_id
)
{
    if (out_vendor_id != NULL) {
        *out_vendor_id = 0;
    }
    if (out_product_id != NULL) {
        *out_product_id = 0;
    }
    if (device_name == NULL) {
        return;
    }

    char *upper = copy_nullable_cstring(device_name);
    if (upper == NULL) {
        return;
    }
    for (char *p = upper; *p != '\0'; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }

    if (out_vendor_id != NULL) {
        (void)hex_u32_after_marker(upper, "VID_", out_vendor_id);
    }
    if (out_product_id != NULL) {
        (void)hex_u32_after_marker(upper, "PID_", out_product_id);
    }
    free(upper);
}

static char *raw_input_device_name(HANDLE device)
{
    UINT size = 0;
    if (GetRawInputDeviceInfoA(device, RIDI_DEVICENAME, NULL, &size) != 0 || size == 0) {
        return NULL;
    }

    char *name = calloc((size_t)size + 1, sizeof(*name));
    if (name == NULL) {
        return NULL;
    }

    if (GetRawInputDeviceInfoA(device, RIDI_DEVICENAME, name, &size) == (UINT)-1) {
        free(name);
        return NULL;
    }
    return name;
}

static bool raw_input_hid_usage(HANDLE device, uint32_t *out_usage_page, uint32_t *out_usage)
{
    RID_DEVICE_INFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    UINT size = sizeof(info);
    if (GetRawInputDeviceInfoA(device, RIDI_DEVICEINFO, &info, &size) == (UINT)-1
        || info.dwType != RIM_TYPEHID) {
        return false;
    }
    if (out_usage_page != NULL) {
        *out_usage_page = info.hid.usUsagePage;
    }
    if (out_usage != NULL) {
        *out_usage = info.hid.usUsage;
    }
    return true;
}

static UINT normalize_raw_keyboard_vk(const RAWKEYBOARD *keyboard)
{
    if (keyboard == NULL) {
        return 0;
    }

    switch (keyboard->VKey) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    {
        UINT scan_code = keyboard->MakeCode;
        if ((keyboard->Flags & RI_KEY_E0) != 0) {
            scan_code |= 0xE000;
        }
        return MapVirtualKeyA(scan_code, MAPVK_VSC_TO_VK_EX);
    }
    default:
        return keyboard->VKey;
    }
}

static uint32_t normalized_raw_scan_code(const RAWKEYBOARD *keyboard)
{
    if (keyboard == NULL) {
        return 0;
    }

    uint32_t scan_code = keyboard->MakeCode;
    if ((keyboard->Flags & RI_KEY_E0) != 0) {
        scan_code |= 0xE000;
    }
    return scan_code;
}

static bool binding_matches_device(
    const Windows_Pedal_Binding *binding,
    const char *device_name,
    uint32_t vendor_id,
    uint32_t product_id
)
{
    if (binding == NULL || !binding->valid) {
        return false;
    }
    if (binding->device_name != NULL && device_name != NULL && strcmp(binding->device_name, device_name) == 0) {
        return true;
    }
    return binding->vendor_id != 0
        && binding->product_id != 0
        && binding->vendor_id == vendor_id
        && binding->product_id == product_id;
}

static bool binding_matches_pedal_event(
    const Windows_Pedal_Binding *binding,
    const char *device_name,
    uint32_t vendor_id,
    uint32_t product_id,
    uint32_t scan_code,
    uint32_t vk
)
{
    if (binding == NULL
        || !binding->valid
        || binding->report != NULL
        || binding->scan_code != scan_code
        || binding->vk != vk) {
        return false;
    }
    return binding_matches_device(binding, device_name, vendor_id, product_id);
}

static Platform_Pedal_Role pedal_role_for_event(
    const char *device_name,
    uint32_t vendor_id,
    uint32_t product_id,
    uint32_t scan_code,
    uint32_t vk
)
{
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        if (binding_matches_pedal_event(
                &g_windows.pedal_bindings[role],
                device_name,
                vendor_id,
                product_id,
                scan_code,
                vk)) {
            return role;
        }
    }
    return PLATFORM_PEDAL_ROLE_NONE;
}

static bool raw_hid_report_has_signal(const uint8_t *report, uint32_t report_size)
{
    if (report == NULL || report_size == 0) {
        return false;
    }
    for (uint32_t i = 0; i < report_size; ++i) {
        if (report[i] != 0) {
            return true;
        }
    }
    return false;
}

static void learn_pedal_binding(
    const char *device_name,
    uint32_t vendor_id,
    uint32_t product_id,
    uint32_t scan_code,
    uint32_t vk
)
{
    if (!pedal_role_is_bindable(g_windows.pedal_register_role)) {
        return;
    }

    Windows_Pedal_Binding binding;
    memset(&binding, 0, sizeof(binding));
    binding.device_name = copy_nullable_cstring(device_name);
    if (binding.device_name == NULL) {
        fputs("stoin: warning: failed to copy Windows pedal device name\n", stderr);
        return;
    }
    binding.vendor_id = vendor_id;
    binding.product_id = product_id;
    binding.usage_page = HID_PAGE_KEYBOARD;
    binding.usage = vk;
    binding.scan_code = scan_code;
    binding.vk = vk;
    binding.valid = true;

    free_pedal_binding(&g_windows.pedal_bindings[g_windows.pedal_register_role]);
    g_windows.pedal_bindings[g_windows.pedal_register_role] = binding;

    printf("stoin: learned %s pedal from Windows Raw Input device\n",
        pedal_role_label(g_windows.pedal_register_role));
    print_pedal_binding(&binding, g_windows.pedal_register_role);
    if (!save_pedal_config(g_windows.pedal_config_path)) {
        fprintf(stderr, "stoin: warning: failed to save pedal config to %s\n", g_windows.pedal_config_path);
    }

    g_windows.pedal_registering = false;
}

static void learn_pedal_hid_binding(
    const char *device_name,
    uint32_t vendor_id,
    uint32_t product_id,
    uint32_t usage_page,
    uint32_t usage,
    const uint8_t *report,
    uint32_t report_size
)
{
    if (!pedal_role_is_bindable(g_windows.pedal_register_role)
        || !raw_hid_report_has_signal(report, report_size)) {
        return;
    }

    Windows_Pedal_Binding binding;
    memset(&binding, 0, sizeof(binding));
    binding.device_name = copy_nullable_cstring(device_name);
    binding.report = malloc(report_size);
    if (binding.device_name == NULL || binding.report == NULL) {
        free_pedal_binding(&binding);
        fputs("stoin: warning: failed to copy Windows HID pedal binding\n", stderr);
        return;
    }

    memcpy(binding.report, report, report_size);
    binding.report_size = report_size;
    binding.vendor_id = vendor_id;
    binding.product_id = product_id;
    binding.usage_page = usage_page;
    binding.usage = usage;
    binding.valid = true;

    free_pedal_binding(&g_windows.pedal_bindings[g_windows.pedal_register_role]);
    g_windows.pedal_bindings[g_windows.pedal_register_role] = binding;

    printf("stoin: learned %s pedal from Windows Raw Input HID report\n",
        pedal_role_label(g_windows.pedal_register_role));
    print_pedal_binding(&binding, g_windows.pedal_register_role);
    if (!save_pedal_config(g_windows.pedal_config_path)) {
        fprintf(stderr, "stoin: warning: failed to save pedal config to %s\n", g_windows.pedal_config_path);
    }

    g_windows.pedal_registering = false;
}

static void handle_pedal_raw_keyboard(const RAWINPUT *raw)
{
    if (raw == NULL || raw->header.dwType != RIM_TYPEKEYBOARD) {
        return;
    }

    const RAWKEYBOARD *keyboard = &raw->data.keyboard;
    const uint32_t vk = normalize_raw_keyboard_vk(keyboard);
    const uint32_t scan_code = normalized_raw_scan_code(keyboard);
    if (vk == 0 || scan_code == 0) {
        return;
    }

    const bool pressed = (keyboard->Flags & RI_KEY_BREAK) == 0;
    char *device_name = raw_input_device_name(raw->header.hDevice);
    if (device_name == NULL) {
        return;
    }

    uint32_t vendor_id = 0;
    uint32_t product_id = 0;
    parse_vid_pid_from_device_name(device_name, &vendor_id, &product_id);

    if (g_windows.pedal_registering) {
        if (pressed) {
            learn_pedal_binding(device_name, vendor_id, product_id, scan_code, vk);
        }
        free(device_name);
        return;
    }

    const Platform_Pedal_Role role =
        pedal_role_for_event(device_name, vendor_id, product_id, scan_code, vk);
    free(device_name);

    if (role == PLATFORM_PEDAL_ROLE_NONE || g_windows.pedal_handler == NULL) {
        return;
    }

    Windows_Pedal_Binding *binding = &g_windows.pedal_bindings[role];
    if (binding->is_down == pressed) {
        return;
    }
    binding->is_down = pressed;
    g_windows.pedal_handler(role, pressed, g_windows.pedal_userdata);
}

static void handle_pedal_raw_hid(const RAWINPUT *raw)
{
    if (raw == NULL || raw->header.dwType != RIM_TYPEHID) {
        return;
    }

    const RAWHID *hid = &raw->data.hid;
    if (hid->dwSizeHid == 0 || hid->dwCount == 0) {
        return;
    }

    char *device_name = raw_input_device_name(raw->header.hDevice);
    if (device_name == NULL) {
        return;
    }

    uint32_t vendor_id = 0;
    uint32_t product_id = 0;
    uint32_t usage_page = 0;
    uint32_t usage = 0;
    parse_vid_pid_from_device_name(device_name, &vendor_id, &product_id);
    if (!raw_input_hid_usage(raw->header.hDevice, &usage_page, &usage)) {
        free(device_name);
        return;
    }

    const uint8_t *reports = hid->bRawData;
    for (DWORD i = 0; i < hid->dwCount; ++i) {
        const uint8_t *report = reports + (i * hid->dwSizeHid);
        const uint32_t report_size = hid->dwSizeHid;

        if (g_windows.pedal_registering) {
            learn_pedal_hid_binding(
                device_name,
                vendor_id,
                product_id,
                usage_page,
                usage,
                report,
                report_size);
            if (!g_windows.pedal_registering) {
                break;
            }
            continue;
        }

        for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
             role < PLATFORM_PEDAL_ROLE_COUNT;
             ++role) {
            Windows_Pedal_Binding *binding = &g_windows.pedal_bindings[role];
            if (binding->report == NULL
                || binding->report_size != report_size
                || binding->usage_page != usage_page
                || binding->usage != usage
                || !binding_matches_device(binding, device_name, vendor_id, product_id)) {
                continue;
            }

            const bool pressed = memcmp(binding->report, report, report_size) == 0;
            if (binding->is_down == pressed) {
                continue;
            }
            binding->is_down = pressed;
            if (g_windows.pedal_handler != NULL) {
                g_windows.pedal_handler(role, pressed, g_windows.pedal_userdata);
            }
        }
    }

    free(device_name);
}

static LRESULT CALLBACK pedal_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)hwnd;
    (void)wparam;

    if (message == WM_INPUT) {
        UINT size = 0;
        if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER)) != 0
            || size == 0
            || size > WINDOWS_PEDAL_MAX_RAW_INPUT_SIZE) {
            return 0;
        }

        BYTE *buffer = malloc(size);
        if (buffer == NULL) {
            return 0;
        }

        if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size) {
            const RAWINPUT *raw = (const RAWINPUT *)buffer;
            if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                handle_pedal_raw_keyboard(raw);
            } else if (raw->header.dwType == RIM_TYPEHID) {
                handle_pedal_raw_hid(raw);
            }
        }
        free(buffer);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static bool ensure_pedal_window(void)
{
    if (g_windows.pedal_window != NULL) {
        return true;
    }

    HINSTANCE instance = GetModuleHandleA(NULL);
    if (!g_windows.pedal_window_class_registered) {
        WNDCLASSA window_class;
        memset(&window_class, 0, sizeof(window_class));
        window_class.lpfnWndProc = pedal_window_proc;
        window_class.hInstance = instance;
        window_class.lpszClassName = WINDOWS_PEDAL_CLASS_NAME;
        if (RegisterClassA(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        g_windows.pedal_window_class_registered = true;
    }

    g_windows.pedal_window = CreateWindowExA(
        0,
        WINDOWS_PEDAL_CLASS_NAME,
        "stoin pedals",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        NULL,
        instance,
        NULL
    );
    return g_windows.pedal_window != NULL;
}

static bool register_pedal_raw_input(void)
{
    if (!ensure_pedal_window()) {
        return false;
    }

    RAWINPUTDEVICE *devices = NULL;
    RAWINPUTDEVICE keyboard;
    memset(&keyboard, 0, sizeof(keyboard));
    keyboard.usUsagePage = 0x01;
    keyboard.usUsage = 0x06;
    keyboard.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    keyboard.hwndTarget = g_windows.pedal_window;
    arrput(devices, keyboard);

    UINT device_count = 0;
    if (GetRawInputDeviceList(NULL, &device_count, sizeof(RAWINPUTDEVICELIST)) == 0 && device_count > 0) {
        RAWINPUTDEVICELIST *device_list = calloc(device_count, sizeof(*device_list));
        if (device_list != NULL
            && GetRawInputDeviceList(device_list, &device_count, sizeof(RAWINPUTDEVICELIST)) != (UINT)-1) {
            for (UINT i = 0; i < device_count; ++i) {
                if (device_list[i].dwType != RIM_TYPEHID) {
                    continue;
                }

                uint32_t usage_page = 0;
                uint32_t usage = 0;
                if (!raw_input_hid_usage(device_list[i].hDevice, &usage_page, &usage)
                    || usage_page == 0
                    || usage == 0
                    || (usage_page == 0x01 && usage == 0x06)) {
                    continue;
                }

                bool already_registered = false;
                for (size_t j = 0; j < arrlenu(devices); ++j) {
                    if (devices[j].usUsagePage == (USHORT)usage_page
                        && devices[j].usUsage == (USHORT)usage) {
                        already_registered = true;
                        break;
                    }
                }
                if (already_registered) {
                    continue;
                }

                RAWINPUTDEVICE hid;
                memset(&hid, 0, sizeof(hid));
                hid.usUsagePage = (USHORT)usage_page;
                hid.usUsage = (USHORT)usage;
                hid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
                hid.hwndTarget = g_windows.pedal_window;
                arrput(devices, hid);
            }
        }
        free(device_list);
    }

    const bool ok = arrlenu(devices) > 0
        && RegisterRawInputDevices(devices, (UINT)arrlenu(devices), sizeof(devices[0])) != FALSE;
    arrfree(devices);
    return ok;
}

bool platform_pedals_init(
    const char *config_path,
    Platform_Pedal_Role register_role,
    Platform_Pedal_Event_Fn handler,
    void *userdata
)
{
    platform_pedals_shutdown();

    g_windows.pedal_config_path = config_path != NULL ? config_path : "stoin-pedals.json";
    g_windows.pedal_handler = handler;
    g_windows.pedal_userdata = userdata;
    g_windows.pedal_register_role = register_role;
    g_windows.pedal_registering = pedal_role_is_bindable(register_role);

    const bool loaded = load_pedal_config(g_windows.pedal_config_path);
    if (!loaded && !g_windows.pedal_registering) {
        return true;
    }

    if (!register_pedal_raw_input()) {
        fputs("stoin: warning: failed to register Windows Raw Input for USB pedals\n", stderr);
        platform_pedals_shutdown();
        return false;
    }
    g_windows.pedals_started = true;

    if (g_windows.pedal_registering) {
        printf("stoin: pedal registration armed for %s mode\n", pedal_role_label(register_role));
        printf("stoin: press the pedal to use as %s mode\n", pedal_role_label(register_role));
        puts("stoin: avoid typing or pressing other HID keyboard devices until registration finishes");
        while (g_windows.pedal_registering) {
            platform_pedals_poll();
            platform_sleep_ms(10);
        }
        platform_pedals_shutdown();
        return platform_pedals_init(config_path, PLATFORM_PEDAL_ROLE_NONE, handler, userdata);
    }

    if (loaded) {
        for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
             role < PLATFORM_PEDAL_ROLE_COUNT;
             ++role) {
            if (g_windows.pedal_bindings[role].valid) {
                print_pedal_binding(&g_windows.pedal_bindings[role], role);
            }
        }
    }

    return true;
}

void platform_pedals_poll(void)
{
    if (!g_windows.pedals_started && g_windows.pedal_window == NULL) {
        return;
    }

    MSG message;
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            PostQuitMessage((int)message.wParam);
            break;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

void platform_pedals_shutdown(void)
{
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Windows_Pedal_Binding *binding = &g_windows.pedal_bindings[role];
        if (binding->valid && binding->is_down && g_windows.pedal_handler != NULL) {
            g_windows.pedal_handler(role, false, g_windows.pedal_userdata);
        }
    }

    if (g_windows.pedal_window != NULL) {
        DestroyWindow(g_windows.pedal_window);
        g_windows.pedal_window = NULL;
    }

    clear_pedal_bindings();
    g_windows.pedals_started = false;
    g_windows.pedal_registering = false;
    g_windows.pedal_config_path = NULL;
    g_windows.pedal_handler = NULL;
    g_windows.pedal_userdata = NULL;
    g_windows.pedal_register_role = PLATFORM_PEDAL_ROLE_NONE;
}
