#ifndef ORTHOGRAPHY_H
#define ORTHOGRAPHY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Orthography_Word {
    char *key;
    char value;
} Orthography_Word;

typedef struct Orthography {
    Orthography_Word *words;
} Orthography;

bool orthography_load(Orthography *orthography, const char *path);
void orthography_destroy(Orthography *orthography);
size_t orthography_word_count(const Orthography *orthography);
bool orthography_apply(
    const Orthography *orthography,
    const char *word,
    const char *suffix,
    char **out_result
);

#endif
