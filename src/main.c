#include "platform.h"
#include "steno.h"

#include <string.h>
#include <stdio.h>

typedef struct App {
    Steno *steno;
} App;

static bool send_text(const char *utf8, void *userdata)
{
    (void)userdata;
    return platform_send_text_utf8(utf8);
}

static bool handle_input(const Input_Event *event, void *userdata)
{
    App *app = userdata;
    return steno_handle_event(app->steno, event);
}

int main(int argc, char **argv)
{
    const Steno_Config steno_config = {
        .keymap_path = "stoin.keymap",
        .dictionary_path = "stoin-dictionary.json",
        .send_text = send_text,
        .send_userdata = NULL,
    };

    if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        if (!steno_run_self_test(&steno_config)) {
            fprintf(stderr, "stoin: self-test failed\n");
            return 1;
        }
        puts("stoin: self-test passed");
        return 0;
    }

    App app = {
        .steno = steno_create(&steno_config),
    };
    if (app.steno == NULL) {
        return 1;
    }

    if (argc == 3 && strcmp(argv[1], "--lookup") == 0) {
        const char *translation = NULL;
        if (steno_lookup_stroke(app.steno, argv[2], &translation)) {
            printf("%s -> %s\n", argv[2], translation);
        } else {
            printf("%s -> [untranslated]\n", argv[2]);
        }
        steno_destroy(app.steno);
        return 0;
    }

    if ((argc == 2 || argc == 3) && strcmp(argv[1], "--dump-dictionary") == 0) {
        const char *path = argc == 3 ? argv[2] : "stoin-dictionary.json";
        if (!steno_dump_dictionary_json(app.steno, path)) {
            fprintf(stderr, "stoin: failed to dump dictionary to '%s'\n", path);
            steno_destroy(app.steno);
            return 1;
        }
        printf("stoin: wrote %zu entries to %s\n", steno_dictionary_count(app.steno), path);
        steno_destroy(app.steno);
        return 0;
    }

    if (!platform_init(handle_input, &app)) {
        steno_destroy(app.steno);
        return 1;
    }

    puts("stoin: macOS event tap running");
    printf("stoin: loaded %zu key bindings and %zu dictionary entries\n",
        steno_key_binding_count(app.steno),
        steno_dictionary_count(app.steno));
    puts("stoin: steno capture starts enabled; press Ctrl+Esc to toggle it");
    puts("stoin: Command/Control/Option shortcuts pass through; press Ctrl+C in this terminal to quit");

    platform_run();
    platform_shutdown();
    steno_destroy(app.steno);
    return 0;
}
