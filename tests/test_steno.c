#include "gemini_pr.h"
#include "test_support.h"

#include "steno_stroke.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool test_core_units(void);
bool test_dictionary_runtime(void);

int main(void)
{
    Test_Output output = {0};
    Steno_Config config = test_steno_config(&output);
    bool ok = true;

    ok = ok && test_core_units();
    ok = ok && test_dictionary_runtime();

    Steno *steno = steno_create(&config);
    if (steno == NULL) {
        fputs("test failed: could not create steno engine\n", stderr);
        test_output_destroy(&output);
        return 1;
    }

    uint64_t gemini_bits = 0;
    const uint8_t gemini_number_star_z[GEMINI_PR_PACKET_SIZE] = { 0xA0, 0x00, 0x08, 0x00, 0x00, 0x01 };
    ok = ok && gemini_pr_decode_packet(gemini_number_star_z, &gemini_bits);

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
    uint64_t stacking_bits = 0;
    uint64_t dz_prefix_conflict_bits = 0;
    uint64_t dz_d_prefix_bits = 0;
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
    uint64_t dictionary_toggle_bits = 0;
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
    ok = ok && stroke_string_to_bits("STABGDZ", &stacking_bits);
    ok = ok && stroke_string_to_bits("TPADZ", &dz_prefix_conflict_bits);
    ok = ok && stroke_string_to_bits("KWADZ", &dz_d_prefix_bits);
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
    ok = ok && stroke_string_to_bits("STPH", &dictionary_toggle_bits);
    ok = ok && stroke_string_to_bits("PW-B", &phrase_is_a_bits);


    config.phrasing_path = "phrasing.json";
    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && handle_test_stroke(steno, "PW-B");
    ok = ok && expect_string(
        "ordinary dictionary namespace ignores phrase tables",
        output.text,
        "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && handle_test_stroke(steno, "PW-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("initial verb pedal selects IV", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && handle_test_stroke(steno, "PH-B");
    ok = ok && handle_test_stroke(steno, "PH-BL");
    ok = ok && handle_test_stroke(steno, "T-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string(
        "mnemonic IV stems remain disjoint from shared tails",
        output.text,
        "makes a makes like takes a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_FINAL_VERB, true);
    ok = ok && handle_test_stroke(steno, "SK-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_FINAL_VERB, false);
    ok = ok && expect_string("final verb pedal selects FV", output.text, "she is");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, true);
    ok = ok && handle_test_stroke(steno, "WH-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, false);
    ok = ok && expect_string("nonverb pedal selects NV", output.text, "with a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && handle_test_stroke(steno, "SK-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("same chord has IV meaning", output.text, "asks a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, true);
    ok = ok && handle_test_stroke(steno, "SK-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, false);
    ok = ok && expect_string("same chord has NV meaning", output.text, "she a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && handle_test_stroke(steno, "PW-T");
    ok = ok && expect_string("pedal tap arms the next stroke", output.text, "is the");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && handle_test_stroke(steno, "PW-B");
    ok = ok && handle_test_stroke(steno, "PW-T");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("held pedal applies to consecutive strokes", output.text, "is a is the");
    ok = ok && steno_handle_stroke_bits(steno, undo_bits);
    ok = ok && expect_string("phrase output participates in undo", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_FINAL_VERB, true);
    ok = ok && handle_test_stroke(steno, "PW-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_FINAL_VERB, false);
    ok = ok && expect_string(
        "simultaneous pedals do not choose an arbitrary namespace",
        output.text,
        "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, true);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_fallback_test_bits,
    }));
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, false);
    ok = ok && expect_string("pedal phrase miss falls back to dictionary", output.text, "test");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, true);
    ok = ok && handle_test_stroke(steno, "KAT");
    ok = ok && handle_test_stroke(steno, "TO");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, false);
    ok = ok && expect_string(
        "pedal misses preserve multi-stroke dictionary lookup",
        output.text,
        "main combined");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_is_a_bits,
        .phrase_namespace = PHRASE_NAMESPACE_INITIAL_VERB,
    }));
    ok = ok && steno_handle_stroke_bits(steno, phrase_fallback_test_bits);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, true);
    ok = ok && handle_test_stroke(steno, "WH-B");
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_NONVERB, false);
    ok = ok && expect_string(
        "pedal phrases and dictionary words interleave",
        output.text,
        "is a test with a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    steno_set_session_active(steno, false);
    steno_set_session_active(steno, true);
    ok = ok && handle_test_stroke(steno, "PW-B");
    ok = ok && expect_string("session reset clears pedal state", output.text, "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "c", true);
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "c", false);
    ok = ok && send_key_event(steno, "k", false);
    ok = ok && expect_string("qwerty chord stays in dictionary without pedal", output.text, "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && send_key_event(steno, "c", true);
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "c", false);
    ok = ok && send_key_event(steno, "k", false);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("qwerty chord uses held IV pedal", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "c", true);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, true);
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "c", false);
    ok = ok && send_key_event(steno, "k", false);
    steno_set_phrase_namespace(steno, PHRASE_NAMESPACE_INITIAL_VERB, false);
    ok = ok && expect_string("pedal press during qwerty chord selects IV", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && !send_key_event(steno, "left_shift", true);
    ok = ok && !send_key_event(steno, "left_shift", false);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("qwerty shift tap has no side effect", output.text, "fee");

    config.phrasing_path = "tests/test-phrasing.json";
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

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "ET");
    ok = ok && handle_test_stroke(steno, "SET");
    ok = ok && handle_test_stroke(steno, "RA");
    ok = ok && expect_string("multi-stroke match without prefix", output.text, "et cetera");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "KAT");
    ok = ok && handle_test_stroke(steno, "ET");
    ok = ok && handle_test_stroke(steno, "SET");
    ok = ok && handle_test_stroke(steno, "RA");
    ok = ok && expect_string(
        "multi-stroke match after attached prefix",
        output.text,
        "cat et cetera");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "APL");
    ok = ok && handle_test_stroke(steno, "PAOU");
    ok = ok && expect_string(
        "forward-attached prefix with provisional outline",
        output.text,
        "ampew");
    reset_output_log(&output);
    ok = ok && handle_test_stroke(steno, "TAEUGZ");
    ok = ok && expect_string(
        "retroactive left attachment replaces provisional outline",
        output.text,
        "amputation");
    ok = ok && expect_string("retroactive left attachment delete", output.last_delete, "ew");
    ok = ok && expect_string("retroactive left attachment insert", output.last_send, "utation");
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, undo_bits);
    ok = ok && expect_string(
        "undo retroactive left attachment",
        output.text,
        "ampew");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "PAOU");
    ok = ok && handle_test_stroke(steno, "TAEUGZ");
    ok = ok && expect_string(
        "retroactive left attachment without a prefix",
        output.text,
        "putation");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "KHER");
    ok = ok && handle_test_stroke(steno, "PAOU");
    ok = ok && handle_test_stroke(steno, "TAEUGS");
    ok = ok && expect_string(
        "retroactive left attachment applies orthography to preceding word",
        output.text,
        "cherries");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "APL");
    ok = ok && handle_test_stroke(steno, "PAOU");
    ok = ok && handle_test_stroke(steno, "TAEUGD");
    ok = ok && expect_string(
        "retroactive left attachment with suffix-key fallback",
        output.text,
        "amputated");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "KAT");
    ok = ok && handle_test_stroke(steno, "ET");
    ok = ok && handle_test_stroke(steno, "SET");
    ok = ok && handle_test_stroke(steno, "RAS");
    ok = ok && expect_string(
        "suffix match after attached prefix",
        output.text,
        "cat et ceteras");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "KU");
    ok = ok && handle_test_stroke(steno, "KUS");
    ok = ok && expect_string(
        "suffix fallback does not steal exact final stroke",
        output.text,
        "can you can you say");

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
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, stacking_bits);
        ok = ok && expect_string("suffix key dz uses ing", output.text, "stacking");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, dz_prefix_conflict_bits);
        ok = ok && expect_string(
            "suffix key dz does not steal existing prefix z",
            output.text,
            "TPADZ");

        ok = ok && reset_test_steno(&suffix_key_steno, &config);
        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(suffix_key_steno, dz_d_prefix_bits);
        ok = ok && expect_string(
            "suffix key dz does not steal existing prefix d",
            output.text,
            "quads");

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
        ok = ok && steno_handle_stroke_bits(format_steno, period_bits);
        ok = ok && handle_test_stroke(format_steno, "HURD");
        ok = ok && expect_string("capitalized provisional dictionary outline", output.text, "cat. Heard");

        reset_output_log(&output);
        ok = ok && handle_test_stroke(format_steno, "R-R");
        ok = ok && expect_string(
            "retroactive dictionary replacement preserves capitalization",
            output.text,
            "cat. Herd");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, plural_bits);
        ok = ok && expect_string(
            "chained retroactive replacement preserves capitalization",
            output.text,
            "cat. Herds");

        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(format_steno, cat_bits);
        ok = ok && expect_string(
            "retroactive replacement preserves resulting capitalization state",
            output.text,
            "cat. Herds cat");

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
        "tests/test-toggle-dictionary.json",
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

        const char *disabled_layer_undo = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &disabled_layer_undo);
        ok = ok && expect_string("disabled dictionary does not override", disabled_layer_undo, "=undo");

        const char *dictionary_toggle = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "STPH", &dictionary_toggle);
        ok = ok && expect_string("custom dictionary toggle command", dictionary_toggle, "{plover:toggle_dict:!test-toggle-dictionary.json}");

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && steno_handle_stroke_bits(layered_steno, dictionary_toggle_bits);
        ok = ok && steno_handle_stroke_bits(layered_steno, undo_bits);
        ok = ok && expect_string("enabled dictionary command emits no text", output.text, "");
        ok = ok && expect_size("enabled dictionary key combo count", output.key_combo_count, 1);
        ok = ok && expect_string("enabled dictionary key combo", output.last_key_combo, "Left");

        const char *enabled_layer_left = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &enabled_layer_left);
        ok = ok && expect_string("enabled dictionary overrides", enabled_layer_left, "{#Left}{^}");

        ok = ok && steno_handle_stroke_bits(layered_steno, dictionary_toggle_bits);
        const char *disabled_layer_again = NULL;
        ok = ok && steno_lookup_stroke(layered_steno, "-R", &disabled_layer_again);
        ok = ok && expect_string("dictionary toggles off", disabled_layer_again, "=undo");

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

    test_output_destroy(&output);
    steno_destroy(steno);

    if (!ok) {
        return 1;
    }

    puts("test_steno: ok");
    return 0;
}
