#include "platform_linux_internal.h"

#include "json_util.h"
#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "../stb_ds.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define BIT_WORD(bit) ((bit) / BITS_PER_LONG)
#define BIT_MASK(bit) (1UL << ((bit) % BITS_PER_LONG))
#define BIT_ARRAY_LENGTH(max_bit) (((max_bit) / BITS_PER_LONG) + 1)

typedef struct Linux_Key_Map {
    const char *name;
    uint16_t logical_keycode;
    unsigned int evdev_keycode;
    char printable;
} Linux_Key_Map;

Linux_State g_linux = {
    .uinput_fd = -1,
    .file_watcher_fd = -1,
};

static const Linux_Key_Map LINUX_KEY_MAP[] = {
    {"a", 0, KEY_A, 'a'},
    {"s", 1, KEY_S, 's'},
    {"d", 2, KEY_D, 'd'},
    {"f", 3, KEY_F, 'f'},
    {"h", 4, KEY_H, 'h'},
    {"g", 5, KEY_G, 'g'},
    {"z", 6, KEY_Z, 'z'},
    {"x", 7, KEY_X, 'x'},
    {"c", 8, KEY_C, 'c'},
    {"v", 9, KEY_V, 'v'},
    {"b", 11, KEY_B, 'b'},
    {"q", 12, KEY_Q, 'q'},
    {"w", 13, KEY_W, 'w'},
    {"e", 14, KEY_E, 'e'},
    {"r", 15, KEY_R, 'r'},
    {"y", 16, KEY_Y, 'y'},
    {"t", 17, KEY_T, 't'},
    {"1", 18, KEY_1, '1'},
    {"2", 19, KEY_2, '2'},
    {"3", 20, KEY_3, '3'},
    {"4", 21, KEY_4, '4'},
    {"6", 22, KEY_6, '6'},
    {"5", 23, KEY_5, '5'},
    {"9", 25, KEY_9, '9'},
    {"7", 26, KEY_7, '7'},
    {"8", 28, KEY_8, '8'},
    {"0", 29, KEY_0, '0'},
    {"]", 30, KEY_RIGHTBRACE, ']'},
    {"o", 31, KEY_O, 'o'},
    {"u", 32, KEY_U, 'u'},
    {"[", 33, KEY_LEFTBRACE, '['},
    {"i", 34, KEY_I, 'i'},
    {"p", 35, KEY_P, 'p'},
    {"l", 37, KEY_L, 'l'},
    {"j", 38, KEY_J, 'j'},
    {"'", 39, KEY_APOSTROPHE, '\''},
    {"apostrophe", 39, KEY_APOSTROPHE, '\''},
    {"quote", 39, KEY_APOSTROPHE, '\''},
    {"k", 40, KEY_K, 'k'},
    {";", 41, KEY_SEMICOLON, ';'},
    {"semicolon", 41, KEY_SEMICOLON, ';'},
    {"\\", 42, KEY_BACKSLASH, '\\'},
    {"backslash", 42, KEY_BACKSLASH, '\\'},
    {",", 43, KEY_COMMA, ','},
    {"comma", 43, KEY_COMMA, ','},
    {"/", 44, KEY_SLASH, '/'},
    {"slash", 44, KEY_SLASH, '/'},
    {"n", 45, KEY_N, 'n'},
    {"m", 46, KEY_M, 'm'},
    {".", 47, KEY_DOT, '.'},
    {"period", 47, KEY_DOT, '.'},
    {"dot", 47, KEY_DOT, '.'},
    {"tab", 48, KEY_TAB, '\0'},
    {"space", 49, KEY_SPACE, ' '},
    {"`", 50, KEY_GRAVE, '`'},
    {"backspace", 51, KEY_BACKSPACE, '\0'},
    {"enter", 36, KEY_ENTER, '\0'},
    {"return", 36, KEY_ENTER, '\0'},
    {"escape", 53, KEY_ESC, '\0'},
    {"esc", 53, KEY_ESC, '\0'},
    {"right_command", 54, KEY_RIGHTMETA, '\0'},
    {"right_super", 54, KEY_RIGHTMETA, '\0'},
    {"left_command", 55, KEY_LEFTMETA, '\0'},
    {"left_super", 55, KEY_LEFTMETA, '\0'},
    {"left_shift", 56, KEY_LEFTSHIFT, '\0'},
    {"left_option", 58, KEY_LEFTALT, '\0'},
    {"left_alt", 58, KEY_LEFTALT, '\0'},
    {"left_control", 59, KEY_LEFTCTRL, '\0'},
    {"left_ctrl", 59, KEY_LEFTCTRL, '\0'},
    {"right_shift", 60, KEY_RIGHTSHIFT, '\0'},
    {"right_option", 61, KEY_RIGHTALT, '\0'},
    {"right_alt", 61, KEY_RIGHTALT, '\0'},
    {"right_control", 62, KEY_RIGHTCTRL, '\0'},
    {"right_ctrl", 62, KEY_RIGHTCTRL, '\0'},
};

static bool test_bit(unsigned int bit, const unsigned long *array, size_t array_length)
{
    return BIT_WORD(bit) < array_length && (array[BIT_WORD(bit)] & BIT_MASK(bit)) != 0;
}

static char *copy_cstring(const char *s)
{
    if (s == NULL) {
        return NULL;
    }

    const size_t length = strlen(s);
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, length + 1);
    return copy;
}

static bool key_name_equals(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }

    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool platform_keycode_from_name(const char *name, uint16_t *out_keycode)
{
    if (name == NULL || out_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (key_name_equals(name, LINUX_KEY_MAP[i].name)) {
            *out_keycode = LINUX_KEY_MAP[i].logical_keycode;
            return true;
        }
    }

    return false;
}

bool linux_evdev_keycode_from_logical(uint16_t logical_keycode, unsigned int *out_evdev_keycode)
{
    if (out_evdev_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (LINUX_KEY_MAP[i].logical_keycode == logical_keycode) {
            *out_evdev_keycode = LINUX_KEY_MAP[i].evdev_keycode;
            return true;
        }
    }
    return false;
}

bool linux_logical_keycode_from_evdev(unsigned int evdev_keycode, uint16_t *out_logical_keycode)
{
    if (out_logical_keycode == NULL) {
        return false;
    }

    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (LINUX_KEY_MAP[i].evdev_keycode == evdev_keycode) {
            *out_logical_keycode = LINUX_KEY_MAP[i].logical_keycode;
            return true;
        }
    }
    return false;
}

char linux_printable_from_logical(uint16_t logical_keycode)
{
    for (size_t i = 0; i < sizeof(LINUX_KEY_MAP) / sizeof(LINUX_KEY_MAP[0]); ++i) {
        if (LINUX_KEY_MAP[i].logical_keycode == logical_keycode && LINUX_KEY_MAP[i].printable != '\0') {
            return LINUX_KEY_MAP[i].printable;
        }
    }
    return '\0';
}

static void update_modifier_state(uint16_t logical_keycode, bool is_down)
{
    switch (logical_keycode) {
    case 54:
    case 55:
        g_linux.command_down = is_down;
        break;
    case 56:
    case 60:
        g_linux.shift_down = is_down;
        break;
    case 58:
    case 61:
        g_linux.option_down = is_down;
        break;
    case 59:
    case 62:
        g_linux.control_down = is_down;
        break;
    default:
        break;
    }
}

static bool device_name_is_stoin_virtual_keyboard(const char *name)
{
    return name != NULL && strcmp(name, STOIN_LINUX_UINPUT_NAME) == 0;
}

static const char *pedal_role_name(Platform_Pedal_Role role)
{
    switch (role) {
    case PLATFORM_PEDAL_ROLE_PHRASE_CORE:
        return "phrase_core";
    case PLATFORM_PEDAL_ROLE_PHRASE_NONVERB:
        return "phrase_nonverb";
    case PLATFORM_PEDAL_ROLE_NONE:
    case PLATFORM_PEDAL_ROLE_COUNT:
    default:
        return "none";
    }
}

static const char *pedal_role_label(Platform_Pedal_Role role)
{
    switch (role) {
    case PLATFORM_PEDAL_ROLE_PHRASE_CORE:
        return "core phrase";
    case PLATFORM_PEDAL_ROLE_PHRASE_NONVERB:
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

static bool input_device_is_keyboard(int fd)
{
    unsigned long ev_bits[BIT_ARRAY_LENGTH(EV_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        return false;
    }
    if (!test_bit(EV_KEY, ev_bits, sizeof(ev_bits) / sizeof(ev_bits[0]))) {
        return false;
    }

    unsigned long key_bits[BIT_ARRAY_LENGTH(KEY_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    return test_bit(KEY_A, key_bits, sizeof(key_bits) / sizeof(key_bits[0]))
        && test_bit(KEY_SPACE, key_bits, sizeof(key_bits) / sizeof(key_bits[0]))
        && test_bit(KEY_ENTER, key_bits, sizeof(key_bits) / sizeof(key_bits[0]));
}

static bool input_device_is_pedal_candidate(int fd)
{
    unsigned long ev_bits[BIT_ARRAY_LENGTH(EV_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        return false;
    }

    if (test_bit(EV_KEY, ev_bits, sizeof(ev_bits) / sizeof(ev_bits[0]))) {
        unsigned long key_bits[BIT_ARRAY_LENGTH(KEY_MAX)] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) == 0) {
            for (unsigned int code = 0; code < KEY_MAX; ++code) {
                if (test_bit(code, key_bits, sizeof(key_bits) / sizeof(key_bits[0]))) {
                    return true;
                }
            }
        }
    }

    if (test_bit(EV_ABS, ev_bits, sizeof(ev_bits) / sizeof(ev_bits[0]))) {
        unsigned long abs_bits[BIT_ARRAY_LENGTH(ABS_MAX)] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) == 0) {
            for (unsigned int code = 0; code < ABS_MAX; ++code) {
                if (test_bit(code, abs_bits, sizeof(abs_bits) / sizeof(abs_bits[0]))) {
                    return true;
                }
            }
        }
    }

    return false;
}

static void linux_device_identity(int fd, Linux_Pedal_Device *device)
{
    if (device == NULL) {
        return;
    }

    struct input_id id = {0};
    if (ioctl(fd, EVIOCGID, &id) == 0) {
        device->bustype = id.bustype;
        device->vendor_id = id.vendor;
        device->product_id = id.product;
        device->version = id.version;
    }
    (void)ioctl(fd, EVIOCGNAME(sizeof(device->name)), device->name);
}

static void free_pedal_binding(Linux_Pedal_Binding *binding)
{
    if (binding == NULL) {
        return;
    }
    free(binding->path);
    memset(binding, 0, sizeof(*binding));
}

static void clear_pedal_bindings(void)
{
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        free_pedal_binding(&g_linux.pedal_bindings[role]);
    }
}

static bool parse_optional_u32_field(const char *start, const char *end, const char *name, uint32_t *out_value)
{
    uint32_t value = 0;
    if (!json_parse_uint_field(start, end, name, &value)) {
        return false;
    }
    if (out_value != NULL) {
        *out_value = value;
    }
    return true;
}

static bool load_pedal_binding_from_json(
    const char *json,
    Platform_Pedal_Role role,
    Linux_Pedal_Binding *binding
)
{
    const char *end = NULL;
    const char *start = json_find_object(json, pedal_role_name(role), &end);
    if (start == NULL) {
        return false;
    }

    Linux_Pedal_Binding loaded;
    memset(&loaded, 0, sizeof(loaded));
    char *path = NULL;
    uint32_t type = 0;
    uint32_t code = 0;
    if (!json_parse_uint_field(start, end, "type", &type)
        || !json_parse_uint_field(start, end, "code", &code)
        || type > UINT16_MAX
        || code > UINT16_MAX
        || !json_parse_string_field(start, end, "path", &path)) {
        fprintf(stderr,
            "stoin: warning: ignoring incomplete %s pedal binding in %s\n",
            pedal_role_label(role),
            g_linux.pedal_config_path);
        free(path);
        return false;
    }
    loaded.path = path;
    loaded.type = (uint16_t)type;
    loaded.code = (uint16_t)code;

    uint32_t value = 0;
    if (parse_optional_u32_field(start, end, "value", &value)) {
        loaded.value = (int)value;
    }
    uint32_t temp = 0;
    if (parse_optional_u32_field(start, end, "vendor_id", &temp)) loaded.vendor_id = (uint16_t)temp;
    if (parse_optional_u32_field(start, end, "product_id", &temp)) loaded.product_id = (uint16_t)temp;
    if (parse_optional_u32_field(start, end, "bustype", &temp)) loaded.bustype = (uint16_t)temp;
    if (parse_optional_u32_field(start, end, "version", &temp)) loaded.version = (uint16_t)temp;
    char *name = NULL;
    if (json_parse_string_field(start, end, "name", &name)) {
        snprintf(loaded.name, sizeof(loaded.name), "%s", name);
        free(name);
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
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Linux_Pedal_Binding binding;
        memset(&binding, 0, sizeof(binding));
        if (load_pedal_binding_from_json(json, role, &binding)) {
            free_pedal_binding(&g_linux.pedal_bindings[role]);
            g_linux.pedal_bindings[role] = binding;
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
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        const Linux_Pedal_Binding *binding = &g_linux.pedal_bindings[role];
        if (!binding->valid) {
            continue;
        }

        fprintf(file, ",\n");
        fprintf(file, "  \"%s\": {\n", pedal_role_name(role));
        fprintf(file, "    \"backend\": \"linux_evdev\",\n");
        fprintf(file, "    \"path\": ");
        json_write_string(file, binding->path);
        fprintf(file, ",\n");
        fprintf(file, "    \"name\": ");
        json_write_string(file, binding->name);
        fprintf(file, ",\n");
        fprintf(file, "    \"vendor_id\": %u,\n", binding->vendor_id);
        fprintf(file, "    \"product_id\": %u,\n", binding->product_id);
        fprintf(file, "    \"bustype\": %u,\n", binding->bustype);
        fprintf(file, "    \"version\": %u,\n", binding->version);
        fprintf(file, "    \"type\": %u,\n", binding->type);
        fprintf(file, "    \"code\": %u,\n", binding->code);
        fprintf(file, "    \"value\": %d\n", binding->value);
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

static void print_pedal_binding(const Linux_Pedal_Binding *binding, Platform_Pedal_Role role)
{
    printf("stoin: pedal %s = path=", pedal_role_label(role));
    json_write_string(stdout, binding->path);
    printf(" name=");
    json_write_string(stdout, binding->name);
    printf(" vid=0x%04x pid=0x%04x type=%u code=%u value=%d\n",
        binding->vendor_id,
        binding->product_id,
        binding->type,
        binding->code,
        binding->value);
}

static bool binding_matches_pedal_device(const Linux_Pedal_Binding *binding, const Linux_Pedal_Device *device)
{
    if (binding == NULL || device == NULL || !binding->valid) {
        return false;
    }
    if (binding->path != NULL && device->path != NULL && strcmp(binding->path, device->path) == 0) {
        return true;
    }
    return binding->vendor_id != 0
        && binding->product_id != 0
        && binding->vendor_id == device->vendor_id
        && binding->product_id == device->product_id
        && (binding->name[0] == '\0' || strcmp(binding->name, device->name) == 0);
}

static bool pedal_event_is_press(const struct input_event *event)
{
    if (event == NULL) {
        return false;
    }
    if (event->type == EV_KEY) {
        return event->value != 0;
    }
    if (event->type == EV_ABS) {
        return event->value != 0;
    }
    return false;
}

static bool pedal_event_matches_binding(const Linux_Pedal_Binding *binding, const struct input_event *event)
{
    if (binding == NULL || event == NULL || !binding->valid) {
        return false;
    }
    if (binding->type != event->type || binding->code != event->code) {
        return false;
    }
    if (binding->type == EV_KEY) {
        return true;
    }
    return event->value == 0 || event->value == binding->value;
}

static void learn_pedal_binding(const Linux_Pedal_Device *device, const struct input_event *event)
{
    if (device == NULL
        || event == NULL
        || !pedal_role_is_bindable(g_linux.pedal_register_role)
        || !pedal_event_is_press(event)) {
        return;
    }

    Linux_Pedal_Binding binding;
    memset(&binding, 0, sizeof(binding));
    binding.path = copy_cstring(device->path);
    if (binding.path == NULL) {
        fputs("stoin: warning: failed to copy Linux pedal path\n", stderr);
        return;
    }

    snprintf(binding.name, sizeof(binding.name), "%s", device->name);
    binding.vendor_id = device->vendor_id;
    binding.product_id = device->product_id;
    binding.bustype = device->bustype;
    binding.version = device->version;
    binding.type = event->type;
    binding.code = event->code;
    binding.value = event->value;
    binding.valid = true;

    free_pedal_binding(&g_linux.pedal_bindings[g_linux.pedal_register_role]);
    g_linux.pedal_bindings[g_linux.pedal_register_role] = binding;

    printf("stoin: learned %s pedal from Linux evdev device\n",
        pedal_role_label(g_linux.pedal_register_role));
    print_pedal_binding(&binding, g_linux.pedal_register_role);
    if (!save_pedal_config(g_linux.pedal_config_path)) {
        fprintf(stderr, "stoin: warning: failed to save pedal config to %s\n", g_linux.pedal_config_path);
    }
    g_linux.pedal_registering = false;
}

static void process_pedal_event(Linux_Pedal_Device *device, const struct input_event *event)
{
    if (device == NULL || event == NULL || (event->type != EV_KEY && event->type != EV_ABS)) {
        return;
    }

    if (g_linux.pedal_registering) {
        learn_pedal_binding(device, event);
        return;
    }

    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Linux_Pedal_Binding *binding = &g_linux.pedal_bindings[role];
        if (!binding_matches_pedal_device(binding, device)
            || !pedal_event_matches_binding(binding, event)) {
            continue;
        }

        const bool pressed = pedal_event_is_press(event);
        if (binding->is_down == pressed) {
            continue;
        }
        binding->is_down = pressed;
        if (g_linux.pedal_handler != NULL) {
            g_linux.pedal_handler(role, pressed, g_linux.pedal_userdata);
        }
    }
}

static void process_pedal_device(Linux_Pedal_Device *device)
{
    if (device == NULL || device->fd < 0) {
        return;
    }

    while (true) {
        struct input_event event = {0};
        const ssize_t bytes_read = read(device->fd, &event, sizeof(event));
        if (bytes_read == (ssize_t)sizeof(event)) {
            process_pedal_event(device, &event);
            continue;
        }
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

static bool add_pedal_device(const char *path, bool require_saved_binding)
{
    if (path == NULL) {
        return false;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    Linux_Pedal_Device device = {
        .fd = fd,
        .path = copy_cstring(path),
    };
    linux_device_identity(fd, &device);
    if (device.path == NULL
        || device_name_is_stoin_virtual_keyboard(device.name)
        || !input_device_is_pedal_candidate(fd)) {
        free(device.path);
        close(fd);
        return false;
    }

    bool has_saved_binding = false;
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        if (binding_matches_pedal_device(&g_linux.pedal_bindings[role], &device)) {
            has_saved_binding = true;
            break;
        }
    }
    if (require_saved_binding && !has_saved_binding) {
        free(device.path);
        close(fd);
        return false;
    }

    if (!g_linux.pedal_registering && ioctl(fd, EVIOCGRAB, 1) != 0) {
        fprintf(stderr, "stoin: warning: failed to grab Linux pedal device %s", path);
        if (errno != 0) {
            fprintf(stderr, " (%s)", strerror(errno));
        }
        fputc('\n', stderr);
    }

    arrput(g_linux.pedals, device);
    if (!g_linux.pedal_registering) {
        printf("stoin: watching Linux pedal candidate %s path=%s vid=0x%04x pid=0x%04x\n",
            device.name[0] != '\0' ? device.name : "(unknown device)",
            device.path,
            device.vendor_id,
            device.product_id);
    }
    return true;
}

static void close_pedal_devices(void)
{
    for (size_t i = 0; i < arrlenu(g_linux.pedals); ++i) {
        if (g_linux.pedals[i].fd >= 0) {
            (void)ioctl(g_linux.pedals[i].fd, EVIOCGRAB, 0);
            close(g_linux.pedals[i].fd);
        }
        free(g_linux.pedals[i].path);
    }
    arrfree(g_linux.pedals);
    g_linux.pedals = NULL;
}

static bool open_pedal_devices(bool registering)
{
    glob_t matches = {0};
    const int glob_result = glob("/dev/input/event*", 0, NULL, &matches);
    if (glob_result != 0 || matches.gl_pathc == 0) {
        globfree(&matches);
        return false;
    }

    for (size_t i = 0; i < matches.gl_pathc; ++i) {
        (void)add_pedal_device(matches.gl_pathv[i], !registering);
    }

    globfree(&matches);
    return arrlenu(g_linux.pedals) > 0;
}

typedef enum Linux_Keyboard_Open_Result {
    LINUX_KEYBOARD_OPEN_ADDED,
    LINUX_KEYBOARD_OPEN_PERMISSION_DENIED,
    LINUX_KEYBOARD_OPEN_FAILED,
    LINUX_KEYBOARD_OPEN_NOT_KEYBOARD,
    LINUX_KEYBOARD_OPEN_GRAB_FAILED,
    LINUX_KEYBOARD_OPEN_ALLOCATION_FAILED,
} Linux_Keyboard_Open_Result;

typedef struct Linux_Keyboard_Open_Report {
    size_t path_count;
    size_t added_count;
    size_t permission_denied_count;
    size_t open_failed_count;
    size_t non_keyboard_count;
    size_t grab_failed_count;
    size_t allocation_failed_count;
} Linux_Keyboard_Open_Report;

static Linux_Keyboard_Open_Result add_keyboard_device(const char *path)
{
    if (path == NULL) {
        return LINUX_KEYBOARD_OPEN_FAILED;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return (errno == EACCES || errno == EPERM)
            ? LINUX_KEYBOARD_OPEN_PERMISSION_DENIED
            : LINUX_KEYBOARD_OPEN_FAILED;
    }

    char name[256] = {0};
    (void)ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    if (device_name_is_stoin_virtual_keyboard(name) || !input_device_is_keyboard(fd)) {
        close(fd);
        return LINUX_KEYBOARD_OPEN_NOT_KEYBOARD;
    }

    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        fprintf(stderr, "stoin: warning: failed to grab Linux keyboard device %s", path);
        if (errno != 0) {
            fprintf(stderr, " (%s)", strerror(errno));
        }
        fputc('\n', stderr);
        close(fd);
        return LINUX_KEYBOARD_OPEN_GRAB_FAILED;
    }

    char *path_copy = copy_cstring(path);
    if (path_copy == NULL) {
        (void)ioctl(fd, EVIOCGRAB, 0);
        close(fd);
        return LINUX_KEYBOARD_OPEN_ALLOCATION_FAILED;
    }

    Linux_Keyboard_Device device = {
        .fd = fd,
        .path = path_copy,
    };
    snprintf(device.name, sizeof(device.name), "%s", name[0] != '\0' ? name : path);
    arrput(g_linux.keyboards, device);
    return LINUX_KEYBOARD_OPEN_ADDED;
}

static void close_keyboard_devices(void)
{
    for (size_t i = 0; i < arrlenu(g_linux.keyboards); ++i) {
        if (g_linux.keyboards[i].fd >= 0) {
            (void)ioctl(g_linux.keyboards[i].fd, EVIOCGRAB, 0);
            close(g_linux.keyboards[i].fd);
        }
        free(g_linux.keyboards[i].path);
    }
    arrfree(g_linux.keyboards);
    g_linux.keyboards = NULL;
}

static void print_keyboard_open_report(const Linux_Keyboard_Open_Report *report)
{
    if (report == NULL) {
        return;
    }

    fprintf(stderr,
        "stoin: checked %zu Linux input event devices: %zu opened, %zu permission denied, %zu open failed, %zu not keyboards, %zu grab failed\n",
        report->path_count,
        report->added_count,
        report->permission_denied_count,
        report->open_failed_count,
        report->non_keyboard_count,
        report->grab_failed_count);
    if (report->permission_denied_count > 0) {
        fputs("stoin: qwerty capture needs read/grab access to keyboard devices under /dev/input/event*\n", stderr);
        fputs("stoin: see docs/linux-setup.md for the stoin input-device udev rule\n", stderr);
    }
}

static bool open_keyboard_devices(void)
{
    glob_t matches = {0};
    const int glob_result = glob("/dev/input/event*", 0, NULL, &matches);
    if (glob_result != 0 || matches.gl_pathc == 0) {
        globfree(&matches);
        fputs("stoin: no Linux input event devices found under /dev/input/event*\n", stderr);
        return false;
    }

    Linux_Keyboard_Open_Report report = {
        .path_count = matches.gl_pathc,
    };

    for (size_t i = 0; i < matches.gl_pathc; ++i) {
        switch (add_keyboard_device(matches.gl_pathv[i])) {
        case LINUX_KEYBOARD_OPEN_ADDED:
            ++report.added_count;
            break;
        case LINUX_KEYBOARD_OPEN_PERMISSION_DENIED:
            ++report.permission_denied_count;
            break;
        case LINUX_KEYBOARD_OPEN_FAILED:
            ++report.open_failed_count;
            break;
        case LINUX_KEYBOARD_OPEN_NOT_KEYBOARD:
            ++report.non_keyboard_count;
            break;
        case LINUX_KEYBOARD_OPEN_GRAB_FAILED:
            ++report.grab_failed_count;
            break;
        case LINUX_KEYBOARD_OPEN_ALLOCATION_FAILED:
            ++report.allocation_failed_count;
            break;
        }
    }

    globfree(&matches);
    if (arrlenu(g_linux.keyboards) == 0) {
        print_keyboard_open_report(&report);
    }
    return arrlenu(g_linux.keyboards) > 0;
}

static void process_key_event(const struct input_event *event)
{
    if (event == NULL || event->type != EV_KEY) {
        return;
    }

    uint16_t logical_keycode = 0;
    const bool mapped = linux_logical_keycode_from_evdev(event->code, &logical_keycode);
    const bool is_down = event->value != 0;
    const bool is_repeat = event->value == 2;

    if (mapped && !is_repeat) {
        update_modifier_state(logical_keycode, is_down);
    }

    bool consumed = false;
    if (mapped && g_linux.handler != NULL) {
        const Input_Event input = {
            .keycode = logical_keycode,
            .is_down = is_down,
            .is_repeat = is_repeat,
            .shift = g_linux.shift_down,
            .control = g_linux.control_down,
            .option = g_linux.option_down,
            .command = g_linux.command_down,
            .printable = linux_printable_from_logical(logical_keycode),
        };
        consumed = g_linux.handler(&input, g_linux.userdata);
    }

    if (!consumed) {
        (void)linux_uinput_emit_key_event(event->code, event->value);
    }
}

static void process_keyboard_device(Linux_Keyboard_Device *device)
{
    if (device == NULL || device->fd < 0) {
        return;
    }

    while (true) {
        struct input_event event = {0};
        const ssize_t bytes_read = read(device->fd, &event, sizeof(event));
        if (bytes_read == (ssize_t)sizeof(event)) {
            process_key_event(&event);
            continue;
        }
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

bool platform_user_session_is_active(void)
{
    return true;
}

void platform_translation_timing_set_enabled(bool enabled)
{
    g_linux.translation_timing_enabled = enabled;
    if (!enabled) {
        g_linux.translation_timing_active = false;
        g_linux.translation_timing_start_ns = 0;
    }
}

void platform_translation_timing_begin(uint64_t start_ns)
{
    if (!g_linux.translation_timing_enabled || start_ns == 0) {
        return;
    }

    g_linux.translation_timing_start_ns = start_ns;
    g_linux.translation_timing_active = true;
}

void platform_translation_timing_cancel(void)
{
    g_linux.translation_timing_active = false;
    g_linux.translation_timing_start_ns = 0;
}

void linux_report_translation_timing_before_output(const char *operation)
{
    if (!g_linux.translation_timing_enabled || !g_linux.translation_timing_active) {
        return;
    }

    const uint64_t now_ns = platform_monotonic_ns();
    const uint64_t start_ns = g_linux.translation_timing_start_ns;
    const uint64_t elapsed_ns = now_ns >= start_ns ? now_ns - start_ns : 0;
    g_linux.translation_timing_active = false;
    g_linux.translation_timing_start_ns = 0;
    ++g_linux.translation_timing_sequence;

    fprintf(stderr,
        "stoin: translation latency #%" PRIu64 " before %s uinput event: %.3f ms (%.1f us)\n",
        g_linux.translation_timing_sequence,
        operation != NULL ? operation : "first",
        (double)elapsed_ns / 1000000.0,
        (double)elapsed_ns / 1000.0);
}

uint64_t platform_monotonic_ns(void)
{
    struct timespec now = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

uint64_t platform_monotonic_ms(void)
{
    return platform_monotonic_ns() / UINT64_C(1000000);
}

void platform_sleep_ms(unsigned int milliseconds)
{
    struct timespec requested = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };

    while (nanosleep(&requested, &requested) != 0 && errno == EINTR) {
    }
}

bool platform_init(Handle_Input_Fn handler, void *userdata)
{
    g_linux.handler = handler;
    g_linux.userdata = userdata;

    if (!platform_output_init()) {
        return false;
    }

    if (!open_keyboard_devices()) {
        fputs("stoin: failed to open and grab any Linux keyboard devices under /dev/input/event*\n", stderr);
        fputs("stoin: check qwerty capture permissions for /dev/input/event*, then restart stoin\n", stderr);
        platform_shutdown();
        return false;
    }

    g_linux.running = true;
    return true;
}

void platform_run(void)
{
    g_linux.running = true;
    while (g_linux.running) {
        struct pollfd *fds = NULL;

        for (size_t i = 0; i < arrlenu(g_linux.keyboards); ++i) {
            if (g_linux.keyboards[i].fd >= 0) {
                arrput(fds, ((struct pollfd) {
                    .fd = g_linux.keyboards[i].fd,
                    .events = POLLIN,
                }));
            }
        }

        const size_t pedal_index = arrlenu(fds);
        for (size_t i = 0; i < arrlenu(g_linux.pedals); ++i) {
            if (g_linux.pedals[i].fd >= 0) {
                arrput(fds, ((struct pollfd) {
                    .fd = g_linux.pedals[i].fd,
                    .events = POLLIN,
                }));
            }
        }

        const int watcher_fd = linux_file_watcher_fd();
        const size_t watcher_index = arrlenu(fds);
        if (watcher_fd >= 0) {
            arrput(fds, ((struct pollfd) {
                .fd = watcher_fd,
                .events = POLLIN,
            }));
        }

        if (arrlenu(fds) == 0) {
            arrfree(fds);
            platform_sleep_ms(250);
            continue;
        }

        const int ready = poll(fds, (nfds_t)arrlenu(fds), -1);
        if (ready < 0) {
            arrfree(fds);
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        size_t fd_index = 0;
        for (size_t i = 0; i < arrlenu(g_linux.keyboards); ++i) {
            if (g_linux.keyboards[i].fd < 0) {
                continue;
            }
            if ((fds[fd_index].revents & POLLIN) != 0) {
                process_keyboard_device(&g_linux.keyboards[i]);
            }
            ++fd_index;
        }

        fd_index = pedal_index;
        for (size_t i = 0; i < arrlenu(g_linux.pedals); ++i) {
            if (g_linux.pedals[i].fd < 0) {
                continue;
            }
            if ((fds[fd_index].revents & POLLIN) != 0) {
                process_pedal_device(&g_linux.pedals[i]);
            }
            ++fd_index;
        }

        if (watcher_fd >= 0
            && watcher_index < arrlenu(fds)
            && (fds[watcher_index].revents & POLLIN) != 0) {
            platform_file_watcher_poll();
        }

        arrfree(fds);
    }
}

void platform_shutdown(void)
{
    g_linux.running = false;
    platform_file_watcher_stop();
    platform_pedals_shutdown();
    close_keyboard_devices();
    linux_output_shutdown();
    g_linux.handler = NULL;
    g_linux.userdata = NULL;
    g_linux.shift_down = false;
    g_linux.control_down = false;
    g_linux.option_down = false;
    g_linux.command_down = false;
}

bool platform_pedals_init(
    const char *config_path,
    Platform_Pedal_Role register_role,
    Platform_Pedal_Event_Fn handler,
    void *userdata
)
{
    platform_pedals_shutdown();

    g_linux.pedal_config_path = config_path != NULL ? config_path : "stoin-pedals.json";
    g_linux.pedal_handler = handler;
    g_linux.pedal_userdata = userdata;
    g_linux.pedal_register_role = register_role;
    g_linux.pedal_registering = pedal_role_is_bindable(register_role);

    const bool loaded = load_pedal_config(g_linux.pedal_config_path);
    if (!loaded && !g_linux.pedal_registering) {
        return true;
    }

    if (!open_pedal_devices(g_linux.pedal_registering)) {
        if (g_linux.pedal_registering) {
            fputs("stoin: warning: no readable Linux input devices found for pedal registration\n", stderr);
            fputs("stoin: pedal registration needs read access to /dev/input/event* devices\n", stderr);
        }
        platform_pedals_shutdown();
        return !pedal_role_is_bindable(register_role);
    }

    if (g_linux.pedal_registering) {
        printf("stoin: pedal registration armed for %s mode\n", pedal_role_label(register_role));
        printf("stoin: press the pedal to use as %s mode\n", pedal_role_label(register_role));
        puts("stoin: avoid typing, clicking, or pressing other input devices until registration finishes");
        while (g_linux.pedal_registering) {
            platform_pedals_poll();
            platform_sleep_ms(10);
        }
        platform_pedals_shutdown();
        return platform_pedals_init(config_path, PLATFORM_PEDAL_ROLE_NONE, handler, userdata);
    }

    if (loaded) {
        for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
             role < PLATFORM_PEDAL_ROLE_COUNT;
             ++role) {
            if (g_linux.pedal_bindings[role].valid) {
                print_pedal_binding(&g_linux.pedal_bindings[role], role);
            }
        }
    }
    return true;
}

void platform_pedals_poll(void)
{
    for (size_t i = 0; i < arrlenu(g_linux.pedals); ++i) {
        process_pedal_device(&g_linux.pedals[i]);
    }
}

void platform_pedals_shutdown(void)
{
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_PHRASE_CORE;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Linux_Pedal_Binding *binding = &g_linux.pedal_bindings[role];
        if (binding->valid && binding->is_down && g_linux.pedal_handler != NULL) {
            g_linux.pedal_handler(role, false, g_linux.pedal_userdata);
        }
    }

    close_pedal_devices();
    clear_pedal_bindings();
    g_linux.pedal_config_path = NULL;
    g_linux.pedal_handler = NULL;
    g_linux.pedal_userdata = NULL;
    g_linux.pedal_register_role = PLATFORM_PEDAL_ROLE_NONE;
    g_linux.pedal_registering = false;
}
