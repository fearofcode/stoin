#include "runtime_config.h"

#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

static const char *skip_json_ws(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static bool parse_json_string(const char **cursor, char **out_string)
{
    const char *p = *cursor;
    if (*p != '"') {
        return false;
    }
    ++p;

    char *result = NULL;
    while (*p != '\0' && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            c = (unsigned char)*p++;
            switch (c) {
            case '"': arrput(result, '"'); break;
            case '\\': arrput(result, '\\'); break;
            case '/': arrput(result, '/'); break;
            case 'b': arrput(result, '\b'); break;
            case 'f': arrput(result, '\f'); break;
            case 'n': arrput(result, '\n'); break;
            case 'r': arrput(result, '\r'); break;
            case 't': arrput(result, '\t'); break;
            default:
                arrfree(result);
                return false;
            }
        } else {
            arrput(result, (char)c);
        }
    }

    if (*p != '"') {
        arrfree(result);
        return false;
    }
    ++p;

    arrput(result, '\0');
    *cursor = p;
    *out_string = result;
    return true;
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

static bool parse_json_bool(const char **cursor, bool *out_value)
{
    const char *p = skip_json_ws(*cursor);
    if (strncmp(p, "true", 4) == 0) {
        *cursor = p + 4;
        *out_value = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0) {
        *cursor = p + 5;
        *out_value = false;
        return true;
    }
    return false;
}

void runtime_config_destroy(Runtime_Config *config)
{
    if (config == NULL) {
        return;
    }
    runtime_config_clear_dictionaries(config);
    free(config->word_list_path);
    memset(config, 0, sizeof(*config));
}

static bool runtime_config_parse_dictionary_object(Runtime_Config *config, const char **cursor)
{
    const char *p = skip_json_ws(*cursor);
    if (*p != '{') {
        return false;
    }
    ++p;

    char *path = NULL;
    bool enabled = true;
    bool parsed_ok = false;

    while (true) {
        p = skip_json_ws(p);
        if (*p == '}') {
            parsed_ok = path != NULL;
            ++p;
            break;
        }

        char *key = NULL;
        if (!parse_json_string(&p, &key)) {
            break;
        }
        p = skip_json_ws(p);
        if (*p != ':') {
            arrfree(key);
            break;
        }
        ++p;

        bool value_ok = false;
        if (strcmp(key, "path") == 0) {
            arrfree(path);
            path = NULL;
            p = skip_json_ws(p);
            value_ok = parse_json_string(&p, &path);
        } else if (strcmp(key, "enabled") == 0) {
            value_ok = parse_json_bool(&p, &enabled);
        }
        arrfree(key);
        if (!value_ok) {
            break;
        }

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            parsed_ok = path != NULL;
            ++p;
            break;
        }
        break;
    }

    bool ok = parsed_ok && runtime_config_add_dictionary_enabled(config, path, enabled);
    arrfree(path);
    if (!ok) {
        return false;
    }
    *cursor = p;
    return true;
}

static bool runtime_config_parse_dictionary_array(Runtime_Config *config, const char **cursor)
{
    const char *p = skip_json_ws(*cursor);
    if (*p != '[') {
        return false;
    }
    ++p;

    runtime_config_clear_dictionaries(config);
    while (true) {
        p = skip_json_ws(p);
        if (*p == ']') {
            *cursor = p + 1;
            return true;
        }

        if (*p == '{') {
            if (!runtime_config_parse_dictionary_object(config, &p)) {
                return false;
            }
        } else {
            char *path = NULL;
            if (!parse_json_string(&p, &path)) {
                return false;
            }
            const bool ok = runtime_config_add_dictionary(config, path);
            arrfree(path);
            if (!ok) {
                return false;
            }
        }

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == ']') {
            *cursor = p + 1;
            return true;
        }
        return false;
    }
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

    const char *p = skip_json_ws(file);
    if (*p != '{') {
        fprintf(stderr, "stoin: config '%s' is not a JSON object\n", path);
        free(file);
        return false;
    }
    ++p;

    bool parsed_ok = false;
    while (true) {
        p = skip_json_ws(p);
        if (*p == '}') {
            parsed_ok = true;
            break;
        }

        char *key = NULL;
        if (!parse_json_string(&p, &key)) {
            break;
        }
        p = skip_json_ws(p);
        if (*p != ':') {
            arrfree(key);
            break;
        }
        ++p;

        bool value_ok = false;
        if (strcmp(key, "word_list") == 0) {
            char *word_list = NULL;
            p = skip_json_ws(p);
            value_ok = parse_json_string(&p, &word_list)
                && runtime_config_set_word_list(config, word_list);
            arrfree(word_list);
        } else if (strcmp(key, "dictionaries") == 0) {
            value_ok = runtime_config_parse_dictionary_array(config, &p);
        }
        arrfree(key);
        if (!value_ok) {
            break;
        }

        p = skip_json_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            parsed_ok = true;
            break;
        }
        break;
    }

    free(file);
    if (!parsed_ok) {
        fprintf(stderr, "stoin: config '%s' could not be parsed\n", path);
        return false;
    }
    return true;
}
