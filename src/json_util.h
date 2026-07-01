#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

const char *json_find_object(const char *json, const char *name, const char **out_end);
bool json_parse_uint_field(const char *start, const char *end, const char *name, uint32_t *out_value);
bool json_parse_string_field(const char *start, const char *end, const char *name, char **out_value);
void json_write_string(FILE *file, const char *value);

#endif
