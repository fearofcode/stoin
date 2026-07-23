#include "platform_windows_internal.h"

#include <setupapi.h>

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOWS_SERIAL_MAX_CANDIDATES 256

typedef struct Windows_Serial_Candidate {
    char name[16];
    char path[PLATFORM_SERIAL_PATH_MAX];
    char friendly_name[256];
    char description[256];
    char manufacturer[256];
    int score;
    unsigned int number;
    bool exists;
} Windows_Serial_Candidate;

struct Platform_Serial_Port {
    HANDLE handle;
    char port_path[PLATFORM_SERIAL_PATH_MAX];
    bool had_error;
};

static bool add_serial_path(
    char out_paths[][PLATFORM_SERIAL_PATH_MAX],
    size_t max_paths,
    size_t *path_count,
    const char *path
)
{
    if (out_paths == NULL || path_count == NULL || path == NULL || *path_count >= max_paths) {
        return false;
    }

    for (size_t i = 0; i < *path_count; ++i) {
        if (strcmp(out_paths[i], path) == 0) {
            return false;
        }
    }

    const int written = snprintf(out_paths[*path_count], PLATFORM_SERIAL_PATH_MAX, "%s", path);
    if (written <= 0 || (size_t)written >= PLATFORM_SERIAL_PATH_MAX) {
        return false;
    }

    ++*path_count;
    return true;
}
static bool copy_registry_string_property(
    HDEVINFO device_info,
    SP_DEVINFO_DATA *device_data,
    DWORD property,
    char *out_value,
    size_t out_size
)
{
    if (out_value == NULL || out_size == 0) {
        return false;
    }
    out_value[0] = '\0';

    DWORD type = 0;
    DWORD required_size = 0;
    if (!SetupDiGetDeviceRegistryPropertyA(
            device_info,
            device_data,
            property,
            &type,
            (PBYTE)out_value,
            (DWORD)out_size,
            &required_size)
        || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        out_value[0] = '\0';
        return false;
    }
    out_value[out_size - 1] = '\0';
    return true;
}

static bool copy_port_name_from_device_registry(
    HDEVINFO device_info,
    SP_DEVINFO_DATA *device_data,
    char *out_name,
    size_t out_size
)
{
    if (out_name == NULL || out_size == 0) {
        return false;
    }
    out_name[0] = '\0';

    HKEY key = SetupDiOpenDevRegKey(
        device_info,
        device_data,
        DICS_FLAG_GLOBAL,
        0,
        DIREG_DEV,
        KEY_QUERY_VALUE);
    if (key == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD type = 0;
    DWORD size = (DWORD)out_size;
    const LONG result = RegQueryValueExA(key, "PortName", NULL, &type, (LPBYTE)out_name, &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        out_name[0] = '\0';
        return false;
    }
    out_name[out_size - 1] = '\0';
    return true;
}

static bool windows_ascii_contains_ci(const char *text, const char *needle)
{
    if (text == NULL || needle == NULL || *needle == '\0') {
        return false;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        const char *a = p;
        const char *b = needle;
        while (*a != '\0'
            && *b != '\0'
            && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return true;
        }
    }
    return false;
}

static unsigned int serial_port_number_from_name(const char *name)
{
    if (name == NULL
        || (name[0] != 'C' && name[0] != 'c')
        || (name[1] != 'O' && name[1] != 'o')
        || (name[2] != 'M' && name[2] != 'm')
        || !isdigit((unsigned char)name[3])) {
        return 0;
    }

    char *end = NULL;
    const unsigned long parsed = strtoul(name + 3, &end, 10);
    if (end == name + 3 || *end != '\0' || parsed == 0 || parsed > 256) {
        return 0;
    }
    return (unsigned int)parsed;
}

static int windows_serial_metadata_score(const Windows_Serial_Candidate *candidate)
{
    if (candidate == NULL) {
        return 0;
    }

    int score = 0;
    const char *fields[] = {
        candidate->friendly_name,
        candidate->description,
        candidate->manufacturer,
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        const char *field = fields[i];
        if (windows_ascii_contains_ci(field, "usb serial port")) {
            score += 120;
        }
        if (windows_ascii_contains_ci(field, "usb serial")) {
            score += 80;
        }
        if (windows_ascii_contains_ci(field, "ftdi")) {
            score += 70;
        }
        if (windows_ascii_contains_ci(field, "usb")) {
            score += 30;
        }
        if (windows_ascii_contains_ci(field, "intel")
            || windows_ascii_contains_ci(field, "active management")
            || windows_ascii_contains_ci(field, "amt")) {
            score -= 100;
        }
    }
    return score;
}

static Windows_Serial_Candidate *find_serial_candidate(
    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES],
    size_t candidate_count,
    const char *name
)
{
    for (size_t i = 0; i < candidate_count; ++i) {
        if (strcmp(candidates[i].name, name) == 0) {
            return &candidates[i];
        }
    }
    return NULL;
}

static Windows_Serial_Candidate *add_or_find_serial_candidate(
    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES],
    size_t *candidate_count,
    const char *name
)
{
    if (candidate_count == NULL || name == NULL) {
        return NULL;
    }

    Windows_Serial_Candidate *existing = find_serial_candidate(candidates, *candidate_count, name);
    if (existing != NULL) {
        return existing;
    }
    if (*candidate_count >= WINDOWS_SERIAL_MAX_CANDIDATES) {
        return NULL;
    }

    Windows_Serial_Candidate *candidate = &candidates[(*candidate_count)++];
    memset(candidate, 0, sizeof(*candidate));
    const int name_written = snprintf(candidate->name, sizeof(candidate->name), "%s", name);
    const int path_written = snprintf(candidate->path, sizeof(candidate->path), "\\\\.\\%s", name);
    if (name_written <= 0
        || (size_t)name_written >= sizeof(candidate->name)
        || path_written <= 0
        || (size_t)path_written >= sizeof(candidate->path)) {
        --*candidate_count;
        return NULL;
    }
    candidate->number = serial_port_number_from_name(name);
    candidate->exists = true;
    return candidate;
}

static void add_setupapi_serial_candidates(
    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES],
    size_t *candidate_count
)
{
    GUID ports_guid = {0};
    DWORD guid_count = 0;
    if (!SetupDiClassGuidsFromNameA("Ports", &ports_guid, 1, &guid_count) || guid_count == 0) {
        return;
    }

    HDEVINFO device_info = SetupDiGetClassDevsA(&ports_guid, NULL, NULL, DIGCF_PRESENT);
    if (device_info == INVALID_HANDLE_VALUE) {
        return;
    }

    for (DWORD index = 0; ; ++index) {
        SP_DEVINFO_DATA device_data;
        memset(&device_data, 0, sizeof(device_data));
        device_data.cbSize = sizeof(device_data);
        if (!SetupDiEnumDeviceInfo(device_info, index, &device_data)) {
            break;
        }

        char port_name[16] = {0};
        if (!copy_port_name_from_device_registry(device_info, &device_data, port_name, sizeof(port_name))
            || serial_port_number_from_name(port_name) == 0) {
            continue;
        }

        Windows_Serial_Candidate *candidate =
            add_or_find_serial_candidate(candidates, candidate_count, port_name);
        if (candidate == NULL) {
            continue;
        }

        (void)copy_registry_string_property(
            device_info,
            &device_data,
            SPDRP_FRIENDLYNAME,
            candidate->friendly_name,
            sizeof(candidate->friendly_name));
        (void)copy_registry_string_property(
            device_info,
            &device_data,
            SPDRP_DEVICEDESC,
            candidate->description,
            sizeof(candidate->description));
        (void)copy_registry_string_property(
            device_info,
            &device_data,
            SPDRP_MFG,
            candidate->manufacturer,
            sizeof(candidate->manufacturer));
        candidate->score = windows_serial_metadata_score(candidate);
    }

    SetupDiDestroyDeviceInfoList(device_info);
}

static int compare_serial_candidates(const void *a, const void *b)
{
    const Windows_Serial_Candidate *left = a;
    const Windows_Serial_Candidate *right = b;
    if (left->score != right->score) {
        return right->score - left->score;
    }
    if (left->number < right->number) {
        return -1;
    }
    if (left->number > right->number) {
        return 1;
    }
    return strcmp(left->name, right->name);
}

size_t platform_find_serial_devices(char out_paths[][PLATFORM_SERIAL_PATH_MAX], size_t max_paths)
{
    if (out_paths == NULL || max_paths == 0) {
        return 0;
    }

    Windows_Serial_Candidate candidates[WINDOWS_SERIAL_MAX_CANDIDATES];
    memset(candidates, 0, sizeof(candidates));
    size_t candidate_count = 0;
    add_setupapi_serial_candidates(candidates, &candidate_count);

    for (unsigned int i = 1; i <= 256; ++i) {
        char name[16] = {0};
        char target[512] = {0};
        snprintf(name, sizeof(name), "COM%u", i);
        if (QueryDosDeviceA(name, target, sizeof(target)) != 0) {
            (void)add_or_find_serial_candidate(candidates, &candidate_count, name);
        }
    }

    qsort(candidates, candidate_count, sizeof(candidates[0]), compare_serial_candidates);

    size_t path_count = 0;
    for (size_t i = 0; i < candidate_count && path_count < max_paths; ++i) {
        if (candidates[i].exists) {
            (void)add_serial_path(out_paths, max_paths, &path_count, candidates[i].path);
        }
    }
    return path_count;
}

bool platform_find_serial_device(char *out_path, size_t out_size)
{
    if (out_path == NULL || out_size == 0) {
        return false;
    }

    char paths[1][PLATFORM_SERIAL_PATH_MAX] = {{0}};
    if (platform_find_serial_devices(paths, 1) == 0) {
        return false;
    }

    const int written = snprintf(out_path, out_size, "%s", paths[0]);
    return written > 0 && (size_t)written < out_size;
}

bool platform_find_gemini_pr_device(char *out_path, size_t out_size)
{
    return platform_find_serial_device(out_path, out_size);
}

static bool normalize_serial_path(const char *port_path, char *out_path, size_t out_size)
{
    if (port_path == NULL || out_path == NULL || out_size == 0) {
        return false;
    }

    if (strncmp(port_path, "\\\\.\\", 4) == 0) {
        const int written = snprintf(out_path, out_size, "%s", port_path);
        return written > 0 && (size_t)written < out_size;
    }

    if ((port_path[0] == 'C' || port_path[0] == 'c')
        && (port_path[1] == 'O' || port_path[1] == 'o')
        && (port_path[2] == 'M' || port_path[2] == 'm')) {
        const int written = snprintf(out_path, out_size, "\\\\.\\%s", port_path);
        return written > 0 && (size_t)written < out_size;
    }

    const int written = snprintf(out_path, out_size, "%s", port_path);
    return written > 0 && (size_t)written < out_size;
}

static bool configure_comm_timeouts(HANDLE handle, DWORD timeout_ms)
{
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;
    timeouts.WriteTotalTimeoutConstant = timeout_ms;
    return SetCommTimeouts(handle, &timeouts);
}

bool platform_serial_open(Platform_Serial_Port **out_port, const char *port_path, int baud_rate)
{
    if (out_port == NULL) {
        return false;
    }
    *out_port = NULL;

    char normalized_path[PLATFORM_SERIAL_PATH_MAX] = {0};
    if (!normalize_serial_path(port_path, normalized_path, sizeof(normalized_path))) {
        errno = port_path == NULL ? ENODEV : ENAMETOOLONG;
        return false;
    }

    HANDLE handle = CreateFileA(
        normalized_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (handle == INVALID_HANDLE_VALUE) {
        errno = ENODEV;
        return false;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        errno = EIO;
        return false;
    }

    dcb.BaudRate = (DWORD)baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(handle, &dcb)
        || !SetupComm(handle, 4096, 4096)
        || !configure_comm_timeouts(handle, 100)
        || !PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        CloseHandle(handle);
        errno = EIO;
        return false;
    }

    Platform_Serial_Port *port = calloc(1, sizeof(*port));
    if (port == NULL) {
        CloseHandle(handle);
        errno = ENOMEM;
        return false;
    }

    port->handle = handle;
    snprintf(port->port_path, sizeof(port->port_path), "%s", normalized_path);
    *out_port = port;
    return true;
}

void platform_serial_close(Platform_Serial_Port *port)
{
    if (port == NULL) {
        return;
    }
    if (port->handle != NULL && port->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(port->handle);
        port->handle = INVALID_HANDLE_VALUE;
    }
    free(port);
}

const char *platform_serial_port_path(const Platform_Serial_Port *port)
{
    return port == NULL ? "" : port->port_path;
}

bool platform_serial_had_error(const Platform_Serial_Port *port)
{
    return port != NULL && port->had_error;
}

void platform_serial_flush(Platform_Serial_Port *port)
{
    if (port == NULL || port->handle == NULL || port->handle == INVALID_HANDLE_VALUE) {
        return;
    }
    PurgeComm(port->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

Platform_Serial_Read_Result platform_serial_read_byte(
    Platform_Serial_Port *port,
    uint8_t *out_byte,
    unsigned int timeout_ms
)
{
    if (port == NULL || out_byte == NULL || port->handle == NULL || port->handle == INVALID_HANDLE_VALUE) {
        if (port != NULL) {
            port->had_error = true;
        }
        errno = EBADF;
        return PLATFORM_SERIAL_READ_ERROR;
    }

    if (!configure_comm_timeouts(port->handle, timeout_ms)) {
        port->had_error = true;
        return PLATFORM_SERIAL_READ_ERROR;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(port->handle, out_byte, 1, &bytes_read, NULL)) {
        port->had_error = true;
        return PLATFORM_SERIAL_READ_ERROR;
    }
    if (bytes_read == 1) {
        return PLATFORM_SERIAL_READ_BYTE;
    }
    return PLATFORM_SERIAL_READ_NONE;
}

bool platform_serial_write_all(
    Platform_Serial_Port *port,
    const uint8_t *bytes,
    size_t byte_count,
    unsigned int timeout_ms
)
{
    if (port == NULL || bytes == NULL || port->handle == NULL || port->handle == INVALID_HANDLE_VALUE) {
        if (port != NULL) {
            port->had_error = true;
        }
        errno = EBADF;
        return false;
    }

    if (!configure_comm_timeouts(port->handle, timeout_ms)) {
        port->had_error = true;
        return false;
    }

    size_t written_count = 0;
    while (written_count < byte_count) {
        const size_t remaining = byte_count - written_count;
        const DWORD request_count = remaining > UINT32_MAX ? UINT32_MAX : (DWORD)remaining;
        DWORD bytes_written = 0;
        if (!WriteFile(port->handle, bytes + written_count, request_count, &bytes_written, NULL)) {
            port->had_error = true;
            return false;
        }
        if (bytes_written == 0) {
            errno = ETIMEDOUT;
            return false;
        }
        written_count += bytes_written;
    }

    return true;
}
