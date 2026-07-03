#include "gemini_pr.h"
#include "json_util.h"
#include "orthography.h"
#include "platform.h"
#include "stentura.h"
#include "steno.h"
#include "steno_stroke.h"
#include "stroke_merge.h"
#include "tx_bolt.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stb_ds.h"

typedef struct Test_Output {
    char *text;
    char last_send[128];
    char last_delete[128];
    char last_key_combo[128];
    size_t send_count;
    size_t delete_count;
    size_t key_combo_count;
} Test_Output;

typedef struct Watch_Test {
    Steno *steno;
    size_t reload_count;
} Watch_Test;

static bool expect_string(const char *name, const char *actual, const char *expected);

enum {
    TEST_KEYCODE_ESCAPE = 53,
    TEST_KEYCODE_LEFT_CONTROL = 59,
};

static bool test_send_text(const char *utf8, void *userdata)
{
    Test_Output *output = userdata;
    ++output->send_count;
    snprintf(output->last_send, sizeof(output->last_send), "%s", utf8);

    if (output->text != NULL && arrlenu(output->text) > 0) {
        arrpop(output->text);
    }
    for (const char *p = utf8; *p != '\0'; ++p) {
        arrput(output->text, *p);
    }
    arrput(output->text, '\0');
    return true;
}

static bool test_delete_text(const char *utf8, void *userdata)
{
    Test_Output *output = userdata;
    const size_t delete_length = strlen(utf8);
    if (delete_length == 0) {
        return true;
    }
    if (output->text == NULL) {
        return false;
    }

    const size_t length = strlen(output->text);
    if (delete_length > length) {
        return false;
    }
    if (memcmp(output->text + length - delete_length, utf8, delete_length) != 0) {
        return false;
    }

    ++output->delete_count;
    snprintf(output->last_delete, sizeof(output->last_delete), "%s", utf8);

    const size_t new_length = length - delete_length;
    arrsetlen(output->text, new_length + 1);
    output->text[new_length] = '\0';
    return true;
}

static bool test_send_key_combination(const char *combo, void *userdata)
{
    Test_Output *output = userdata;
    ++output->key_combo_count;
    snprintf(output->last_key_combo, sizeof(output->last_key_combo), "%s", combo);
    return true;
}

static void clear_test_output(Test_Output *output)
{
    arrsetlen(output->text, 0);
    arrput(output->text, '\0');
}

static void reset_output_log(Test_Output *output)
{
    output->last_send[0] = '\0';
    output->last_delete[0] = '\0';
    output->last_key_combo[0] = '\0';
    output->send_count = 0;
    output->delete_count = 0;
    output->key_combo_count = 0;
}

static bool reset_test_steno(Steno **steno, const Steno_Config *config)
{
    if (steno == NULL || config == NULL) {
        return false;
    }
    steno_destroy(*steno);
    *steno = steno_create(config);
    return *steno != NULL;
}

static bool handle_mock_pedal_stroke(Steno *steno, Phrase_Namespace namespace, const char *stroke)
{
    uint64_t bits = 0;
    if (!stroke_string_to_bits(stroke, &bits)) {
        fprintf(stderr, "test failed: could not parse mock pedal stroke '%s'\n", stroke);
        return false;
    }

    steno_set_phrase_namespace(steno, namespace, true);
    Stroke_Input input = {
        .bits = bits,
        .phrase_namespace = namespace,
    };
    const bool ok = steno_handle_stroke(steno, input);
    steno_set_phrase_namespace(steno, namespace, false);
    return ok;
}

static bool expect_mock_pedal_phrase(
    Steno **steno,
    const Steno_Config *config,
    Test_Output *output,
    const char *name,
    Phrase_Namespace namespace,
    const char *stroke,
    const char *expected
)
{
    if (!reset_test_steno(steno, config)) {
        return false;
    }
    clear_test_output(output);
    reset_output_log(output);
    return handle_mock_pedal_stroke(*steno, namespace, stroke)
        && expect_string(name, output->text, expected);
}

static bool write_text_file(const char *path, const char *contents)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    if (fputs(contents, file) == EOF) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static void test_dictionary_watch_callback(void *userdata)
{
    Watch_Test *watch = userdata;
    if (watch == NULL) {
        return;
    }
    if (steno_reload_dictionary(watch->steno)) {
        ++watch->reload_count;
    }
}

static bool send_key_event(Steno *steno, const char *key_name, bool is_down)
{
    uint16_t keycode = 0;
    if (!platform_keycode_from_name(key_name, &keycode)) {
        return false;
    }

    Input_Event event = {
        .keycode = keycode,
        .is_down = is_down,
    };
    return steno_handle_event(steno, &event);
}

static bool send_raw_key_event(
    Steno *steno,
    uint16_t keycode,
    bool is_down,
    bool control,
    bool option,
    bool command
)
{
    const Input_Event event = {
        .keycode = keycode,
        .is_down = is_down,
        .control = control,
        .option = option,
        .command = command,
    };
    return steno_handle_event(steno, &event);
}

static bool expect_string(const char *name, const char *actual, const char *expected)
{
    if (actual != NULL && strcmp(actual, expected) == 0) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected '%s', got '%s'\n",
        name,
        expected,
        actual == NULL ? "(null)" : actual);
    return false;
}

static bool expect_size_at_most(const char *name, size_t actual, size_t expected_max)
{
    if (actual <= expected_max) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected at most %zu, got %zu\n",
        name,
        expected_max,
        actual);
    return false;
}

static bool expect_size(const char *name, size_t actual, size_t expected)
{
    if (actual == expected) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected %zu, got %zu\n",
        name,
        expected,
        actual);
    return false;
}

static bool expect_u64(const char *name, uint64_t actual, uint64_t expected)
{
    if (actual == expected) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected 0x%llx, got 0x%llx\n",
        name,
        (unsigned long long)expected,
        (unsigned long long)actual);
    return false;
}

static bool expect_bytes(
    const char *name,
    const uint8_t *actual,
    size_t actual_size,
    const uint8_t *expected,
    size_t expected_size
)
{
    if (actual_size == expected_size && memcmp(actual, expected, expected_size) == 0) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected %zu bytes, got %zu bytes\n",
        name,
        expected_size,
        actual_size);
    const size_t min_size = actual_size < expected_size ? actual_size : expected_size;
    for (size_t i = 0; i < min_size; ++i) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "test failed: %s: byte %zu expected 0x%02x, got 0x%02x\n",
                name,
                i,
                expected[i],
                actual[i]);
            break;
        }
    }
    return false;
}

static bool expect_stroke_format(const char *input, const char *expected)
{
    uint64_t bits = 0;
    char output[64] = {0};

    if (!stroke_string_to_bits(input, &bits)) {
        fprintf(stderr, "test failed: could not parse stroke '%s'\n", input);
        return false;
    }
    if (!chord_bits_to_string(bits, output, sizeof(output))) {
        fprintf(stderr, "test failed: could not format stroke '%s'\n", input);
        return false;
    }

    return expect_string(input, output, expected);
}

static bool expect_orthography(
    const Orthography *orthography,
    const char *word,
    const char *suffix,
    const char *expected
)
{
    char *actual = NULL;
    if (!orthography_apply(orthography, word, suffix, &actual)) {
        fprintf(stderr, "test failed: orthography %s + %s failed\n", word, suffix);
        return false;
    }

    const bool ok = expect_string("orthography", actual, expected);
    free(actual);
    return ok;
}

static bool expect_trace_contains(FILE *trace_file, const char *name, const char *expected)
{
    char contents[1024] = {0};

    fflush(trace_file);
    rewind(trace_file);
    const size_t read = fread(contents, 1, sizeof(contents) - 1, trace_file);
    contents[read] = '\0';
    if (strstr(contents, expected) != NULL) {
        return true;
    }

    fprintf(stderr, "test failed: %s: expected trace to contain '%s', got '%s'\n",
        name,
        expected,
        contents);
    return false;
}

static bool expect_json_object_with_brace_in_string(void)
{
    const char *json =
        "{\n"
        "  \"phrase_core\": {\n"
        "    \"device_name\": \"\\\\\\\\?\\\\HID#VID_3553#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}\",\n"
        "    \"usage_page\": 7,\n"
        "    \"vk\": 65\n"
        "  }\n"
        "}\n";
    const char *end = NULL;
    const char *start = json_find_object(json, "phrase_core", &end);
    if (start == NULL || end == NULL) {
        fputs("test failed: json object with brace in string was not found\n", stderr);
        return false;
    }

    uint32_t value = 0;
    if (!json_parse_uint_field(start, end, "vk", &value) || value != 65) {
        fprintf(stderr, "test failed: json field after brace in string parsed as %u\n", value);
        return false;
    }

    char *device_name = NULL;
    const bool parsed = json_parse_string_field(start, end, "device_name", &device_name);
    const bool ok = parsed
        && expect_string(
            "json string with brace",
            device_name,
            "\\\\?\\HID#VID_3553#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}");
    free(device_name);
    return ok;
}

int main(void)
{
    Test_Output output = {0};
    Steno_Config config = {
        .keymap_path = "tests/test.keymap",
        .dictionary_path = "tests/test-dictionary.json",
        .word_list_path = "tests/test-words.txt",
        .send_text = test_send_text,
        .delete_text = test_delete_text,
        .send_key_combination = test_send_key_combination,
        .send_userdata = &output,
    };

    Steno *steno = steno_create(&config);
    if (steno == NULL) {
        fputs("test failed: could not create steno engine\n", stderr);
        return 1;
    }

    bool ok = true;
    ok = ok && expect_json_object_with_brace_in_string();

    Orthography test_orthography = {0};
    ok = ok && orthography_load(&test_orthography, "tests/test-words.txt");
    ok = ok && orthography_word_count(&test_orthography) > 0;
    ok = ok && expect_orthography(&test_orthography, "artistic", "ly", "artistically");
    ok = ok && expect_orthography(&test_orthography, "cosmetic", "ly", "cosmetically");
    ok = ok && expect_orthography(&test_orthography, "establish", "s", "establishes");
    ok = ok && expect_orthography(&test_orthography, "speech", "s", "speeches");
    ok = ok && expect_orthography(&test_orthography, "approach", "s", "approaches");
    ok = ok && expect_orthography(&test_orthography, "beach", "s", "beaches");
    ok = ok && expect_orthography(&test_orthography, "arch", "s", "arches");
    ok = ok && expect_orthography(&test_orthography, "larch", "s", "larches");
    ok = ok && expect_orthography(&test_orthography, "march", "s", "marches");
    ok = ok && expect_orthography(&test_orthography, "search", "s", "searches");
    ok = ok && expect_orthography(&test_orthography, "starch", "s", "starches");
    ok = ok && expect_orthography(&test_orthography, "stomach", "s", "stomachs");
    ok = ok && expect_orthography(&test_orthography, "monarch", "s", "monarchs");
    ok = ok && expect_orthography(&test_orthography, "patriarch", "s", "patriarchs");
    ok = ok && expect_orthography(&test_orthography, "oligarch", "s", "oligarchs");
    ok = ok && expect_orthography(&test_orthography, "cherry", "s", "cherries");
    ok = ok && expect_orthography(&test_orthography, "day", "s", "days");
    ok = ok && expect_orthography(&test_orthography, "penny", "s", "pennies");
    ok = ok && expect_orthography(&test_orthography, "pharmacy", "ist", "pharmacist");
    ok = ok && expect_orthography(&test_orthography, "melody", "ist", "melodist");
    ok = ok && expect_orthography(&test_orthography, "pacify", "ist", "pacifist");
    ok = ok && expect_orthography(&test_orthography, "geology", "ist", "geologist");
    ok = ok && expect_orthography(&test_orthography, "metallurgy", "ist", "metallurgist");
    ok = ok && expect_orthography(&test_orthography, "anarchy", "ist", "anarchist");
    ok = ok && expect_orthography(&test_orthography, "monopoly", "ist", "monopolist");
    ok = ok && expect_orthography(&test_orthography, "alchemy", "ist", "alchemist");
    ok = ok && expect_orthography(&test_orthography, "similar", "ish", "similarish");
    ok = ok && expect_orthography(&test_orthography, "red", "ish", "reddish");
    ok = ok && expect_orthography(&test_orthography, "tinker", "er", "tinkerer");
    ok = ok && expect_orthography(&test_orthography, "filter", "er", "filterer");
    ok = ok && expect_orthography(&test_orthography, "stymie", "ed", "stymied");
    ok = ok && expect_orthography(&test_orthography, "tie", "ed", "tied");
    ok = ok && expect_orthography(&test_orthography, "die", "ed", "died");
    orthography_destroy(&test_orthography);

    uint64_t rr_bits = 0;
    char rr_string[64] = {0};
    ok = ok && stroke_string_to_bits("R-R", &rr_bits);
    ok = ok && chord_bits_to_string(rr_bits, rr_string, sizeof(rr_string));
    ok = ok && expect_string("R-R canonicalization", rr_string, "R-R");
    ok = ok && expect_stroke_format("SA-P", "SAP");
    ok = ok && expect_stroke_format("TAT", "TAT");
    ok = ok && expect_stroke_format("R-R", "R-R");
    ok = ok && expect_stroke_format("-T", "-T");
    ok = ok && expect_stroke_format("-F", "F");
    ok = ok && expect_stroke_format("#1", "#S");
    ok = ok && expect_stroke_format("#2", "#T");
    ok = ok && expect_stroke_format("#3", "#P");
    ok = ok && expect_stroke_format("#4", "#H");
    ok = ok && expect_stroke_format("#5", "#A");
    ok = ok && expect_stroke_format("#0", "#O");
    ok = ok && expect_stroke_format("#-6", "#F");
    ok = ok && expect_stroke_format("#-7", "#-P");
    ok = ok && expect_stroke_format("#-8", "#L");
    ok = ok && expect_stroke_format("#-9", "#-T");
    ok = ok && expect_stroke_format("#*-678G", "#*FPLG");
    uint64_t bare_number_bits = 0;
    ok = ok && !stroke_string_to_bits("1", &bare_number_bits);
    const char *drill_chords[] = {
        "SAP", "HUD", "SOG", "TOD", "WET", "POG", "ROD", "KUS", "PEB", "ROR",
        "WEZ", "WEL", "TER", "TAT", "WEF", "KAB", "WES", "SAP", "TAS", "RET",
        "TAD", "PEP", "SEB", "KOF", "TUZ", "PEF", "HEL", "PUB", "RAT", "WAF",
        "TAB", "RAS", "HUP", "WUP", "PEZ", "SOF", "HUR", "PUZ", "SOB", "POT",
        "KED", "WUD", "SAG", "RAP", "RAL", "ROL", "WOZ", "KAD", "KAT", "KOB",
        "RAD", "TAR", "SAL", "ROF", "SOR", "WOT", "HUF", "TUR", "KAF", "HOR",
        "SOD", "KOT", "SEF", "RED", "HAP", "PAP", "KEG", "KOZ", "TUS", "SOZ",
        "TAG", "HAS", "TAF", "HES", "HOL", "WUR", "TEB", "HAB", "HER", "PER",
        "TOP", "HAZ", "POL", "WOS", "HOP", "SUT", "TOR", "REL", "PAT", "SER",
        "WUS", "PUP", "KAG", "POD", "SUB", "HED", "SAB", "SUL", "TEF", "SOL",
    };
    for (size_t i = 0; i < sizeof(drill_chords) / sizeof(drill_chords[0]); ++i) {
        ok = ok && expect_stroke_format(drill_chords[i], drill_chords[i]);
    }

    uint64_t gemini_bits = 0;
    const uint8_t gemini_sat[GEMINI_PR_PACKET_SIZE] = { 0x80, 0x40, 0x20, 0x00, 0x04, 0x00 };
    ok = ok && gemini_pr_decode_packet(gemini_sat, &gemini_bits);
    char gemini_sat_string[64] = {0};
    ok = ok && chord_bits_to_string(gemini_bits, gemini_sat_string, sizeof(gemini_sat_string));
    ok = ok && expect_string("Gemini PR SAT packet", gemini_sat_string, "SAT");

    const uint8_t gemini_number_star_z[GEMINI_PR_PACKET_SIZE] = { 0xA0, 0x00, 0x08, 0x00, 0x00, 0x01 };
    ok = ok && gemini_pr_decode_packet(gemini_number_star_z, &gemini_bits);
    char gemini_number_star_z_string[64] = {0};
    ok = ok && chord_bits_to_string(gemini_bits, gemini_number_star_z_string, sizeof(gemini_number_star_z_string));
    ok = ok && expect_string("Gemini PR number star Z packet", gemini_number_star_z_string, "#*Z");

    const uint8_t bad_gemini_start[GEMINI_PR_PACKET_SIZE] = { 0x00, 0x40, 0x20, 0x00, 0x04, 0x00 };
    ok = ok && !gemini_pr_decode_packet(bad_gemini_start, &gemini_bits);
    const uint8_t bad_gemini_continuation[GEMINI_PR_PACKET_SIZE] = { 0x80, 0xC0, 0x20, 0x00, 0x04, 0x00 };
    ok = ok && !gemini_pr_decode_packet(bad_gemini_continuation, &gemini_bits);

    const uint8_t stentura_crc_input[] = "123456789";
    ok = ok && expect_u64(
        "Stentura CRC-16 check value",
        stentura_crc16(stentura_crc_input, sizeof(stentura_crc_input) - 1),
        0xBB3D);

    uint64_t stentura_bits = 0;
    char stentura_string[64] = {0};
    const uint8_t stentura_sat[4] = { 0xC8, 0xC4, 0xC0, 0xC8 };
    ok = ok && stentura_decode_stroke(stentura_sat, &stentura_bits);
    ok = ok && chord_bits_to_string(stentura_bits, stentura_string, sizeof(stentura_string));
    ok = ok && expect_string("Stentura SAT stroke", stentura_string, "SAT");

    const uint8_t stentura_praoerbgs[4] = { 0xC1, 0xCE, 0xE5, 0xD4 };
    memset(stentura_string, 0, sizeof(stentura_string));
    ok = ok && stentura_decode_stroke(stentura_praoerbgs, &stentura_bits);
    ok = ok && chord_bits_to_string(stentura_bits, stentura_string, sizeof(stentura_string));
    ok = ok && expect_string("Stentura PRAOERBGS stroke", stentura_string, "PRAOERBGS");

    const uint8_t bad_stentura_stroke[4] = { 0xC8, 0x84, 0xC0, 0xC8 };
    ok = ok && !stentura_decode_stroke(bad_stentura_stroke, &stentura_bits);

    const uint8_t stentura_strokes[] = {
        0xC8, 0xC4, 0xC0, 0xC8,
        0xC1, 0xCE, 0xE5, 0xD4,
    };
    uint64_t decoded_stentura_strokes[2] = {0};
    size_t decoded_stentura_count = 0;
    ok = ok && stentura_decode_strokes(
        stentura_strokes,
        sizeof(stentura_strokes),
        decoded_stentura_strokes,
        sizeof(decoded_stentura_strokes) / sizeof(decoded_stentura_strokes[0]),
        &decoded_stentura_count);
    ok = ok && expect_size("Stentura decoded stroke count", decoded_stentura_count, 2);

    uint8_t stentura_packet[64] = {0};
    const uint8_t expected_stentura_read[] = {
        0x01, 0x20, 0x12, 0x00, 0x0B, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x14, 0x00, 0x01, 0x00, 0x08, 0x00, 0x83, 0x3C,
    };
    size_t stentura_packet_size = stentura_make_read(stentura_packet, sizeof(stentura_packet), 32, 1, 8, 20);
    ok = ok && expect_bytes(
        "Stentura READC request",
        stentura_packet,
        stentura_packet_size,
        expected_stentura_read,
        sizeof(expected_stentura_read));

    const uint8_t expected_stentura_open[] = {
        0x01, 0x4F, 0x20, 0x00, 0x0A, 0x00, 0x41, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xDD,
        0x52, 0x45, 0x41, 0x4C, 0x54, 0x49, 0x4D, 0x45, 0x2E,
        0x30, 0x30, 0x30, 0x49, 0xF2,
    };
    memset(stentura_packet, 0, sizeof(stentura_packet));
    stentura_packet_size =
        stentura_make_open(stentura_packet, sizeof(stentura_packet), 79, 'A', "REALTIME.000");
    ok = ok && expect_bytes(
        "Stentura OPEN request",
        stentura_packet,
        stentura_packet_size,
        expected_stentura_open,
        sizeof(expected_stentura_open));

    const uint8_t expected_stentura_reset[] = {
        0x01, 0x43, 0x12, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x13,
    };
    memset(stentura_packet, 0, sizeof(stentura_packet));
    stentura_packet_size = stentura_make_reset(stentura_packet, sizeof(stentura_packet), 67);
    ok = ok && expect_bytes(
        "Stentura RESET request",
        stentura_packet,
        stentura_packet_size,
        expected_stentura_reset,
        sizeof(expected_stentura_reset));

    const uint8_t valid_stentura_response[] = {
        0x01, 0x05, 0x0E, 0x00, 0x09, 0x00, 0x01,
        0x00, 0x02, 0x00, 0x03, 0x00, 0xB0, 0xCA,
    };
    const uint8_t valid_stentura_data_response[] = {
        0x01, 0x05, 0x15, 0x00, 0x09, 0x00, 0x01, 0x00, 0x02,
        0x00, 0x03, 0x00, 0xC0, 0xBA, 0x68, 0x65, 0x6C, 0x6C,
        0x6F, 0xD2, 0x34,
    };
    ok = ok && stentura_validate_response(valid_stentura_response, sizeof(valid_stentura_response));
    ok = ok && stentura_validate_response(valid_stentura_data_response, sizeof(valid_stentura_data_response));
    ok = ok && !stentura_validate_response(valid_stentura_response, sizeof(valid_stentura_response) - 1);
    uint8_t bad_stentura_response[sizeof(valid_stentura_data_response)] = {0};
    memcpy(bad_stentura_response, valid_stentura_data_response, sizeof(bad_stentura_response));
    bad_stentura_response[sizeof(bad_stentura_response) - 1] ^= 0x01;
    ok = ok && !stentura_validate_response(bad_stentura_response, sizeof(bad_stentura_response));

    Tx_Bolt tx_bolt = {0};
    uint64_t tx_bolt_bits = 0;
    char tx_bolt_string[64] = {0};
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x01, &tx_bolt_bits);
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x42, &tx_bolt_bits);
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x84, &tx_bolt_bits);
    ok = ok && tx_bolt_flush_stroke(&tx_bolt, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt SAP packet", tx_bolt_string, "SAP");

    memset(&tx_bolt, 0, sizeof(tx_bolt));
    memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x48, &tx_bolt_bits);
    ok = ok && tx_bolt_decode_byte(&tx_bolt, 0xD8, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt number star Z packet", tx_bolt_string, "#*Z");

    const struct {
        uint8_t bytes[3];
        size_t byte_count;
        const char *expected;
    } tx_bolt_number_bar_cases[] = {
        { { 0x01, 0xD0 }, 2, "#S" },
        { { 0x02, 0xD0 }, 2, "#T" },
        { { 0x08, 0xD0 }, 2, "#P" },
        { { 0x20, 0xD0 }, 2, "#H" },
        { { 0x00, 0x42, 0xD0 }, 3, "#A" },
        { { 0x44, 0xD0 }, 2, "#O" },
        { { 0x81, 0xD0 }, 2, "#F" },
        { { 0x84, 0xD0 }, 2, "#-P" },
        { { 0x90, 0xD0 }, 2, "#L" },
        { { 0xD1 }, 1, "#-T" },
    };
    for (size_t i = 0; i < sizeof(tx_bolt_number_bar_cases) / sizeof(tx_bolt_number_bar_cases[0]); ++i) {
        memset(&tx_bolt, 0, sizeof(tx_bolt));
        memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
        bool decoded = false;
        for (size_t j = 0; j < tx_bolt_number_bar_cases[i].byte_count; ++j) {
            decoded = tx_bolt_decode_byte(&tx_bolt, tx_bolt_number_bar_cases[i].bytes[j], &tx_bolt_bits);
        }
        ok = ok && decoded;
        ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
        ok = ok && expect_string("TX Bolt number-bar position", tx_bolt_string, tx_bolt_number_bar_cases[i].expected);
    }

    memset(&tx_bolt, 0, sizeof(tx_bolt));
    memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
    ok = ok && !tx_bolt_decode_byte(&tx_bolt, 0x01, &tx_bolt_bits);
    ok = ok && tx_bolt_decode_byte(&tx_bolt, 0x02, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt lower set starts new stroke", tx_bolt_string, "S");
    memset(tx_bolt_string, 0, sizeof(tx_bolt_string));
    ok = ok && tx_bolt_flush_stroke(&tx_bolt, &tx_bolt_bits);
    ok = ok && chord_bits_to_string(tx_bolt_bits, tx_bolt_string, sizeof(tx_bolt_string));
    ok = ok && expect_string("TX Bolt queued next stroke", tx_bolt_string, "T");

    Stroke_Merge merge = {0};
    uint64_t merge_output = 0;
    stroke_merge_init(&merge, 150);
    ok = ok && stroke_merge_push(&merge, 1, UINT64_C(0x01), 1000);
    ok = ok && !stroke_merge_next_output(&merge, &merge_output);
    ok = ok && stroke_merge_push(&merge, 2, UINT64_C(0x20), 1030);
    ok = ok && stroke_merge_next_output(&merge, &merge_output);
    ok = ok && expect_u64("multi-input different devices merge", merge_output, UINT64_C(0x21));
    ok = ok && !stroke_merge_next_output(&merge, &merge_output);

    stroke_merge_clear(&merge);
    ok = ok && stroke_merge_push(&merge, 1, UINT64_C(0x01), 2000);
    ok = ok && stroke_merge_push(&merge, 1, UINT64_C(0x02), 2010);
    ok = ok && !stroke_merge_next_output(&merge, &merge_output);
    ok = ok && stroke_merge_push(&merge, 2, UINT64_C(0x20), 2020);
    ok = ok && stroke_merge_next_output(&merge, &merge_output);
    ok = ok && expect_u64("multi-input queued stroke merged first", merge_output, UINT64_C(0x21));
    ok = ok && stroke_merge_next_output(&merge, &merge_output);
    ok = ok && expect_u64("multi-input same device queued separately", merge_output, UINT64_C(0x02));

    stroke_merge_clear(&merge);
    ok = ok && stroke_merge_push(&merge, 1, UINT64_C(0x04), 3000);
    ok = ok && stroke_merge_push(&merge, 1, UINT64_C(0x08), 3010);
    ok = ok && stroke_merge_poll(&merge, 3150);
    ok = ok && stroke_merge_next_output(&merge, &merge_output);
    ok = ok && expect_u64("multi-input timeout emits pending", merge_output, UINT64_C(0x04));
    ok = ok && stroke_merge_next_output(&merge, &merge_output);
    ok = ok && expect_u64("multi-input timeout emits queued", merge_output, UINT64_C(0x08));

    stroke_merge_clear(&merge);
    stroke_merge_set_window_ms(&merge, 0);
    ok = ok && stroke_merge_push(&merge, 1, UINT64_C(0x10), 4000);
    ok = ok && stroke_merge_next_output(&merge, &merge_output);
    ok = ok && expect_u64("zero merge window emits immediately", merge_output, UINT64_C(0x10));
    stroke_merge_destroy(&merge);

    const char *the = NULL;
    ok = ok && steno_lookup_stroke(steno, "-T", &the);
    ok = ok && expect_string("dictionary lookup -T", the, "the");

    const char *undo = NULL;
    ok = ok && steno_lookup_stroke(steno, "-R", &undo);
    ok = ok && expect_string("dictionary lookup -R", undo, "=undo");

    const char *suffix_s = NULL;
    ok = ok && steno_lookup_stroke(steno, "-S", &suffix_s);
    ok = ok && expect_string("dictionary lookup -S", suffix_s, "{^s}");

    const char *glue_p = NULL;
    ok = ok && steno_lookup_stroke(steno, "P*P", &glue_p);
    ok = ok && expect_string("dictionary lookup P*P", glue_p, "{&P}");

    const char *ampersand = NULL;
    ok = ok && steno_lookup_stroke(steno, "PH", &ampersand);
    ok = ok && expect_string("dictionary lookup unicode escaped key and value", ampersand, "&");

    const char *stories = NULL;
    ok = ok && steno_lookup_stroke(steno, "STOE-R/-Z", &stories);
    ok = ok && expect_string("dictionary lookup canonical multi-stroke", stories, "stories");

    const char *questioningly = NULL;
    ok = ok && steno_lookup_stroke(steno, "#*-678G", &questioningly);
    ok = ok && expect_string("dictionary lookup number-bar digits", questioningly, "the questioningly");

    const char *evergrande = NULL;
    ok = ok && steno_lookup_stroke(steno, "#*-6R/TKPWRA-PBD", &evergrande);
    ok = ok && expect_string("dictionary lookup multi-stroke number-bar digits", evergrande, "the Evergrande");

    const char *histories = NULL;
    ok = ok && steno_lookup_stroke(steno, "HEU/STOE-R/-Z", &histories);
    ok = ok && expect_string("dictionary lookup longest multi-stroke", histories, "histories");

    const char *reload_path = "build/test-hot-reload-dictionary.json";
    ok = ok && write_text_file(reload_path, "{ \"S\": \"old\" }\n");
    Steno_Config reload_config = config;
    reload_config.dictionary_path = reload_path;
    Steno *reload_steno = steno_create(&reload_config);
    ok = ok && reload_steno != NULL;
    if (reload_steno != NULL) {
        uint64_t reload_bits = 0;
        ok = ok && stroke_string_to_bits("S", &reload_bits);

        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload initial dictionary", output.text, "old");

        ok = ok && write_text_file(reload_path, "{");
        ok = ok && !steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload keeps old dictionary on parse failure", output.text, " old");

        ok = ok && write_text_file(reload_path, "{ \"S\": \"newer\" }\n");
        ok = ok && steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("hot reload updated dictionary", output.text, " newer");

        Watch_Test watch = {
            .steno = reload_steno,
        };
        const char *const watch_paths[] = { reload_path };
        ok = ok && platform_file_watcher_start(
            watch_paths,
            sizeof(watch_paths) / sizeof(watch_paths[0]),
            test_dictionary_watch_callback,
            &watch
        );
        ok = ok && write_text_file(reload_path, "{ \"S\": \"watched\" }\n");
        for (size_t attempt = 0; ok && watch.reload_count == 0 && attempt < 50; ++attempt) {
            platform_file_watcher_poll();
            platform_sleep_ms(10);
        }
        platform_file_watcher_stop();
        ok = ok && watch.reload_count > 0;
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(reload_steno, reload_bits);
        ok = ok && expect_string("platform dictionary watcher reload", output.text, " watched");

        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
        ok = ok && write_text_file(reload_path, "{ \"S\": \"disabled\" }\n");
        ok = ok && steno_reload_dictionary(reload_steno);
        clear_test_output(&output);
        ok = ok && !send_key_event(reload_steno, "a", true);
        ok = ok && !send_key_event(reload_steno, "a", false);
        ok = ok && expect_string("hot reload while capture disabled suppresses output", output.text, "");

        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
        ok = ok && send_raw_key_event(reload_steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
        ok = ok && !send_raw_key_event(reload_steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
        clear_test_output(&output);
        ok = ok && send_key_event(reload_steno, "a", true);
        ok = ok && send_key_event(reload_steno, "a", false);
        ok = ok && expect_string("hot reload while disabled applies after reenable", output.text, " disabled");

        steno_destroy(reload_steno);
    }
    remove(reload_path);

    const char *dump_path = "build/test-dictionary-dump.json";
    ok = ok && steno_dump_dictionary_json(steno, dump_path);
    size_t dump_size = 0;
    char *dump = read_entire_file(dump_path, &dump_size);
    ok = ok && dump != NULL && dump_size > 0;
    if (dump != NULL) {
        ok = ok && strstr(dump, "\"STOER/Z\": \"stories\"") != NULL;
        free(dump);
    }
    remove(dump_path);

    FILE *trace_file = tmpfile();
    ok = ok && trace_file != NULL;
    if (trace_file != NULL) {
        Steno_Config trace_config = config;
        trace_config.trace_file = trace_file;
        Steno *trace_steno = steno_create(&trace_config);
        ok = ok && trace_steno != NULL;
        if (trace_steno != NULL) {
            uint64_t trace_bits = 0;
            clear_test_output(&output);
            ok = ok && stroke_string_to_bits("-T", &trace_bits);
            ok = ok && steno_handle_stroke_bits(trace_steno, trace_bits);
            ok = ok && handle_mock_pedal_stroke(trace_steno, PHRASE_NAMESPACE_INITIAL_VERB, "PW-B");
            ok = ok && handle_mock_pedal_stroke(trace_steno, PHRASE_NAMESPACE_INITIAL_VERB, "#KW");
            ok = ok && handle_mock_pedal_stroke(trace_steno, PHRASE_NAMESPACE_INITIAL_VERB, "SAO");
            ok = ok && expect_trace_contains(trace_file, "trace translated stroke", "-T -> the\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace phrase stroke",
                "PW-B [phrase] -> is a\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace phrase fallback dictionary stroke",
                "#KW [phrase fallback] -> test\n");
            ok = ok && expect_trace_contains(
                trace_file,
                "trace phrase fallback raw stroke",
                "SAO [phrase fallback] -> [untranslated]\n");
            steno_destroy(trace_steno);
        }
        fclose(trace_file);
    }

    clear_test_output(&output);
    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
    ok = ok && !send_key_event(steno, "u", true);
    ok = ok && !send_key_event(steno, "u", false);
    ok = ok && expect_string("ctrl escape disables capture", output.text, "");

    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, true, true, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, true, false, false, false);
    ok = ok && send_raw_key_event(steno, TEST_KEYCODE_ESCAPE, false, false, false, false);
    ok = ok && !send_raw_key_event(steno, TEST_KEYCODE_LEFT_CONTROL, false, false, false, false);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("ctrl escape reenables capture", output.text, "fee");

    ok = ok && reset_test_steno(&steno, &config);

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("first undoable translation", output.text, "fee");
    ok = ok && send_key_event(steno, "i", true);
    ok = ok && send_key_event(steno, "i", false);
    ok = ok && expect_string("second undoable translation", output.text, "fee pay");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("one level undo", output.text, "fee");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("two level undo", output.text, "");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("empty undo stack", output.text, "");

    clear_test_output(&output);
    ok = ok && send_key_event(steno, "r", true);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "r", false);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("unicode undoable translation", output.text, "caffè");
    ok = ok && send_key_event(steno, "j", true);
    ok = ok && send_key_event(steno, "j", false);
    ok = ok && expect_string("unicode undo", output.text, "");

    uint64_t story_bits = 0;
    uint64_t plural_bits = 0;
    uint64_t past_bits = 0;
    uint64_t stymie_first_bits = 0;
    uint64_t stymied_second_bits = 0;
    uint64_t history_bits = 0;
    uint64_t undo_bits = 0;
    uint64_t filler_bits = 0;
    uint64_t sap_bits = 0;
    uint64_t saps_bits = 0;
    uint64_t er_bits = 0;
    uint64_t erz_bits = 0;
    uint64_t cat_bits = 0;
    uint64_t stitch_a_bits = 0;
    uint64_t stitch_b_bits = 0;
    uint64_t stitch_c_bits = 0;
    uint64_t test_bits = 0;
    uint64_t phrase_fallback_test_bits = 0;
    uint64_t eye_bits = 0;
    uint64_t to_bits = 0;
    uint64_t hyphen_bits = 0;
    uint64_t stitch_word_bits = 0;
    uint64_t suffix_s_bits = 0;
    uint64_t red_bits = 0;
    uint64_t cherry_bits = 0;
    uint64_t cherries_bits = 0;
    uint64_t defer_bits = 0;
    uint64_t deferred_bits = 0;
    uint64_t failing_bits = 0;
    uint64_t nonfinal_d_suffix_bits = 0;
    uint64_t nonfinal_s_suffix_bits = 0;
    uint64_t nonfinal_g_suffix_bits = 0;
    uint64_t suffix_ish_bits = 0;
    uint64_t raw_ish_bits = 0;
    uint64_t prefix_bits = 0;
    uint64_t port_bits = 0;
    uint64_t delete_space_bits = 0;
    uint64_t force_space_bits = 0;
    uint64_t one_bits = 0;
    uint64_t two_bits = 0;
    uint64_t glue_p_bits = 0;
    uint64_t basket_bits = 0;
    uint64_t ball_bits = 0;
    uint64_t toggle_star_bits = 0;
    uint64_t retro_delete_space_bits = 0;
    uint64_t retro_insert_space_bits = 0;
    uint64_t caps_mode_bits = 0;
    uint64_t reset_mode_bits = 0;
    uint64_t snake_mode_bits = 0;
    uint64_t camel_mode_bits = 0;
    uint64_t lower_mode_bits = 0;
    uint64_t title_mode_bits = 0;
    uint64_t empty_space_mode_bits = 0;
    uint64_t reset_space_mode_bits = 0;
    uint64_t repeat_bits = 0;
    uint64_t period_bits = 0;
    uint64_t period_space_bits = 0;
    uint64_t comma_bits = 0;
    uint64_t cap_next_bits = 0;
    uint64_t upper_next_bits = 0;
    uint64_t lower_next_bits = 0;
    uint64_t lower_previous_bits = 0;
    uint64_t plover_bits = 0;
    uint64_t right_arrow_bits = 0;
    uint64_t modal_toggle_bits = 0;
    uint64_t phrase_is_a_bits = 0;
    ok = ok && stroke_string_to_bits("STOER", &story_bits);
    ok = ok && stroke_string_to_bits("-Z", &plural_bits);
    ok = ok && stroke_string_to_bits("-D", &past_bits);
    ok = ok && stroke_string_to_bits("STAOEU", &stymie_first_bits);
    ok = ok && stroke_string_to_bits("PHAOED", &stymied_second_bits);
    ok = ok && stroke_string_to_bits("HEU", &history_bits);
    ok = ok && stroke_string_to_bits("-R", &undo_bits);
    ok = ok && stroke_string_to_bits("#", &filler_bits);
    ok = ok && stroke_string_to_bits("SAP", &sap_bits);
    ok = ok && stroke_string_to_bits("SAPS", &saps_bits);
    ok = ok && stroke_string_to_bits("*ER", &er_bits);
    ok = ok && stroke_string_to_bits("*ERZ", &erz_bits);
    ok = ok && stroke_string_to_bits("KAT", &cat_bits);
    ok = ok && stroke_string_to_bits("A", &stitch_a_bits);
    ok = ok && stroke_string_to_bits("PW", &stitch_b_bits);
    ok = ok && stroke_string_to_bits("KR", &stitch_c_bits);
    ok = ok && stroke_string_to_bits("TEFT", &test_bits);
    ok = ok && stroke_string_to_bits("#KW", &phrase_fallback_test_bits);
    ok = ok && stroke_string_to_bits("AOEU", &eye_bits);
    ok = ok && stroke_string_to_bits("TO", &to_bits);
    ok = ok && stroke_string_to_bits("H-PB", &hyphen_bits);
    ok = ok && stroke_string_to_bits("-RBGS", &stitch_word_bits);
    ok = ok && stroke_string_to_bits("-S", &suffix_s_bits);
    ok = ok && stroke_string_to_bits("RED", &red_bits);
    ok = ok && stroke_string_to_bits("KHER", &cherry_bits);
    ok = ok && stroke_string_to_bits("KHERZ", &cherries_bits);
    ok = ok && stroke_string_to_bits("TKEFR", &defer_bits);
    ok = ok && stroke_string_to_bits("TKEFRD", &deferred_bits);
    ok = ok && stroke_string_to_bits("TPAEULG", &failing_bits);
    ok = ok && stroke_string_to_bits("WADZ", &nonfinal_d_suffix_bits);
    ok = ok && stroke_string_to_bits("KASD", &nonfinal_s_suffix_bits);
    ok = ok && stroke_string_to_bits("KAURBGS", &nonfinal_g_suffix_bits);
    ok = ok && stroke_string_to_bits("EURB", &suffix_ish_bits);
    ok = ok && stroke_string_to_bits("R-R", &raw_ish_bits);
    ok = ok && stroke_string_to_bits("PRAOE", &prefix_bits);
    ok = ok && stroke_string_to_bits("PORT", &port_bits);
    ok = ok && stroke_string_to_bits("TK-LS", &delete_space_bits);
    ok = ok && stroke_string_to_bits("S-P", &force_space_bits);
    ok = ok && stroke_string_to_bits("#S", &one_bits);
    ok = ok && stroke_string_to_bits("#T", &two_bits);
    ok = ok && stroke_string_to_bits("P*P", &glue_p_bits);
    ok = ok && stroke_string_to_bits("PWA", &basket_bits);
    ok = ok && stroke_string_to_bits("PWAL", &ball_bits);
    ok = ok && stroke_string_to_bits("#*", &toggle_star_bits);
    ok = ok && stroke_string_to_bits("SP-LS", &retro_delete_space_bits);
    ok = ok && stroke_string_to_bits("S-PD", &retro_insert_space_bits);
    ok = ok && stroke_string_to_bits("KA*PS", &caps_mode_bits);
    ok = ok && stroke_string_to_bits("R*EFT", &reset_mode_bits);
    ok = ok && stroke_string_to_bits("WRA", &snake_mode_bits);
    ok = ok && stroke_string_to_bits("WRO", &camel_mode_bits);
    ok = ok && stroke_string_to_bits("WRE", &lower_mode_bits);
    ok = ok && stroke_string_to_bits("WRU", &title_mode_bits);
    ok = ok && stroke_string_to_bits("TPHA", &empty_space_mode_bits);
    ok = ok && stroke_string_to_bits("KPAO", &reset_space_mode_bits);
    ok = ok && stroke_string_to_bits("SKWR", &repeat_bits);
    ok = ok && stroke_string_to_bits("TP-PL", &period_bits);
    ok = ok && stroke_string_to_bits("PH-FP", &period_space_bits);
    ok = ok && stroke_string_to_bits("KW-BG", &comma_bits);
    ok = ok && stroke_string_to_bits("KPA", &cap_next_bits);
    ok = ok && stroke_string_to_bits("KPA*L", &upper_next_bits);
    ok = ok && stroke_string_to_bits("HRO*ER", &lower_next_bits);
    ok = ok && stroke_string_to_bits("HRO*ERD", &lower_previous_bits);
    ok = ok && stroke_string_to_bits("PHROF", &plover_bits);
    ok = ok && stroke_string_to_bits("STPH-G", &right_arrow_bits);
    ok = ok && stroke_string_to_bits("STPH", &modal_toggle_bits);
    ok = ok && stroke_string_to_bits("PW-B", &phrase_is_a_bits);

    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && expect_string("phrase outline without namespace stays normal steno", output.text, "PW-B");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && handle_mock_pedal_stroke(steno, PHRASE_NAMESPACE_INITIAL_VERB, "PW-B");
    ok = ok && expect_string("initial verb is a", output.text, "is a");
    ok = ok && handle_mock_pedal_stroke(steno, PHRASE_NAMESPACE_INITIAL_VERB, "PW-T");
    ok = ok && expect_string("initial verb spacing", output.text, "is a is the");
    ok = ok && steno_handle_stroke_bits(steno, undo_bits);
    ok = ok && expect_string("initial verb undo", output.text, "is a");

    const struct {
        const char *stroke;
        const char *expected;
    } phrase_be_cases[] = {
        { "PW", "is" },
        { "PW-D", "was" },
        { "PW-T", "is the" },
        { "PW-TD", "was the" },
        { "PW-B", "is a" },
        { "PW-BD", "was a" },
        { "PW-PB", "is an" },
        { "PW-PBD", "was an" },
        { "PW-P", "is it" },
        { "PW-PD", "was it" },
        { "PW-RT", "is that" },
        { "PW-RTD", "was that" },
        { "PW-TS", "is this" },
        { "PW-TSD", "was this" },
        { "PW-SZ", "is these" },
        { "PW-SDZ", "was these" },
        { "PW-TZ", "is those" },
        { "PW-TDZ", "was those" },
        { "PW-PL", "is me" },
        { "PW-PLD", "was me" },
        { "PW-RP", "is you" },
        { "PW-RPD", "was you" },
        { "PW-R", "is your" },
        { "PW-RD", "was your" },
        { "PW-S", "is us" },
        { "PW-SD", "was us" },
        { "PW-FR", "is her" },
        { "PW-FRD", "was her" },
        { "PW-FL", "is him" },
        { "PW-FLD", "was him" },
        { "PW-RB", "is she" },
        { "PW-RBD", "was she" },
        { "PW-RBL", "is she will" },
        { "PW-RBLD", "was she will" },
        { "PW-RBLT", "is she'll" },
        { "PW-RBLTD", "was she'll" },
        { "PW-RPB", "is he" },
        { "PW-RPBD", "was he" },
        { "PW-RPBL", "is he will" },
        { "PW-RPBLD", "was he will" },
        { "PW-RPBLT", "is he'll" },
        { "PW-RPBLTD", "was he'll" },
        { "PW-GT", "is going to" },
        { "PW-GTD", "was going to" },
        { "PW-G", "is give" },
        { "PW-GD", "was give" },
        { "PW-BGT", "is why" },
        { "PW-BGTD", "was why" },
        { "PW-RPL", "is who" },
        { "PW-RPLD", "was who" },
        { "PW-BLG", "is what" },
        { "PW-BLGD", "was what" },
        { "PW-PBG", "is when" },
        { "PW-PBGD", "was when" },
        { "PW-RLG", "is where" },
        { "PW-RLGD", "was where" },
        { "PW-PLG", "is how" },
        { "PW-PLGD", "was how" },
        { "PW-PLT", "is them" },
        { "PW-PLTD", "was them" },
        { "PW-L", "is all" },
        { "PW-LD", "was all" },
        { "PW-PBT", "is one" },
        { "PW-PBTD", "was one" },
    };
    for (size_t i = 0; i < sizeof(phrase_be_cases) / sizeof(phrase_be_cases[0]); ++i) {
        ok = ok && expect_mock_pedal_phrase(
            &steno,
            &config,
            &output,
            "initial verb be tail inventory",
            PHRASE_NAMESPACE_INITIAL_VERB,
            phrase_be_cases[i].stroke,
            phrase_be_cases[i].expected);
    }

    const struct {
        const char *stroke;
        const char *expected;
    } nonverb_cases[] = {
        { "W-RT", "with that" },
        { "TPHR-RPB", "unless he" },
        { "TPH-F", "even if" },
        { "TPH-PBG", "even when" },
        { "S-F", "as if" },
        { "K-L", "even though" },
        { "T-P", "one of them" },
        { "-F", "anything else" },
    };
    for (size_t i = 0; i < sizeof(nonverb_cases) / sizeof(nonverb_cases[0]); ++i) {
        ok = ok && expect_mock_pedal_phrase(
            &steno,
            &config,
            &output,
            "nonverb phrase inventory",
            PHRASE_NAMESPACE_NONVERB,
            nonverb_cases[i].stroke,
            nonverb_cases[i].expected);
    }

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_mock_pedal_stroke(steno, PHRASE_NAMESPACE_INITIAL_VERB, "SAO");
    ok = ok && expect_string("initial verb miss falls back to raw outline", output.text, "SAO");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_mock_pedal_stroke(steno, PHRASE_NAMESPACE_INITIAL_VERB, "#KW");
    ok = ok && expect_string("initial verb miss falls back to dictionary", output.text, "test");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && steno_handle_stroke_bits(steno, phrase_fallback_test_bits);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("held phrase pedal allows dictionary word between phrases", output.text, "is a test is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && send_key_event(steno, "e", true);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "comma", true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "comma", false);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("qwerty phrase namespace routes gathered chord", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "e", true);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "comma", true);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "comma", false);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("qwerty phrase namespace can begin mid-chord", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && !send_key_event(steno, "left_shift", true);
    ok = ok && !send_key_event(steno, "left_shift", false);
    ok = ok && send_key_event(steno, "e", true);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "comma", true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "comma", false);
    ok = ok && expect_string("qwerty shift tap routes next chord through phrase namespace", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "right_shift", true);
    ok = ok && send_key_event(steno, "right_shift", false);
    ok = ok && expect_string("qwerty mapped shift tap emits nothing", output.text, "");
    ok = ok && send_key_event(steno, "e", true);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "comma", true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "comma", false);
    ok = ok && expect_string("qwerty mapped shift tap routes next chord through phrase namespace", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("serial phrase namespace routes direct stroke", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && expect_string("serial phrase namespace latches tapped pedal", output.text, "is a");
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && expect_string("serial phrase namespace latch clears after stroke", output.text, "is a PW-B");

    ok = ok && reset_test_steno(&steno, &config);

    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, story_bits);
    ok = ok && expect_string("story first stroke", output.text, "story");
    ok = ok && output.send_count == 1 && output.delete_count == 0;
    ok = ok && expect_string("story send text", output.last_send, "story");

    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, plural_bits);
    ok = ok && expect_string("stories retroactive replacement", output.text, "stories");
    ok = ok && output.send_count == 1 && output.delete_count == 1;
    ok = ok && expect_string("stories minimal delete", output.last_delete, "y");
    ok = ok && expect_string("stories minimal insert", output.last_send, "ies");

    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, undo_bits);
    ok = ok && expect_string("undo restores replaced translation", output.text, "story");
    ok = ok && output.send_count == 1 && output.delete_count == 1;
    ok = ok && expect_string("undo stories delete", output.last_delete, "ies");
    ok = ok && expect_string("undo stories insert", output.last_send, "y");

    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, past_bits);
    ok = ok && expect_string("past tense after undo uses restored stroke history", output.text, "storied");
    ok = ok && output.send_count == 1 && output.delete_count == 1;
    ok = ok && expect_string("storied minimal delete", output.last_delete, "y");
    ok = ok && expect_string("storied minimal insert", output.last_send, "ied");

    Steno *suffix_key_steno = steno_create(&config);
    ok = ok && suffix_key_steno != NULL;
    if (suffix_key_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, saps_bits);
        ok = ok && expect_string("suffix key single stroke", output.text, "saps");
        ok = ok && expect_string("suffix key single stroke send", output.last_send, "saps");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, history_bits);
        ok = ok && expect_string("suffix key multi-stroke first raw", output.text, "HEU");
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, saps_bits);
        ok = ok && expect_string("suffix key multi-stroke", output.text, "history saps");
        ok = ok && expect_string("suffix key multi-stroke delete", output.last_delete, "HEU");
        ok = ok && expect_string("suffix key multi-stroke insert", output.last_send, "history saps");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, cherries_bits);
        ok = ok && expect_string("suffix key z orthography", output.text, "cherries");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, deferred_bits);
        ok = ok && expect_string("suffix key d orthography", output.text, "deferred");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, failing_bits);
        ok = ok && expect_string("suffix key g", output.text, "failing");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, stymie_first_bits);
        ok = ok && expect_string("suffix key ie-ed first stroke", output.text, "sty");
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, stymied_second_bits);
        ok = ok && expect_string("suffix key ie-ed orthography", output.text, "stymied");
        ok = ok && expect_string("suffix key ie-ed delete", output.last_delete, "");
        ok = ok && expect_string("suffix key ie-ed insert", output.last_send, "mied");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, nonfinal_d_suffix_bits);
        ok = ok && expect_string("suffix key d must be final", output.text, "WADZ");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, nonfinal_s_suffix_bits);
        ok = ok && expect_string("suffix key s must be final", output.text, "KASD");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, nonfinal_g_suffix_bits);
        ok = ok && expect_string("suffix key g must be final", output.text, "KAURBGS");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, sap_bits);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, er_bits);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, suffix_s_bits);
        ok = ok && expect_string("separate attach suffix strokes", output.text, "sappers");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, sap_bits);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, erz_bits);
        ok = ok && expect_string("suffix key attaches base suffix to previous word", output.text, "sappers");
        ok = ok && expect_string("suffix key attach delete", output.last_delete, "");
        ok = ok && expect_string("suffix key attach insert", output.last_send, "pers");

        steno_destroy(suffix_key_steno);
    }

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, history_bits);
    ok = ok && steno_handle_stroke_bits(steno, story_bits);
    ok = ok && steno_handle_stroke_bits(steno, plural_bits);
    ok = ok && expect_string("longest multi-stroke replacement", output.text, "histories");
    ok = ok && expect_string("longest multi-stroke delete", output.last_delete, "HEU story");
    ok = ok && expect_string("longest multi-stroke insert", output.last_send, "histories");

    Steno *compact_steno = steno_create(&config);
    ok = ok && compact_steno != NULL;
    if (compact_steno != NULL) {
        clear_test_output(&output);
        for (size_t i = 0; ok && i < 1999; ++i) {
            ok = ok && steno_handle_stroke_bits(compact_steno, filler_bits);
        }

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, story_bits);
        ok = ok && expect_size_at_most(
            "compacted history stroke count",
            steno_translation_history_stroke_count(compact_steno),
            1000
        );
        ok = ok && expect_string("compaction keeps current stroke output", output.text, " story");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, plural_bits);
        ok = ok && expect_string("retro translation after compaction", output.text, " stories");
        ok = ok && expect_string("retro delete after compaction", output.last_delete, "y");
        ok = ok && expect_string("retro insert after compaction", output.last_send, "ies");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, undo_bits);
        ok = ok && expect_string("undo after compaction", output.text, " story");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(compact_steno, past_bits);
        ok = ok && expect_string("translation after compacted undo", output.text, " storied");

        steno_destroy(compact_steno);
    }

    Steno *format_steno = steno_create(&config);
    ok = ok && expect_size("format steno created", format_steno != NULL ? 1 : 0, 1);
    if (format_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("attach base word", output.text, "cat");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, suffix_s_bits);
        ok = ok && expect_string("attach suffix", output.text, "cats");
        ok = ok && expect_size("attach suffix send count", output.send_count, 1);
        ok = ok && expect_size("attach suffix delete count", output.delete_count, 0);
        ok = ok && expect_string("attach suffix delete", output.last_delete, "");
        ok = ok && expect_string("attach suffix insert", output.last_send, "s");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, undo_bits);
        ok = ok && expect_string("undo attach suffix", output.text, "cat");
        ok = ok && expect_string("undo attach suffix delete", output.last_delete, "s");
        ok = ok && expect_string("undo attach suffix insert", output.last_send, "");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, past_bits);
        ok = ok && expect_string("orthographic ed suffix", output.text, "catted");
        ok = ok && expect_string("orthographic ed delete", output.last_delete, "");
        ok = ok && expect_string("orthographic ed insert", output.last_send, "ted");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, red_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, suffix_ish_bits);
        ok = ok && expect_string("orthographic ish suffix", output.text, "reddish");
        ok = ok && expect_string("orthographic ish delete", output.last_delete, "");
        ok = ok && expect_string("orthographic ish insert", output.last_send, "dish");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, red_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, raw_ish_bits);
        ok = ok && expect_string("raw ish suffix", output.text, "redish");
        ok = ok && expect_string("raw ish delete", output.last_delete, "");
        ok = ok && expect_string("raw ish insert", output.last_send, "ish");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cherry_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, suffix_s_bits);
        ok = ok && expect_string("orthographic y plural", output.text, "cherries");
        ok = ok && expect_string("orthographic y plural delete", output.last_delete, "y");
        ok = ok && expect_string("orthographic y plural insert", output.last_send, "ies");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, defer_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, past_bits);
        ok = ok && expect_string("orthographic doubled consonant", output.text, "deferred");
        ok = ok && expect_string("orthographic doubled consonant delete", output.last_delete, "");
        ok = ok && expect_string("orthographic doubled consonant insert", output.last_send, "red");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, prefix_bits);
        ok = ok && expect_string("prefix attach first stroke", output.text, "pre");
        ok = ok && expect_string("prefix attach first send", output.last_send, "pre");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, port_bits);
        ok = ok && expect_string("prefix attach next word", output.text, "preport");
        ok = ok && expect_string("prefix attach next send", output.last_send, "port");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, basket_bits);
        ok = ok && expect_string("delete-space base word", output.text, "basket");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, delete_space_bits);
        ok = ok && expect_string("delete-space command", output.text, "basket");
        ok = ok && expect_size("delete-space send count", output.send_count, 0);
        ok = ok && expect_size("delete-space delete count", output.delete_count, 0);
        ok = ok && expect_string("delete-space delete", output.last_delete, "");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, ball_bits);
        ok = ok && expect_string("delete-space next word", output.text, "basketball");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, prefix_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, force_space_bits);
        ok = ok && expect_string("force-space command", output.text, "pre ");
        ok = ok && expect_string("force-space insert", output.last_send, " ");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, port_bits);
        ok = ok && expect_string("force-space next word", output.text, "pre port");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, basket_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, ball_bits);
        ok = ok && expect_string("retro delete-space base phrase", output.text, "basket ball");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, retro_delete_space_bits);
        ok = ok && expect_string("retro delete-space command", output.text, "basketball");
        ok = ok && expect_string("retro delete-space delete", output.last_delete, " ball");
        ok = ok && expect_string("retro delete-space insert", output.last_send, "ball");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, retro_insert_space_bits);
        ok = ok && expect_string("retro insert-space command", output.text, "basket ball");
        ok = ok && expect_string("retro insert-space delete", output.last_delete, "ball");
        ok = ok && expect_string("retro insert-space insert", output.last_send, " ball");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("star toggle base word", output.text, "cat");
        ok = ok && steno_handle_stroke_bits(format_steno, toggle_star_bits);
        ok = ok && expect_string("star toggle translated stroke", output.text, "kitty");
        ok = ok && expect_string("star toggle delete", output.last_delete, "cat");
        ok = ok && expect_string("star toggle insert", output.last_send, "kitty");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, period_bits);
        ok = ok && expect_string("period attaches and sets capitalization", output.text, "cat.");
        ok = ok && expect_string("period delete", output.last_delete, "");
        ok = ok && expect_string("period insert", output.last_send, ".");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("capitalization after period", output.text, "cat. Cat");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, undo_bits);
        ok = ok && expect_string("undo after capitalized word", output.text, "cat.");
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("capitalization after undo redo", output.text, "cat. Cat");
        ok = ok && expect_string("capitalization after undo redo insert", output.last_send, " Cat");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, period_space_bits);
        ok = ok && expect_string("period with literal space", output.text, "cat. ");
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("period literal space before next word", output.text, "cat. Cat");
        ok = ok && expect_string("period literal space avoids double spacing", output.last_send, "Cat");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, undo_bits);
        ok = ok && expect_string("undo after literal-space punctuation", output.text, "cat. ");
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("literal-space capitalization after undo redo", output.text, "cat. Cat");
        ok = ok && expect_string("literal-space capitalization redo insert", output.last_send, "Cat");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, comma_bits);
        ok = ok && expect_string("comma attaches without capitalization", output.text, "cat,");
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("comma leaves next word lower-case", output.text, "cat, cat");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cap_next_bits);
        ok = ok && expect_string("case command emits nothing", output.text, "");
        ok = ok && expect_size("case command send count", output.send_count, 0);
        ok = ok && expect_size("case command delete count", output.delete_count, 0);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("cap next word", output.text, "Cat");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, upper_next_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string("upper next word", output.text, "CAT");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, lower_next_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, plover_bits);
        ok = ok && expect_string("lower next word", output.text, "plover");

        ok = ok && reset_test_steno(&format_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, plover_bits);
        ok = ok && steno_handle_stroke_bits(format_steno, lower_previous_bits);
        ok = ok && expect_string("retro lower previous word", output.text, "plover");
        ok = ok && expect_string("retro lower delete", output.last_delete, "Plover");
        ok = ok && expect_string("retro lower insert", output.last_send, "plover");

        steno_destroy(format_steno);
    }

    Steno *mode_steno = steno_create(&config);
    ok = ok && expect_size("mode steno created", mode_steno != NULL ? 1 : 0, 1);
    if (mode_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, caps_mode_bits);
        ok = ok && expect_string("caps mode emits nothing", output.text, "");
        ok = ok && expect_size("caps mode send count", output.send_count, 0);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && expect_string("caps mode word", output.text, "CAT");

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, caps_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("mode command preserves word spacing", output.text, "cat BALL");

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && expect_string("reset mode word", output.text, "cat");

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, lower_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, plover_bits);
        ok = ok && expect_string("lower mode word", output.text, "plover");

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, title_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && expect_string("title mode word", output.text, "Cat");

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, snake_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("snake mode spacing", output.text, "cat_ball");

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, empty_space_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("empty set_space mode", output.text, "catball");
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_space_mode_bits);

        ok = ok && reset_test_steno(&mode_steno, &config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(mode_steno, reset_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, camel_mode_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(mode_steno, ball_bits);
        ok = ok && expect_string("camel mode spacing and case", output.text, "catBall");

        steno_destroy(mode_steno);
    }

    Steno *key_combo_steno = steno_create(&config);
    ok = ok && expect_size("key combo steno created", key_combo_steno != NULL ? 1 : 0, 1);
    if (key_combo_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(key_combo_steno, right_arrow_bits);
        ok = ok && expect_string("key combo command emits no text", output.text, "");
        ok = ok && expect_size("key combo command count", output.key_combo_count, 1);
        ok = ok && expect_string("key combo command", output.last_key_combo, "Right");
        steno_destroy(key_combo_steno);
    }

    Steno *digit_steno = steno_create(&config);
    ok = ok && digit_steno != NULL;
    if (digit_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(digit_steno, one_bits);
        ok = ok && expect_string("digit glue first", output.text, "1");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(digit_steno, two_bits);
        ok = ok && expect_string("digit glue second", output.text, "12");
        ok = ok && expect_string("digit glue delete", output.last_delete, "");
        ok = ok && expect_string("digit glue insert", output.last_send, "2");

        steno_destroy(digit_steno);
    }

    Steno *glue_steno = steno_create(&config);
    ok = ok && glue_steno != NULL;
    if (glue_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(glue_steno, glue_p_bits);
        ok = ok && expect_string("explicit glue first", output.text, "P");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(glue_steno, glue_p_bits);
        ok = ok && expect_string("explicit glue second", output.text, "PP");
        ok = ok && expect_string("explicit glue delete", output.last_delete, "");
        ok = ok && expect_string("explicit glue insert", output.last_send, "P");

        steno_destroy(glue_steno);
    }

    Steno *repeat_steno = steno_create(&config);
    ok = ok && repeat_steno != NULL;
    if (repeat_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(repeat_steno, cat_bits);
        ok = ok && steno_handle_stroke_bits(repeat_steno, repeat_bits);
        ok = ok && expect_string("repeat last translation", output.text, "cat cat");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(repeat_steno, undo_bits);
        ok = ok && expect_string("undo repeated translation", output.text, "cat");
        ok = ok && expect_string("undo repeat delete", output.last_delete, " cat");

        steno_destroy(repeat_steno);
    }

    Steno *stitch_steno = steno_create(&config);
    ok = ok && stitch_steno != NULL;
    if (stitch_steno != NULL) {
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_a_bits);
        ok = ok && expect_string("stitch first letter", output.text, "A");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_b_bits);
        ok = ok && expect_string("stitch second letter", output.text, "A-B");
        ok = ok && expect_string("stitch second delete", output.last_delete, "");
        ok = ok && expect_string("stitch second insert", output.last_send, "-B");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_c_bits);
        ok = ok && expect_string("stitch third letter", output.text, "A-B-C");

        ok = ok && reset_test_steno(&stitch_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, test_bits);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_word_bits);
        ok = ok && expect_string("stitch last word", output.text, "t-e-s-t");

        ok = ok && reset_test_steno(&stitch_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(stitch_steno, eye_bits);
        ok = ok && steno_handle_stroke_bits(stitch_steno, to_bits);
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_word_bits);
        ok = ok && expect_string("stitch last word first command", output.text, "eye t-o");
        ok = ok && steno_handle_stroke_bits(stitch_steno, stitch_word_bits);
        ok = ok && expect_string("stitch last word superseded command", output.text, "e-y-e t-o");

        steno_destroy(stitch_steno);
    }

    const char *layered_paths[] = {
        "tests/test-dictionary.json",
        "tests/test-modal-dictionary.json",
        "tests/test-custom-dictionary.json",
    };
    const bool layered_enabled[] = {
        true,
        false,
        true,
    };
    Steno_Config layered_config = config;
    layered_config.dictionary_path = NULL;
    layered_config.dictionary_paths = layered_paths;
    layered_config.dictionary_enabled = layered_enabled;
    layered_config.dictionary_path_count = sizeof(layered_paths) / sizeof(layered_paths[0]);
    Steno *layered_steno = steno_create(&layered_config);
    ok = ok && layered_steno != NULL;
    if (layered_steno != NULL) {
        const char *kitten = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "KAT", &kitten);
        ok = ok && expect_string("dictionary override", kitten, "kitten");

        const char *modal_off_undo = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &modal_off_undo);
        ok = ok && expect_string("disabled modal dictionary does not override", modal_off_undo, "=undo");

        const char *modal_toggle = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "STPH", &modal_toggle);
        ok = ok && expect_string("custom modal toggle command", modal_toggle, "{plover:toggle_dict:!test-modal-dictionary.json}");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(layered_steno, modal_toggle_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, undo_bits);
        ok = ok && expect_string("enabled modal movement emits no text", output.text, "");
        ok = ok && expect_size("enabled modal movement key combo count", output.key_combo_count, 1);
        ok = ok && expect_string("enabled modal movement key combo", output.last_key_combo, "Left");

        const char *modal_on_left = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &modal_on_left);
        ok = ok && expect_string("enabled modal dictionary overrides", modal_on_left, "{#Left}{^}");

        ok = ok && steno_handle_stroke_bits(layered_steno, modal_toggle_bits);
        const char *modal_off_again = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &modal_off_again);
        ok = ok && expect_string("modal dictionary toggles off", modal_off_again, "=undo");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(layered_steno, eye_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, hyphen_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, to_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, hyphen_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, eye_bits);
        ok = ok && expect_string("hyphen command between words", output.text, "eye-to-eye");

        steno_destroy(layered_steno);
    }

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "q", true);
    ok = ok && send_key_event(steno, "q", false);
    ok = ok && expect_string("raw # chord", output.text, "#");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_session_active(steno, false);
    ok = ok && !send_key_event(steno, "u", true);
    ok = ok && !send_key_event(steno, "u", false);
    ok = ok && !steno_handle_stroke_bits(steno, gemini_bits);
    ok = ok && expect_string("inactive session suppresses output", output.text, "");
    steno_set_session_active(steno, true);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("active session resumes output", output.text, "fee");

    const char *star_keys[] = { "t", "g", "b", "y", "h", "n" };
    for (size_t i = 0; i < sizeof(star_keys) / sizeof(star_keys[0]); ++i) {
        ok = ok && reset_test_steno(&steno, &config);
        clear_test_output(&output);
        ok = ok && send_key_event(steno, star_keys[i], true);
        ok = ok && send_key_event(steno, star_keys[i], false);
        ok = ok && expect_string("star key mapping", output.text, "*");
    }

    Steno_Config empty_config = config;
    empty_config.dictionary_path = "tests/empty-dictionary.json";
    Steno *empty_steno = steno_create(&empty_config);
    ok = ok && empty_steno != NULL;
    if (empty_steno != NULL) {
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "u", true);
        ok = ok && send_key_event(empty_steno, "u", false);
        ok = ok && expect_string("empty dictionary raw chord", output.text, "F");

        ok = ok && reset_test_steno(&empty_steno, &empty_config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(empty_steno, gemini_bits);
        ok = ok && expect_string("Gemini PR raw stroke", output.text, "#*Z");

        ok = ok && reset_test_steno(&empty_steno, &empty_config);
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "z", true);
        ok = ok && send_key_event(empty_steno, "z", false);
        ok = ok && expect_string("left multi-bit key", output.text, "#S");

        ok = ok && reset_test_steno(&empty_steno, &empty_config);
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "m", true);
        ok = ok && send_key_event(empty_steno, "m", false);
        ok = ok && expect_string("right multi-bit key implicit hyphen", output.text, "FR");

        ok = ok && reset_test_steno(&empty_steno, &empty_config);
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "comma", true);
        ok = ok && send_key_event(empty_steno, "comma", false);
        ok = ok && expect_string("right multi-bit key explicit hyphen", output.text, "-PB");

        ok = ok && reset_test_steno(&empty_steno, &empty_config);
        clear_test_output(&output);
        ok = ok && send_key_event(empty_steno, "a", true);
        ok = ok && send_key_event(empty_steno, "space", true);
        ok = ok && send_key_event(empty_steno, "i", true);
        ok = ok && send_key_event(empty_steno, "a", false);
        ok = ok && send_key_event(empty_steno, "space", false);
        ok = ok && send_key_event(empty_steno, "i", false);
        ok = ok && expect_string("empty dictionary raw drill chord", output.text, "SAP");
        steno_destroy(empty_steno);
    }

    Steno_Config gemini_empty_config = empty_config;
    gemini_empty_config.keymap_path = NULL;
    Steno *gemini_empty_steno = steno_create(&gemini_empty_config);
    ok = ok && gemini_empty_steno != NULL;
    if (gemini_empty_steno != NULL) {
        clear_test_output(&output);
        ok = ok && steno_handle_stroke_bits(gemini_empty_steno, gemini_bits);
        ok = ok && expect_string("Gemini PR no-keymap raw stroke", output.text, "#*Z");
        steno_destroy(gemini_empty_steno);
    }

    arrfree(output.text);
    steno_destroy(steno);

    if (!ok) {
        return 1;
    }

    puts("test_steno: ok");
    return 0;
}
