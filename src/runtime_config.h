#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <stdbool.h>

typedef struct Runtime_Config {
    char **dictionary_paths;
    bool *dictionary_enabled;
    char *word_list_path;
    char *phrasing_path;
} Runtime_Config;

void runtime_config_clear_dictionaries(Runtime_Config *config);
bool runtime_config_add_dictionary_enabled(Runtime_Config *config, const char *path, bool enabled);
bool runtime_config_add_dictionary(Runtime_Config *config, const char *path);
bool runtime_config_set_word_list(Runtime_Config *config, const char *path);
bool runtime_config_set_phrasing(Runtime_Config *config, const char *path);
bool runtime_config_load(Runtime_Config *config, const char *path, bool missing_ok);
void runtime_config_destroy(Runtime_Config *config);

#endif
