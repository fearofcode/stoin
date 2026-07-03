#include "runtime_config.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"
#include "../third_party/cjson/cJSON.h"

static void print_json_parse_error(const char *label, const char *path, const char *file, const char *parse_end)
{
    size_t line = 1;
    size_t column = 1;
    if (file != NULL && parse_end != NULL) {
        for (const char *p = file; p < parse_end && *p != '\0'; ++p) {
            if (*p == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
    }
    fprintf(stderr, "stoin: %s '%s' has invalid JSON near line %zu, column %zu\n", label, path, line, column);
}

void runtime_config_clear_dictionaries(Runtime_Config *config)
{
    if (config == NULL) {
        return;
    }
    for (size_t i = 0; i < arrlenu(config->dictionary_paths); ++i) {
        free(config->dictionary_paths[i]);
    }
    arrfree(config->dictionary_paths);
    config->dictionary_paths = NULL;
    arrfree(config->dictionary_enabled);
    config->dictionary_enabled = NULL;
}

bool runtime_config_add_dictionary_enabled(Runtime_Config *config, const char *path, bool enabled)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    arrput(config->dictionary_paths, copy);
    arrput(config->dictionary_enabled, enabled);
    return true;
}

bool runtime_config_add_dictionary(Runtime_Config *config, const char *path)
{
    return runtime_config_add_dictionary_enabled(config, path, true);
}

bool runtime_config_set_word_list(Runtime_Config *config, const char *path)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    free(config->word_list_path);
    config->word_list_path = copy;
    return true;
}

bool runtime_config_set_phrasing(Runtime_Config *config, const char *path)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    free(config->phrasing_path);
    config->phrasing_path = copy;
    return true;
}

void runtime_config_destroy(Runtime_Config *config)
{
    if (config == NULL) {
        return;
    }
    runtime_config_clear_dictionaries(config);
    free(config->word_list_path);
    free(config->phrasing_path);
    memset(config, 0, sizeof(*config));
}

static bool runtime_config_parse_dictionary_object(
    Runtime_Config *config,
    const cJSON *item,
    const char *config_path,
    size_t index
)
{
    if (!cJSON_IsObject(item)) {
        fprintf(stderr, "stoin: config '%s' dictionaries[%zu] must be a string or object\n", config_path, index);
        return false;
    }

    const cJSON *path = cJSON_GetObjectItemCaseSensitive(item, "path");
    if (!cJSON_IsString(path) || path->valuestring == NULL) {
        fprintf(stderr, "stoin: config '%s' dictionaries[%zu].path must be a string\n", config_path, index);
        return false;
    }

    bool enabled = true;
    const cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(item, "enabled");
    if (enabled_item != NULL) {
        if (!cJSON_IsBool(enabled_item)) {
            fprintf(stderr, "stoin: config '%s' dictionaries[%zu].enabled must be true or false\n", config_path, index);
            return false;
        }
        enabled = cJSON_IsTrue(enabled_item);
    }

    return runtime_config_add_dictionary_enabled(config, path->valuestring, enabled);
}

static bool runtime_config_parse_dictionary_array(
    Runtime_Config *config,
    const cJSON *array,
    const char *config_path
)
{
    if (!cJSON_IsArray(array)) {
        fprintf(stderr, "stoin: config '%s' dictionaries must be an array\n", config_path);
        return false;
    }

    runtime_config_clear_dictionaries(config);
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, array) {
        bool ok = false;
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            ok = runtime_config_add_dictionary(config, item->valuestring);
        } else {
            ok = runtime_config_parse_dictionary_object(config, item, config_path, index);
        }
        if (!ok) {
            return false;
        }
        ++index;
    }
    return true;
}

static bool runtime_config_parse_string_field(
    Runtime_Config *config,
    const cJSON *root,
    const char *config_path,
    const char *field,
    bool (*setter)(Runtime_Config *, const char *)
)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (item == NULL) {
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        fprintf(stderr, "stoin: config '%s' %s must be a string\n", config_path, field);
        return false;
    }
    return setter(config, item->valuestring);
}

bool runtime_config_load(Runtime_Config *config, const char *path, bool missing_ok)
{
    size_t size = 0;
    char *file = read_entire_file(path, &size);
    if (file == NULL) {
        if (missing_ok) {
            return true;
        }
        fprintf(stderr, "stoin: failed to read config '%s'\n", path);
        return false;
    }

    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(file, size, &parse_end, 0);
    if (root == NULL) {
        print_json_parse_error("config", path, file, parse_end);
        free(file);
        return false;
    }

    bool ok = true;
    if (!cJSON_IsObject(root)) {
        fprintf(stderr, "stoin: config '%s' is not a JSON object\n", path);
        ok = false;
    }

    if (ok) {
        ok = runtime_config_parse_string_field(config, root, path, "word_list", runtime_config_set_word_list)
            && runtime_config_parse_string_field(config, root, path, "phrasing", runtime_config_set_phrasing);
    }

    if (ok) {
        const cJSON *dictionaries = cJSON_GetObjectItemCaseSensitive(root, "dictionaries");
        if (dictionaries != NULL) {
            ok = runtime_config_parse_dictionary_array(config, dictionaries, path);
        }
    }

    cJSON_Delete(root);
    free(file);
    return ok;
}
