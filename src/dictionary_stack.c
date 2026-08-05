#include "dictionary_stack.h"

#include "file_stability.h"
#include "text_util.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../third_party/stb_ds.h"

static bool file_stamps_equal(Platform_File_Stamp a, Platform_File_Stamp b)
{
    return a.exists == b.exists
        && a.size == b.size
        && a.modified_time_ns == b.modified_time_ns;
}

static void clear_dictionary_paths(Dictionary_Stack *stack)
{
    if (stack == NULL) {
        return;
    }
    for (size_t i = 0; i < arrlenu(stack->paths); ++i) {
        free(stack->paths[i]);
    }
    arrfree(stack->paths);
    stack->paths = NULL;
    arrfree(stack->enabled);
    stack->enabled = NULL;
    arrfree(stack->stamps);
    stack->stamps = NULL;
}

static bool add_dictionary_path(Dictionary_Stack *stack, const char *path, bool enabled)
{
    char *copy = copy_cstring(path);
    if (copy == NULL) {
        return false;
    }
    arrput(stack->paths, copy);
    arrput(stack->enabled, enabled);
    return true;
}

bool dictionary_stack_set_paths(
    Dictionary_Stack *stack,
    const char *dictionary_path,
    const char *const *dictionary_paths,
    const bool *dictionary_enabled,
    size_t dictionary_path_count
)
{
    if (stack == NULL) {
        return false;
    }

    clear_dictionary_paths(stack);
    if (dictionary_path_count > 0) {
        if (dictionary_paths == NULL) {
            return false;
        }
        for (size_t i = 0; i < dictionary_path_count; ++i) {
            const bool enabled = dictionary_enabled == NULL || dictionary_enabled[i];
            if (dictionary_paths[i] == NULL || !add_dictionary_path(stack, dictionary_paths[i], enabled)) {
                clear_dictionary_paths(stack);
                return false;
            }
        }
    } else {
        if (dictionary_path == NULL || !add_dictionary_path(stack, dictionary_path, true)) {
            clear_dictionary_paths(stack);
            return false;
        }
    }

    return arrlenu(stack->paths) > 0;
}

static bool refresh_dictionary_stamps(Dictionary_Stack *stack)
{
    if (stack == NULL) {
        return false;
    }

    arrsetlen(stack->stamps, arrlenu(stack->paths));
    for (size_t i = 0; i < arrlenu(stack->paths); ++i) {
        Platform_File_Stamp stamp = {0};
        if (!platform_file_stamp(stack->paths[i], &stamp)) {
            arrsetlen(stack->stamps, 0);
            return false;
        }
        stack->stamps[i] = stamp;
    }
    return true;
}

static bool dictionary_files_changed(Dictionary_Stack *stack, bool *out_changed)
{
    if (stack == NULL || out_changed == NULL) {
        return false;
    }

    *out_changed = false;
    if (arrlenu(stack->stamps) != arrlenu(stack->paths)) {
        *out_changed = true;
        return true;
    }

    for (size_t i = 0; i < arrlenu(stack->paths); ++i) {
        Platform_File_Stamp stamp = {0};
        if (!platform_file_stamp(stack->paths[i], &stamp)) {
            return false;
        }
        if (!file_stamps_equal(stamp, stack->stamps[i])) {
            *out_changed = true;
            return true;
        }
    }

    return true;
}

static bool load_dictionary_from_paths(
    Dictionary *dictionary,
    char *const *paths,
    const bool *enabled,
    size_t path_count
)
{
    if (dictionary == NULL || paths == NULL || enabled == NULL || path_count == 0) {
        return false;
    }

    bool loaded_any = false;
    for (size_t i = 0; i < path_count; ++i) {
        if (!enabled[i]) {
            continue;
        }
        if (paths[i] == NULL || !dictionary_load(dictionary, paths[i])) {
            return false;
        }
        loaded_any = true;
    }

    if (!loaded_any) {
        sh_new_strdup(dictionary->entries);
    }
    return true;
}

bool dictionary_stack_load(Dictionary_Stack *stack)
{
    if (stack == NULL || arrlenu(stack->paths) == 0) {
        return false;
    }

    if (!load_dictionary_from_paths(&stack->dictionary, stack->paths, stack->enabled, arrlenu(stack->paths))) {
        return false;
    }
    if (!refresh_dictionary_stamps(stack)) {
        fputs("stoin: warning: failed to capture dictionary file stamps; hot reload may not work\n", stderr);
    }
    ++stack->revision;
    return true;
}

bool dictionary_stack_reload(Dictionary_Stack *stack)
{
    if (stack == NULL || arrlenu(stack->paths) == 0) {
        return false;
    }

    Dictionary next = {0};
    if (!load_dictionary_from_paths(&next, stack->paths, stack->enabled, arrlenu(stack->paths))) {
        dictionary_destroy(&next);
        (void)refresh_dictionary_stamps(stack);
        if (!stack->reload_error_reported) {
            fputs("stoin: dictionary changed but reload failed; keeping previous dictionary\n", stderr);
            stack->reload_error_reported = true;
        }
        return false;
    }

    dictionary_destroy(&stack->dictionary);
    stack->dictionary = next;
    if (!refresh_dictionary_stamps(stack)) {
        fputs("stoin: warning: reloaded dictionary, but failed to refresh dictionary file stamps\n", stderr);
    }

    stack->reload_error_reported = false;
    ++stack->revision;
    fprintf(stderr, "stoin: reloaded %zu dictionary entries\n", dictionary_count(&stack->dictionary));
    return true;
}

bool dictionary_stack_reload_if_changed(Dictionary_Stack *stack)
{
    if (stack == NULL) {
        return false;
    }

    bool changed = false;
    if (!dictionary_files_changed(stack, &changed)) {
        if (!stack->reload_error_reported) {
            fputs("stoin: failed to check dictionary files for changes\n", stderr);
            stack->reload_error_reported = true;
        }
        return false;
    }

    if (!changed) {
        return true;
    }

    bool stable = false;
    if (!file_paths_wait_until_stable(
            (const char *const *)stack->paths,
            arrlenu(stack->paths),
            &stable)) {
        if (!stack->reload_error_reported) {
            fputs("stoin: failed to debounce dictionary file changes\n", stderr);
            stack->reload_error_reported = true;
        }
        return false;
    }
    if (!stable) {
        return true;
    }

    stack->reload_error_reported = false;
    return dictionary_stack_reload(stack);
}

static const char *path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static bool path_matches_dictionary_selection(const char *path, const char *selection)
{
    if (path == NULL || selection == NULL) {
        return false;
    }

    const size_t path_length = strlen(path);
    const size_t selection_length = strlen(selection);
    if (selection_length == 0 || selection_length > path_length) {
        return false;
    }

    if (strcmp(path + path_length - selection_length, selection) == 0
        && (selection_length == path_length || path[path_length - selection_length - 1] == '/')) {
        return true;
    }

    const char *base = path_basename(path);
    const char *selection_base = path_basename(selection);
    if (strcmp(base, selection_base) == 0) {
        return true;
    }
    if (strncmp(base, "lapwing-", 8) == 0 && strcmp(base + 8, selection_base) == 0) {
        return true;
    }
    return false;
}

static bool toggle_dictionary_selection(Dictionary_Stack *stack, char toggle, const char *selection)
{
    size_t match = SIZE_MAX;
    size_t match_length = SIZE_MAX;
    for (size_t i = 0; i < arrlenu(stack->paths); ++i) {
        if (!path_matches_dictionary_selection(stack->paths[i], selection)) {
            continue;
        }
        const size_t length = strlen(stack->paths[i]);
        if (length < match_length) {
            match = i;
            match_length = length;
        }
    }

    if (match == SIZE_MAX) {
        fprintf(stderr, "stoin: dictionary toggle could not find '%s'\n", selection);
        return true;
    }

    const bool old_enabled = stack->enabled[match];
    bool new_enabled = old_enabled;
    if (toggle == '+') {
        new_enabled = true;
    } else if (toggle == '-') {
        new_enabled = false;
    } else if (toggle == '!') {
        new_enabled = !old_enabled;
    } else {
        fprintf(stderr, "stoin: invalid dictionary toggle '%c%s'\n", toggle, selection);
        return true;
    }

    if (new_enabled == old_enabled) {
        return true;
    }

    stack->enabled[match] = new_enabled;
    if (!dictionary_stack_reload(stack)) {
        stack->enabled[match] = old_enabled;
        (void)dictionary_stack_reload(stack);
        return false;
    }

    fprintf(stderr,
        "stoin: dictionary '%s' %s\n",
        stack->paths[match],
        new_enabled ? "enabled" : "disabled");
    return true;
}

bool dictionary_stack_toggle(Dictionary_Stack *stack, const char *selections)
{
    if (stack == NULL || selections == NULL) {
        return false;
    }

    const char *p = selections;
    while (*p != '\0') {
        const char *end = strchr(p, ',');
        const size_t length = end == NULL ? strlen(p) : (size_t)(end - p);
        char *selection = copy_trimmed_range(p, length);
        if (selection == NULL) {
            return false;
        }
        if (selection[0] != '\0') {
            const char toggle = selection[0];
            const char *path = selection + 1;
            if (!toggle_dictionary_selection(stack, toggle, path)) {
                free(selection);
                return false;
            }
        }
        free(selection);
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    return true;
}

uint64_t dictionary_stack_revision(const Dictionary_Stack *stack)
{
    return stack == NULL ? 0 : stack->revision;
}

bool dictionary_stack_get_paths(const Dictionary_Stack *stack, const char *const **out_paths, size_t *out_path_count)
{
    if (stack == NULL || out_paths == NULL || out_path_count == NULL) {
        return false;
    }

    *out_paths = (const char *const *)stack->paths;
    *out_path_count = arrlenu(stack->paths);
    return true;
}

void dictionary_stack_destroy(Dictionary_Stack *stack)
{
    if (stack == NULL) {
        return;
    }

    clear_dictionary_paths(stack);
    dictionary_destroy(&stack->dictionary);
}
