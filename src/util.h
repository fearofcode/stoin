#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

char *read_entire_file(const char *path, size_t *out_size);
char *copy_range(const char *start, size_t length);
char *copy_cstring(const char *s);

#endif
