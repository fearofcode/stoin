#include "platform.h"

#include "json_util.h"
#include "util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    HID_PAGE_KEYBOARD = 0x07,
    HID_PAGE_BUTTON = 0x09,
    HID_KEYBOARD_F13 = 0x68,
    HID_KEYBOARD_F24 = 0x73,
    MAC_PEDAL_MAX_DEVICES = 64,
};

typedef struct Mac_Pedal_Binding {
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t location_id;
    uint32_t usage_page;
    uint32_t usage;
    uint32_t element_usage;
    uint32_t element_cookie;
    bool valid;
    bool is_down;
} Mac_Pedal_Binding;

typedef struct Mac_Pedal_Device {
    IOHIDDeviceRef device;
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t location_id;
    char product_name[128];
    bool used;
} Mac_Pedal_Device;

typedef struct Mac_Pedal_State {
    IOHIDManagerRef manager;
    CFRunLoopRef run_loop;
    Mac_Pedal_Device devices[MAC_PEDAL_MAX_DEVICES];
    Mac_Pedal_Binding bindings[PLATFORM_PEDAL_ROLE_COUNT];
    const char *config_path;
    Platform_Pedal_Event_Fn handler;
    void *userdata;
    Platform_Pedal_Role register_role;
    bool registering;
    bool started;
    bool manager_opened_devices;
    bool manager_seized_devices;
} Mac_Pedal_State;

static Mac_Pedal_State g_pedals;

static const char *pedal_role_name(Platform_Pedal_Role role)
{
    switch (role) {
    case PLATFORM_PEDAL_ROLE_INITIAL_VERB:
        return "initial_verb";
    case PLATFORM_PEDAL_ROLE_PHRASE_NONVERB:
        return "phrase_nonverb";
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
    case PLATFORM_PEDAL_ROLE_NONE:
    case PLATFORM_PEDAL_ROLE_PHRASE_NONVERB:
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

static bool usage_page_is_pedal_candidate(uint32_t usage_page)
{
    return usage_page != 0;
}

static bool keyboard_usage_is_safe_pedal_key(uint32_t usage)
{
    return usage >= HID_KEYBOARD_F13 && usage <= HID_KEYBOARD_F24;
}

static void cf_dictionary_set_u32(CFMutableDictionaryRef dictionary, CFStringRef key, uint32_t value)
{
    if (dictionary == NULL || key == NULL) {
        return;
    }

    int number_value = (int)value;
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &number_value);
    if (number == NULL) {
        return;
    }

    CFDictionarySetValue(dictionary, key, number);
    CFRelease(number);
}

static bool hid_keyboard_array_element(IOHIDElementRef element)
{
    if (element == NULL || IOHIDElementGetUsagePage(element) != HID_PAGE_KEYBOARD) {
        return false;
    }

    const IOHIDElementType type = IOHIDElementGetType(element);
    return type == kIOHIDElementTypeInput_ScanCodes
        || IOHIDElementGetUsage(element) == UINT32_MAX;
}

static uint32_t hid_element_cookie(IOHIDElementRef element)
{
    return element == NULL ? 0 : (uint32_t)(uintptr_t)IOHIDElementGetCookie(element);
}

static uint32_t pedal_event_usage(IOHIDElementRef element, IOHIDValueRef value)
{
    if (element == NULL || value == NULL) {
        return 0;
    }

    if (hid_keyboard_array_element(element)) {
        const CFIndex integer_value = IOHIDValueGetIntegerValue(value);
        return integer_value > 0 && integer_value <= UINT32_MAX ? (uint32_t)integer_value : 0;
    }

    return IOHIDElementGetUsage(element);
}

static uint32_t hid_u32_property(IOHIDDeviceRef device, CFStringRef key)
{
    if (device == NULL || key == NULL) {
        return 0;
    }

    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return 0;
    }

    int number = 0;
    if (!CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &number)) {
        return 0;
    }
    return (uint32_t)number;
}

static void hid_string_property(IOHIDDeviceRef device, CFStringRef key, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return;
    }
    buffer[0] = '\0';

    if (device == NULL || key == NULL) {
        return;
    }

    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID()) {
        return;
    }

    (void)CFStringGetCString((CFStringRef)value, buffer, (CFIndex)buffer_size, kCFStringEncodingUTF8);
}

static Mac_Pedal_Device *find_device_context(IOHIDDeviceRef device)
{
    for (size_t i = 0; i < MAC_PEDAL_MAX_DEVICES; ++i) {
        if (g_pedals.devices[i].used && g_pedals.devices[i].device == device) {
            return &g_pedals.devices[i];
        }
    }
    return NULL;
}

static Mac_Pedal_Device *alloc_device_context(void)
{
    for (size_t i = 0; i < MAC_PEDAL_MAX_DEVICES; ++i) {
        if (!g_pedals.devices[i].used) {
            g_pedals.devices[i].used = true;
            return &g_pedals.devices[i];
        }
    }
    return NULL;
}

static bool binding_matches_device(
    const Mac_Pedal_Binding *binding,
    const Mac_Pedal_Device *device,
    uint32_t usage_page,
    uint32_t usage
)
{
    return binding != NULL
        && device != NULL
        && binding->valid
        && binding->vendor_id == device->vendor_id
        && binding->product_id == device->product_id
        && binding->location_id == device->location_id
        && binding->usage_page == usage_page
        && binding->usage == usage;
}

static bool device_has_saved_binding(const Mac_Pedal_Device *device)
{
    if (device == NULL) {
        return false;
    }

    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        const Mac_Pedal_Binding *binding = &g_pedals.bindings[role];
        if (binding->valid
            && binding->vendor_id == device->vendor_id
            && binding->product_id == device->product_id
            && binding->location_id == device->location_id) {
            return true;
        }
    }
    return false;
}

static bool binding_device_already_matched(const Mac_Pedal_Binding *binding, Platform_Pedal_Role stop_role)
{
    if (binding == NULL || !binding->valid) {
        return true;
    }

    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB; role < stop_role; ++role) {
        const Mac_Pedal_Binding *other = &g_pedals.bindings[role];
        if (other->valid
            && other->vendor_id == binding->vendor_id
            && other->product_id == binding->product_id
            && other->location_id == binding->location_id) {
            return true;
        }
    }
    return false;
}

static CFMutableDictionaryRef create_device_matching_dictionary(const Mac_Pedal_Binding *binding)
{
    if (binding == NULL || !binding->valid) {
        return NULL;
    }

    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    if (dictionary == NULL) {
        return NULL;
    }

    cf_dictionary_set_u32(dictionary, CFSTR("VendorID"), binding->vendor_id);
    cf_dictionary_set_u32(dictionary, CFSTR("ProductID"), binding->product_id);
    cf_dictionary_set_u32(dictionary, CFSTR("LocationID"), binding->location_id);
    return dictionary;
}

static bool set_runtime_device_matching(IOHIDManagerRef manager)
{
    if (manager == NULL) {
        return false;
    }

    CFMutableArrayRef matches = CFArrayCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeArrayCallBacks
    );
    if (matches == NULL) {
        return false;
    }

    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        const Mac_Pedal_Binding *binding = &g_pedals.bindings[role];
        if (binding_device_already_matched(binding, role)) {
            continue;
        }

        CFMutableDictionaryRef dictionary = create_device_matching_dictionary(binding);
        if (dictionary == NULL) {
            CFRelease(matches);
            return false;
        }
        CFArrayAppendValue(matches, dictionary);
        CFRelease(dictionary);
    }

    const CFIndex count = CFArrayGetCount(matches);
    if (count > 0) {
        IOHIDManagerSetDeviceMatchingMultiple(manager, matches);
    }
    CFRelease(matches);
    return count > 0;
}

static const Mac_Pedal_Binding *find_unsafe_keyboard_binding(
    const Mac_Pedal_Device *device,
    Platform_Pedal_Role *out_role
)
{
    if (device == NULL) {
        return NULL;
    }

    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        const Mac_Pedal_Binding *binding = &g_pedals.bindings[role];
        if (binding->valid
            && binding->usage_page == HID_PAGE_KEYBOARD
            && !keyboard_usage_is_safe_pedal_key(binding->usage)
            && binding->vendor_id == device->vendor_id
            && binding->product_id == device->product_id
            && binding->location_id == device->location_id) {
            if (out_role != NULL) {
                *out_role = role;
            }
            return binding;
        }
    }
    return NULL;
}

static bool parse_optional_uint_field(const char *start, const char *end, const char *name, uint32_t *out_value)
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

static bool load_binding_from_json(const char *json, Platform_Pedal_Role role, Mac_Pedal_Binding *binding)
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

    Mac_Pedal_Binding loaded = {0};
    if (!json_parse_uint_field(start, end, "vendor_id", &loaded.vendor_id)
        || !json_parse_uint_field(start, end, "product_id", &loaded.product_id)
        || !json_parse_uint_field(start, end, "location_id", &loaded.location_id)
        || !json_parse_uint_field(start, end, "usage_page", &loaded.usage_page)
        || !json_parse_uint_field(start, end, "usage", &loaded.usage)) {
        fprintf(stderr,
            "stoin: warning: ignoring incomplete %s pedal binding in %s\n",
            pedal_role_label(role),
            g_pedals.config_path);
        return false;
    }

    loaded.element_usage = loaded.usage;
    (void)parse_optional_uint_field(start, end, "element_usage", &loaded.element_usage);
    (void)parse_optional_uint_field(start, end, "element_cookie", &loaded.element_cookie);
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
        Mac_Pedal_Binding binding = {0};
        if (load_binding_from_json(json, role, &binding)) {
            g_pedals.bindings[role] = binding;
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
        const Mac_Pedal_Binding *binding = &g_pedals.bindings[role];
        if (!binding->valid) {
            continue;
        }

        fprintf(file, ",\n");
        fprintf(file, "  \"%s\": {\n", pedal_role_name(role));
        fprintf(file, "    \"vendor_id\": %u,\n", binding->vendor_id);
        fprintf(file, "    \"product_id\": %u,\n", binding->product_id);
        fprintf(file, "    \"location_id\": %u,\n", binding->location_id);
        fprintf(file, "    \"usage_page\": %u,\n", binding->usage_page);
        fprintf(file, "    \"usage\": %u,\n", binding->usage);
        fprintf(file, "    \"element_usage\": %u,\n", binding->element_usage);
        fprintf(file, "    \"element_cookie\": %u\n", binding->element_cookie);
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

static void print_binding(const Mac_Pedal_Binding *binding, Platform_Pedal_Role role)
{
    printf("stoin: pedal %s = vid=0x%04x pid=0x%04x loc=0x%08x page=0x%02x usage=0x%02x element=0x%02x cookie=0x%08x\n",
        pedal_role_label(role),
        binding->vendor_id,
        binding->product_id,
        binding->location_id,
        binding->usage_page,
        binding->usage,
        binding->element_usage,
        binding->element_cookie);
}

static void update_keyboard_array_binding(
    Mac_Pedal_Binding *binding,
    uint32_t usage_page,
    uint32_t usage,
    uint32_t element_usage,
    uint32_t element_cookie
)
{
    if (binding == NULL
        || usage_page != HID_PAGE_KEYBOARD
        || usage == 0
        || (binding->usage == usage
            && binding->element_usage == element_usage
            && binding->element_cookie == element_cookie)) {
        return;
    }

    if (binding->usage == element_usage || binding->usage == UINT32_MAX) {
        binding->usage = usage;
        binding->element_usage = element_usage;
        binding->element_cookie = element_cookie;
        if (save_pedal_config(g_pedals.config_path)) {
            fputs("stoin: updated keyboard-array pedal binding with precise key usage\n", stderr);
        }
    }
}

static void learn_pedal_binding(
    const Mac_Pedal_Device *device,
    uint32_t usage_page,
    uint32_t usage,
    uint32_t element_usage,
    uint32_t element_cookie
)
{
    if (!pedal_role_is_bindable(g_pedals.register_role)) {
        return;
    }

    Mac_Pedal_Binding binding = {
        .vendor_id = device->vendor_id,
        .product_id = device->product_id,
        .location_id = device->location_id,
        .usage_page = usage_page,
        .usage = usage,
        .element_usage = element_usage,
        .element_cookie = element_cookie,
        .valid = true,
    };

    g_pedals.bindings[g_pedals.register_role] = binding;
    printf("stoin: learned %s pedal from %s page=0x%02x usage=0x%02x element=0x%02x\n",
        pedal_role_label(g_pedals.register_role),
        device->product_name[0] != '\0' ? device->product_name : "(unknown device)",
        usage_page,
        usage,
        element_usage);
    print_binding(&binding, g_pedals.register_role);
    if (binding.usage_page == HID_PAGE_KEYBOARD
        && !keyboard_usage_is_safe_pedal_key(binding.usage)) {
        fprintf(stderr,
            "stoin: warning: this pedal reports keyboard usage 0x%02x. It must be seized successfully or macOS may also type it into the active app.\n",
            binding.usage);
    }

    if (!save_pedal_config(g_pedals.config_path)) {
        fprintf(stderr, "stoin: warning: failed to save pedal config to %s\n", g_pedals.config_path);
    }

    g_pedals.registering = false;
}

static Platform_Pedal_Role role_for_event(
    const Mac_Pedal_Device *device,
    uint32_t usage_page,
    uint32_t usage,
    uint32_t element_usage,
    uint32_t element_cookie,
    bool pressed,
    Mac_Pedal_Binding **out_binding
)
{
    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Mac_Pedal_Binding *binding = &g_pedals.bindings[role];
        bool matches = binding_matches_device(binding, device, usage_page, usage);
        if (!matches
            && binding != NULL
            && binding->valid
            && device != NULL
            && binding->vendor_id == device->vendor_id
            && binding->product_id == device->product_id
            && binding->location_id == device->location_id
            && binding->usage_page == usage_page) {
            const bool cookie_matches =
                binding->element_cookie == 0 || binding->element_cookie == element_cookie;
            if (usage_page == HID_PAGE_KEYBOARD && cookie_matches) {
                if (pressed) {
                    matches = usage != 0
                        && (binding->usage == usage || binding->usage == element_usage);
                } else {
                    matches = binding->is_down;
                }
            }
        }

        if (matches) {
            if (out_binding != NULL) {
                *out_binding = binding;
            }
            return role;
        }
    }
    return PLATFORM_PEDAL_ROLE_NONE;
}

static void pedal_input_callback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
    (void)sender;
    if (context == NULL || value == NULL || result != kIOReturnSuccess) {
        return;
    }

    Mac_Pedal_Device *device = context;
    IOHIDElementRef element = IOHIDValueGetElement(value);
    if (element == NULL) {
        return;
    }

    const uint32_t usage_page = IOHIDElementGetUsagePage(element);
    const uint32_t element_usage = IOHIDElementGetUsage(element);
    const uint32_t element_cookie = hid_element_cookie(element);
    const uint32_t usage = pedal_event_usage(element, value);
    if (!usage_page_is_pedal_candidate(usage_page)) {
        return;
    }

    const bool pressed = usage_page == HID_PAGE_KEYBOARD && hid_keyboard_array_element(element)
        ? usage != 0
        : IOHIDValueGetIntegerValue(value) != 0;

    if (g_pedals.registering) {
        if (pressed) {
            learn_pedal_binding(device, usage_page, usage, element_usage, element_cookie);
        }
        return;
    }

    Mac_Pedal_Binding *binding = NULL;
    const Platform_Pedal_Role role = role_for_event(
        device,
        usage_page,
        usage,
        element_usage,
        element_cookie,
        pressed,
        &binding
    );
    if (role == PLATFORM_PEDAL_ROLE_NONE || binding == NULL || g_pedals.handler == NULL) {
        return;
    }

    if (pressed) {
        update_keyboard_array_binding(binding, usage_page, usage, element_usage, element_cookie);
    }

    if (binding->is_down == pressed) {
        return;
    }
    binding->is_down = pressed;
    g_pedals.handler(role, pressed, g_pedals.userdata);
}

static bool add_pedal_device(IOHIDDeviceRef hid_device)
{
    if (hid_device == NULL || find_device_context(hid_device) != NULL) {
        return true;
    }

    Mac_Pedal_Device probe = {
        .device = hid_device,
        .vendor_id = hid_u32_property(hid_device, CFSTR("VendorID")),
        .product_id = hid_u32_property(hid_device, CFSTR("ProductID")),
        .location_id = hid_u32_property(hid_device, CFSTR("LocationID")),
    };
    hid_string_property(hid_device, CFSTR("Product"), probe.product_name, sizeof(probe.product_name));

    if (!g_pedals.registering && !device_has_saved_binding(&probe)) {
        return true;
    }

    Mac_Pedal_Device *device = alloc_device_context();
    if (device == NULL) {
        fputs("stoin: warning: too many HID devices while initializing pedals\n", stderr);
        return false;
    }
    *device = probe;
    device->used = true;
    CFRetain(hid_device);

    bool seized = g_pedals.manager_seized_devices;
    if (!g_pedals.manager_opened_devices) {
        IOOptionBits open_options = g_pedals.registering
            ? kIOHIDOptionsTypeNone
            : kIOHIDOptionsTypeSeizeDevice;
        IOReturn open_result = IOHIDDeviceOpen(hid_device, open_options);
        seized = open_result == kIOReturnSuccess && open_options == kIOHIDOptionsTypeSeizeDevice;
        if (open_result != kIOReturnSuccess && open_options != kIOHIDOptionsTypeNone) {
            const IOReturn exclusive_result = open_result;
            open_options = kIOHIDOptionsTypeNone;
            open_result = IOHIDDeviceOpen(hid_device, open_options);
            seized = false;
            if (open_result == kIOReturnSuccess) {
                fprintf(stderr,
                    "stoin: pedal HID device %s opened in shared mode after exclusive open failed (0x%x)\n",
                    device->product_name[0] != '\0' ? device->product_name : "(unknown device)",
                    (unsigned int)exclusive_result);
            }
        }

        if (open_result != kIOReturnSuccess) {
            fprintf(stderr,
                "stoin: warning: failed to open HID device %s (0x%x)\n",
                device->product_name[0] != '\0' ? device->product_name : "(unknown device)",
                (unsigned int)open_result);
            CFRelease(hid_device);
            memset(device, 0, sizeof(*device));
            return false;
        }
    }

    if (!g_pedals.registering && !seized) {
        Platform_Pedal_Role unsafe_role = PLATFORM_PEDAL_ROLE_NONE;
        const Mac_Pedal_Binding *unsafe_binding = find_unsafe_keyboard_binding(&probe, &unsafe_role);
        if (unsafe_binding != NULL) {
            fprintf(stderr,
                "stoin: warning: %s pedal on %s is keyboard usage 0x%02x and is not seized; macOS may also type it into the active app\n",
                pedal_role_label(unsafe_role),
                probe.product_name[0] != '\0' ? probe.product_name : "(unknown device)",
                unsafe_binding->usage);
        }
    }

    if (!g_pedals.registering && seized) {
        fprintf(stderr,
            "stoin: pedal HID device %s opened exclusively\n",
            device->product_name[0] != '\0' ? device->product_name : "(unknown device)");
    }

    CFRunLoopRef run_loop = g_pedals.run_loop != NULL ? g_pedals.run_loop : CFRunLoopGetCurrent();
    IOHIDDeviceScheduleWithRunLoop(hid_device, run_loop, kCFRunLoopCommonModes);
    IOHIDDeviceRegisterInputValueCallback(hid_device, pedal_input_callback, device);
    if (!g_pedals.registering) {
        printf("stoin: watching pedal HID candidate %s vid=0x%04x pid=0x%04x loc=0x%08x%s\n",
            device->product_name[0] != '\0' ? device->product_name : "(unknown device)",
            device->vendor_id,
            device->product_id,
            device->location_id,
            seized ? " (exclusive)" : "");
    }
    return true;
}

static void release_bindings_for_device(const Mac_Pedal_Device *device)
{
    if (device == NULL) {
        return;
    }

    for (Platform_Pedal_Role role = PLATFORM_PEDAL_ROLE_INITIAL_VERB;
         role < PLATFORM_PEDAL_ROLE_COUNT;
         ++role) {
        Mac_Pedal_Binding *binding = &g_pedals.bindings[role];
        if (binding->is_down
            && binding->vendor_id == device->vendor_id
            && binding->product_id == device->product_id
            && binding->location_id == device->location_id) {
            binding->is_down = false;
            if (g_pedals.handler != NULL) {
                g_pedals.handler(role, false, g_pedals.userdata);
            }
        }
    }
}

static void remove_pedal_device(IOHIDDeviceRef hid_device)
{
    Mac_Pedal_Device *device = find_device_context(hid_device);
    if (device == NULL) {
        return;
    }

    release_bindings_for_device(device);
    IOHIDDeviceRegisterInputValueCallback(device->device, NULL, NULL);
    CFRunLoopRef run_loop = g_pedals.run_loop != NULL ? g_pedals.run_loop : CFRunLoopGetCurrent();
    IOHIDDeviceUnscheduleFromRunLoop(device->device, run_loop, kCFRunLoopCommonModes);
    if (!g_pedals.manager_opened_devices) {
        IOHIDDeviceClose(device->device, kIOHIDOptionsTypeNone);
    }
    CFRelease(device->device);
    memset(device, 0, sizeof(*device));
}

static void pedal_device_matching_callback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
    (void)context;
    (void)sender;
    if (result == kIOReturnSuccess) {
        (void)add_pedal_device(device);
    }
}

static void pedal_device_removal_callback(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
    (void)context;
    (void)sender;
    if (result == kIOReturnSuccess) {
        remove_pedal_device(device);
    }
}

static void register_existing_devices(IOHIDManagerRef manager)
{
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (devices == NULL) {
        return;
    }

    const CFIndex count = CFSetGetCount(devices);
    if (count <= 0) {
        CFRelease(devices);
        return;
    }

    const void **values = calloc((size_t)count, sizeof(*values));
    if (values == NULL) {
        CFRelease(devices);
        return;
    }

    CFSetGetValues(devices, values);
    for (CFIndex i = 0; i < count; ++i) {
        (void)add_pedal_device((IOHIDDeviceRef)values[i]);
    }

    free(values);
    CFRelease(devices);
}

static void wait_for_registration(void)
{
    while (g_pedals.registering) {
        (void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    }
}

bool platform_pedals_init(
    const char *config_path,
    Platform_Pedal_Role register_role,
    Platform_Pedal_Event_Fn handler,
    void *userdata
)
{
    platform_pedals_shutdown();

    g_pedals.config_path = config_path != NULL ? config_path : "stoin-pedals.json";
    g_pedals.handler = handler;
    g_pedals.userdata = userdata;
    g_pedals.register_role = register_role;
    g_pedals.registering = pedal_role_is_bindable(register_role);
    const bool registration_requested = g_pedals.registering;

    const bool loaded = load_pedal_config(g_pedals.config_path);
    if (!loaded && !g_pedals.registering) {
        return true;
    }

    if (g_pedals.registering) {
        printf("stoin: pedal registration armed for %s mode\n", pedal_role_label(register_role));
        printf("stoin: press the pedal to use as %s mode\n", pedal_role_label(register_role));
        puts("stoin: avoid typing, clicking, or pressing other HID controls until registration finishes");
    }

    g_pedals.manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (g_pedals.manager == NULL) {
        fputs("stoin: warning: failed to create IOHIDManager for pedals\n", stderr);
        return false;
    }

    g_pedals.run_loop = CFRunLoopGetCurrent();
    if (g_pedals.registering) {
        IOHIDManagerSetDeviceMatching(g_pedals.manager, NULL);
    } else if (!set_runtime_device_matching(g_pedals.manager)) {
        fputs("stoin: warning: failed to create pedal HID device matching set\n", stderr);
        platform_pedals_shutdown();
        return false;
    }
    IOHIDManagerRegisterDeviceMatchingCallback(g_pedals.manager, pedal_device_matching_callback, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(g_pedals.manager, pedal_device_removal_callback, NULL);
    IOHIDManagerScheduleWithRunLoop(g_pedals.manager, g_pedals.run_loop, kCFRunLoopCommonModes);

    IOOptionBits manager_open_options = g_pedals.registering
        ? kIOHIDOptionsTypeNone
        : kIOHIDOptionsTypeSeizeDevice;
    IOReturn open_result = IOHIDManagerOpen(g_pedals.manager, manager_open_options);
    if (open_result != kIOReturnSuccess && manager_open_options != kIOHIDOptionsTypeNone) {
        const IOReturn exclusive_result = open_result;
        fprintf(stderr,
            "stoin: warning: failed to exclusively open registered pedal HID devices (0x%x); falling back to shared access\n",
            (unsigned int)exclusive_result);
        manager_open_options = kIOHIDOptionsTypeNone;
        open_result = IOHIDManagerOpen(g_pedals.manager, manager_open_options);
    }
    if (open_result != kIOReturnSuccess) {
        fprintf(stderr, "stoin: warning: failed to open IOHIDManager for pedals (0x%x)\n", (unsigned int)open_result);
        platform_pedals_shutdown();
        return false;
    }
    g_pedals.manager_opened_devices = !g_pedals.registering;
    g_pedals.manager_seized_devices =
        !g_pedals.registering && manager_open_options == kIOHIDOptionsTypeSeizeDevice;
    if (g_pedals.manager_seized_devices) {
        puts("stoin: registered pedal HID devices opened exclusively by manager");
    }

    g_pedals.started = true;
    register_existing_devices(g_pedals.manager);

    if (registration_requested) {
        wait_for_registration();
        platform_pedals_shutdown();
        return platform_pedals_init(config_path, PLATFORM_PEDAL_ROLE_NONE, handler, userdata);
    }
    return true;
}

void platform_pedals_poll(void)
{
    if (!g_pedals.started) {
        return;
    }

    for (int i = 0; i < 16; ++i) {
        const SInt32 result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true);
        if (result != kCFRunLoopRunHandledSource) {
            break;
        }
    }
}

void platform_pedals_shutdown(void)
{
    for (size_t i = 0; i < MAC_PEDAL_MAX_DEVICES; ++i) {
        Mac_Pedal_Device *device = &g_pedals.devices[i];
        if (!device->used || device->device == NULL) {
            continue;
        }
        release_bindings_for_device(device);
        IOHIDDeviceRegisterInputValueCallback(device->device, NULL, NULL);
        CFRunLoopRef run_loop = g_pedals.run_loop != NULL ? g_pedals.run_loop : CFRunLoopGetCurrent();
        IOHIDDeviceUnscheduleFromRunLoop(device->device, run_loop, kCFRunLoopCommonModes);
        if (!g_pedals.manager_opened_devices) {
            IOHIDDeviceClose(device->device, kIOHIDOptionsTypeNone);
        }
        CFRelease(device->device);
    }

    if (g_pedals.manager != NULL) {
        IOHIDManagerRegisterDeviceMatchingCallback(g_pedals.manager, NULL, NULL);
        IOHIDManagerRegisterDeviceRemovalCallback(g_pedals.manager, NULL, NULL);
        if (g_pedals.run_loop != NULL) {
            IOHIDManagerUnscheduleFromRunLoop(g_pedals.manager, g_pedals.run_loop, kCFRunLoopCommonModes);
        }
        IOHIDManagerClose(g_pedals.manager, kIOHIDOptionsTypeNone);
        CFRelease(g_pedals.manager);
    }

    memset(&g_pedals, 0, sizeof(g_pedals));
}
