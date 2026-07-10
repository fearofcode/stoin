#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H

#include <stdbool.h>
#include <stddef.h>

bool text_append_char(char **out, char c);
bool text_append_range(char **out, const char *start, size_t length);
bool text_append_cstring(char **out, const char *s);
bool text_prepend_range(char **out, const char *start, size_t length);
bool ascii_range_equals_ignore_case(const char *a, size_t a_length, const char *b);
bool ascii_range_starts_with_ignore_case(const char *s, size_t length, const char *prefix);
char *copy_trimmed_range(const char *start, size_t length);
size_t utf8_codepoint_length(const char *s, const char *end);
size_t common_utf8_prefix_bytes(const char *a, const char *b);

#endif
