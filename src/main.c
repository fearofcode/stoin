#include "platform.h"
#include "steno.h"

#include <string.h>
#include <stdio.h>

typedef struct Stoin_App {
    Stoin_Steno *steno;
} Stoin_App;

static bool send_text(const char *utf8, void *userdata)
{
    (void)userdata;
    return stoin_platform_send_text_utf8(utf8);
}

static bool handle_input(const Stoin_Input_Event *event, void *userdata)
{
    Stoin_App *app = userdata;
    return stoin_steno_handle_event(app->steno, event);
}

int main(int argc, char **argv)
{
    const Stoin_Steno_Config steno_config = {
        .keymap_path = "stoin.keymap",
        .dictionary_path = "stoin-dictionary.json",
        .send_text = send_text,
        .send_userdata = NULL,
    };

    if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        if (!stoin_steno_run_self_test(&steno_config)) {
            fprintf(stderr, "stoin: self-test failed\n");
            return 1;
        }
        puts("stoin: self-test passed");
        return 0;
    }

    Stoin_App app = {
        .steno = stoin_steno_create(&steno_config),
    };
    if (app.steno == NULL) {
        return 1;
    }

    if (argc == 3 && strcmp(argv[1], "--lookup") == 0) {
        const char *translation = NULL;
        if (stoin_steno_lookup_stroke(app.steno, argv[2], &translation)) {
            printf("%s -> %s\n", argv[2], translation);
        } else {
            printf("%s -> [untranslated]\n", argv[2]);
        }
        stoin_steno_destroy(app.steno);
        return 0;
    }

    if ((argc == 2 || argc == 3) && strcmp(argv[1], "--dump-dictionary") == 0) {
        const char *path = argc == 3 ? argv[2] : "stoin-dictionary.json";
        if (!stoin_steno_dump_dictionary_json(app.steno, path)) {
            fprintf(stderr, "stoin: failed to dump dictionary to '%s'\n", path);
            stoin_steno_destroy(app.steno);
            return 1;
        }
        printf("stoin: wrote %zu entries to %s\n", stoin_steno_dictionary_count(app.steno), path);
        stoin_steno_destroy(app.steno);
        return 0;
    }

    if (!stoin_platform_init(handle_input, &app)) {
        stoin_steno_destroy(app.steno);
        return 1;
    }

    puts("stoin: macOS event tap running");
    printf("stoin: loaded %zu key bindings and %zu dictionary entries\n",
        stoin_steno_key_binding_count(app.steno),
        stoin_steno_dictionary_count(app.steno));
    puts("stoin: steno capture starts enabled; press Ctrl+Esc to toggle it");
    puts("stoin: Command/Control/Option shortcuts pass through; press Ctrl+C in this terminal to quit");

    stoin_platform_run();
    stoin_platform_shutdown();
    stoin_steno_destroy(app.steno);
    return 0;
}
