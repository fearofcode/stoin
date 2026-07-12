#include "stitch.h"

#include "text_util.h"

#include <ctype.h>
#include <string.h>

#include "../third_party/stb_ds.h"

typedef struct Text_Token {
    size_t start;
    size_t core_end;
    size_t end;
} Text_Token;

static bool stitch_is_word_byte(unsigned char c)
{
    return c >= 0x80 || isalnum(c) || c == '_' || c == '\'';
}

static bool collect_text_tokens(const char *text, Text_Token **out_tokens)
{
    if (text == NULL || out_tokens == NULL) {
        return false;
    }

    const size_t length = strlen(text);
    size_t index = 0;
    while (index < length) {
        while (index < length && isspace((unsigned char)text[index])) {
            ++index;
        }
        if (index >= length) {
            break;
        }

        const size_t start = index;
        if (stitch_is_word_byte((unsigned char)text[index])) {
            ++index;
            while (index < length) {
                const unsigned char c = (unsigned char)text[index];
                if (stitch_is_word_byte(c) || c == '-') {
                    ++index;
                } else {
                    break;
                }
            }
        } else {
            ++index;
            while (index < length
                && !isspace((unsigned char)text[index])
                && !stitch_is_word_byte((unsigned char)text[index])) {
                ++index;
            }
        }

        const size_t core_end = index;
        while (index < length && isspace((unsigned char)text[index])) {
            ++index;
        }
        Text_Token token = {
            .start = start,
            .core_end = core_end,
            .end = index,
        };
        arrput(*out_tokens, token);
    }

    return true;
}

static bool append_stitched_core(char **out, const char *start, const char *end, const char *delimiter)
{
    bool first = true;
    for (const char *p = start; p < end;) {
        const size_t length = utf8_codepoint_length(p, end);
        if (!first && !text_append_range(out, delimiter, strlen(delimiter))) {
            return false;
        }
        if (!text_append_range(out, p, length)) {
            return false;
        }
        p += length;
        first = false;
    }
    return true;
}

static bool stitch_text_suffix(
    const char *text,
    size_t token_count,
    const char *delimiter,
    char **out
)
{
    Text_Token *tokens = NULL;
    if (!collect_text_tokens(text, &tokens)) {
        return false;
    }

    const size_t available = arrlenu(tokens);
    if (available == 0 || token_count == 0) {
        arrfree(tokens);
        return text_append_cstring(out, text);
    }

    if (token_count > available) {
        token_count = available;
    }

    const size_t first_token = available - token_count;
    const size_t prefix_end = tokens[first_token].start;
    if (!text_append_range(out, text, prefix_end)) {
        arrfree(tokens);
        return false;
    }

    for (size_t i = first_token; i < available; ++i) {
        const Text_Token token = tokens[i];
        if (!append_stitched_core(out, text + token.start, text + token.core_end, delimiter)
            || !text_append_range(out, text + token.core_end, token.end - token.core_end)) {
            arrfree(tokens);
            return false;
        }
    }

    arrfree(tokens);
    return true;
}

static size_t stitch_token_count(const char *text)
{
    Text_Token *tokens = NULL;
    if (!collect_text_tokens(text, &tokens)) {
        return 0;
    }
    const size_t count = arrlenu(tokens);
    arrfree(tokens);
    return count;
}

bool stitch_apply_retro(
    Stitch_Context *stitch,
    const uint64_t *strokes,
    size_t stroke_count,
    size_t replaced_count,
    size_t stitch_count,
    const char *delimiter
)
{
    if (stitch == NULL || stitch->translations == NULL || stitch->replace_output == NULL) {
        return false;
    }

    Translation *translations = *stitch->translations;
    const size_t translation_count = arrlenu(translations);
    if (replaced_count > translation_count) {
        return false;
    }

    size_t replace_start = translation_count - replaced_count;
    char *source_text = NULL;
    while (true) {
        arrfree(source_text);
        source_text = translation_range_source_text(
            translations,
            replace_start,
            translation_count - replace_start
        );
        if (source_text == NULL) {
            return false;
        }
        if (stitch_token_count(source_text) >= stitch_count || replace_start == 0) {
            break;
        }
        --replace_start;
    }

    const char *actual_delimiter = delimiter != NULL ? delimiter : "-";
    const size_t actual_replaced_count = translation_count - replace_start;
    char *old_text = translation_range_text(translations, replace_start, actual_replaced_count);
    char *new_text = NULL;
    if (old_text == NULL
        || !stitch_text_suffix(source_text, stitch_count, actual_delimiter, &new_text)) {
        arrfree(old_text);
        arrfree(source_text);
        arrfree(new_text);
        return false;
    }

    Translation_Source source = stitch->source;
    for (size_t i = replace_start; i < translation_count; ++i) {
        if (translations[i].source != stitch->source) {
            source = TRANSLATION_SOURCE_MIXED;
            break;
        }
    }

    Translation next = {
        .source = source,
    };
    next.utf8 = new_text;
    if (!translation_set_strokes(&next, strokes, stroke_count)) {
        arrfree(old_text);
        arrfree(source_text);
        translation_destroy(&next);
        return false;
    }

    if (!stitch->replace_output(stitch->userdata, old_text, next.utf8)) {
        arrfree(old_text);
        arrfree(source_text);
        translation_destroy(&next);
        return false;
    }

    for (size_t i = replace_start; i < translation_count; ++i) {
        arrput(next.replaced, translations[i]);
    }
    arrsetlen(*stitch->translations, replace_start);
    arrput(*stitch->translations, next);

    arrfree(old_text);
    arrfree(source_text);
    return true;
}
