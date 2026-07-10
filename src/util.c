#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_entire_file(const char *path, size_t *out_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    const long end = ftell(file);
    if (end < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *data = malloc((size_t)end + 1);
    if (data == NULL) {
        fclose(file);
        return NULL;
    }

    const size_t bytes_read = fread(data, 1, (size_t)end, file);
    fclose(file);

    if (bytes_read != (size_t)end) {
        free(data);
        return NULL;
    }

    data[bytes_read] = '\0';
    if (out_size != NULL) {
        *out_size = bytes_read;
    }
    return data;
}

char *copy_range(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

char *copy_cstring(const char *s)
{
    return copy_range(s, strlen(s));
}
