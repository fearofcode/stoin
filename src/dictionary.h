#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Dictionary_Entry {
    char *key;
    char *value;
} Dictionary_Entry;

typedef struct Dictionary {
    Dictionary_Entry *entries;
    size_t longest_key;
} Dictionary;

bool dictionary_load(Dictionary *dictionary, const char *path);
bool dictionary_load_many(Dictionary *dictionary, const char *const *paths, size_t path_count);
void dictionary_destroy(Dictionary *dictionary);
size_t dictionary_count(const Dictionary *dictionary);
size_t dictionary_longest_key(const Dictionary *dictionary);
const char *dictionary_lookup_bits(const Dictionary *dictionary, uint64_t bits);
const char *dictionary_lookup_strokes(const Dictionary *dictionary, const uint64_t *strokes, size_t stroke_count);
bool dictionary_lookup_stroke(const Dictionary *dictionary, const char *stroke, const char **out_translation);
bool dictionary_find_translation_outline(
    const Dictionary *dictionary,
    const char *translation,
    const char *exclude_outline,
    size_t max_stroke_count,
    char *out_outline,
    size_t out_outline_size
);
bool dictionary_dump_json(const Dictionary *dictionary, const char *path);

#endif
