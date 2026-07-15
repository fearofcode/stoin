#include "test_support.h"

#include "gemini_pr.h"
#include "orthography.h"
#include "phrasing.h"
#include "runtime_config.h"
#include "stentura.h"
#include "steno_stroke.h"
#include "stroke_merge.h"
#include "tx_bolt.h"

#include <string.h>

bool test_core_units(void)
{
    bool ok = true;

    uint16_t f13_keycode = 0;
    const bool f13_resolved = platform_keycode_from_name("F13", &f13_keycode);
    ok = ok && expect_size("F13 key resolves", f13_resolved ? 1 : 0, 1);
    ok = ok && expect_size("F13 keycode", f13_keycode, 105);
    uint16_t f16_keycode = 0;
    const bool f16_resolved = platform_keycode_from_name("F16", &f16_keycode);
    ok = ok && expect_size("F16 key resolves", f16_resolved ? 1 : 0, 1);
    ok = ok && expect_size("F16 keycode", f16_keycode, 106);

    const char *runtime_config_path = "build/test-runtime-config.json";
    ok = ok && write_text_file(
        runtime_config_path,
        "{\"phrasing\":\"tests/test-phrasing.json\"}\n"
    );
    Runtime_Config runtime_config = {0};
    ok = ok && runtime_config_load(&runtime_config, runtime_config_path, false);
    ok = ok && expect_string(
        "phrasing config field",
        runtime_config.phrasing_path,
        "tests/test-phrasing.json"
    );
    runtime_config_destroy(&runtime_config);
    remove(runtime_config_path);

    Phrasing *production_phrasing = phrasing_load("phrasing.json");
    ok = ok && expect_size("production phrasing loads", production_phrasing != NULL ? 1 : 0, 1);
    phrasing_destroy(production_phrasing);

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
    ok = ok && expect_stroke_format("-ER", "ER");
    ok = ok && expect_stroke_format("AO-E", "AOE");
    ok = ok && expect_stroke_format("TKA-EURB", "TKAEURB");
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


    return ok;
}
