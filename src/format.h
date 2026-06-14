#ifndef FORMAT_H
#define FORMAT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum Case_Mode {
    CASE_MODE_NORMAL,
    CASE_MODE_CAP_FIRST_WORD,
    CASE_MODE_UPPER_FIRST_WORD,
    CASE_MODE_LOWER_FIRST_CHAR,
    CASE_MODE_UPPER,
    CASE_MODE_TITLE,
    CASE_MODE_LOWER,
} Case_Mode;

typedef enum Retro_Command {
    RETRO_COMMAND_NONE,
    RETRO_COMMAND_TOGGLE_ASTERISK,
    RETRO_COMMAND_DELETE_SPACE,
    RETRO_COMMAND_INSERT_SPACE,
} Retro_Command;

typedef struct Formatted_Text {
    char *text;
    char *ortho_suffix;
    char *stitch_delimiter;
    char **key_combos;
    char *plover_command;
    char *mode_command;
    size_t stitch_count;
    size_t ortho_suffix_text_offset;
    size_t ortho_suffix_text_length;
    Case_Mode text_case;
    Case_Mode next_case;
    Case_Mode retro_case;
    bool attach_prev;
    bool attach_next;
    bool glue;
    bool stitch;
    bool stitch_last_word;
    bool stitch_phrase;
    bool carry_case;
    bool cancel_formatting;
    Retro_Command retro_command;
} Formatted_Text;

bool format_translation_text(const char *translation, Formatted_Text *out);
void formatted_text_destroy(Formatted_Text *formatted);
void formatted_text_apply_case(char *text, Case_Mode mode);

#endif
