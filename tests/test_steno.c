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
    uint64_t khrep_bits = 0;
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
    ok = ok && stroke_string_to_bits("KHREP", &khrep_bits);

    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && expect_string("phrase is inactive without namespace", output.text, "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && handle_phrase_test_stroke(steno, "PW-B", STENO_PHRASE_MODE_VERBS);
    ok = ok && expect_string("phrase wins over dictionary conflict when active", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    reset_output_log(&output);
    ok = ok && handle_phrase_test_stroke(steno, "PW-B", STENO_PHRASE_MODE_VERBS);
    ok = ok && expect_string("initial verb is a", output.text, "is a");
    ok = ok && handle_phrase_test_stroke(steno, "PW-T", STENO_PHRASE_MODE_VERBS);
    ok = ok && expect_string("initial verb spacing", output.text, "is a is the");
    ok = ok && steno_handle_stroke_bits(steno, undo_bits);
    ok = ok && expect_string("initial verb undo", output.text, "is a");

    const struct {
        const char *stroke;
        const char *expected;
    } initial_verb_cases[] = {
        { "PW-B", "is a" },
        { "PW-BD", "was a" },
        { "PWE-B", "are a" },
        { "PWE-BD", "were a" },
        { "PWU-B", "to be a" },
        { "PWA-B", "can be a" },
        { "PWA-BD", "could be a" },
        { "PW-T", "is the" },
        { "H-BD", "had a" },
        { "HU-B", "to have a" },
        { "HA-P", "can have it" },
        { "HA-PD", "could have it" },
        { "ST-P", "says it" },
        { "ST-PD", "said it" },
        { "STE-P", "say it" },
        { "STU-P", "to say it" },
        { "ST-PG", "saying it" },
        { "STA-P", "can say it" },
        { "TH-P", "thinks it" },
        { "TH-PD", "thought it" },
        { "THE-P", "think it" },
        { "THU-P", "to think it" },
        { "TH-PG", "thinking it" },
        { "THA-P", "can think it" },
        { "THR-S", "tells us" },
        { "THR-SD", "told us" },
        { "THRE-S", "tell us" },
        { "THRU-S", "to tell us" },
        { "THR-GS", "telling us" },
        { "THRA-S", "can tell us" },
        { "TKH-P", "holds it" },
        { "TKH-PD", "held it" },
        { "TKHE-P", "hold it" },
        { "TKHU-P", "to hold it" },
        { "TKH-PG", "holding it" },
        { "TKHA-P", "can hold it" },
        { "TKHA-PD", "could hold it" },
        { "SHR-S", "sells us" },
        { "SHR-SD", "sold us" },
        { "SHRE-S", "sell us" },
        { "SHRU-S", "to sell us" },
        { "SHR-GS", "selling us" },
        { "SHRA-P", "can sell it" },
        { "SPHR-S", "spells us" },
        { "SPHR-SD", "spelled us" },
        { "SPHRE-S", "spell us" },
        { "SPHRU-S", "to spell us" },
        { "SPHR-GS", "spelling us" },
        { "SPHRA-P", "can spell it" },
        { "KP-P", "keeps it" },
        { "KP-PD", "kept it" },
        { "KPE-P", "keep it" },
        { "KPU-P", "to keep it" },
        { "KP-PG", "keeping it" },
        { "KPA-P", "can keep it" },
        { "KP-S", "keeps us" },
        { "KP-SD", "kept us" },
        { "KP-GS", "keeping us" },
        { "KHR-B", "calls a" },
        { "KHR-BD", "called a" },
        { "KHRE-P", "call it" },
        { "KHRU-P", "to call it" },
        { "KHRA-P", "can call it" },
        { "KHRA-PD", "could call it" },
        { "KHR-PG", "calling it" },
    };
    for (size_t i = 0; i < sizeof(initial_verb_cases) / sizeof(initial_verb_cases[0]); ++i) {
        ok = ok && expect_phrase_stroke_output(
            &steno,
            &config,
            &output,
            "initial verb set 1",
            initial_verb_cases[i].stroke,
            STENO_PHRASE_MODE_VERBS,
            initial_verb_cases[i].expected);
    }

    ok = ok && reset_test_steno(&steno, &config);
    steno_set_phrase_namespace_enabled(steno, true);
    clear_test_output(&output);
    ok = ok && handle_test_stroke(steno, "KHREP");
    ok = ok && expect_string("phrase namespace normal KHREP skips phrase lookup", output.text, "KHREP");
    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = khrep_bits,
        .phrase = true,
        .phrase_namespace = true,
    }));
    ok = ok && expect_string("phrase namespace phrase KHREP uses call it", output.text, "call it");

    const struct {
        const char *stroke;
        const char *expected;
    } final_verb_long_cases[] = {
        { "SKWHR-B", "she is" },
        { "SKWHR-BD", "she was" },
        { "SKWHR*E", "she is not" },
        { "SKWHR*ED", "she was not" },
        { "SWR-F", "I have" },
        { "SWR-FD", "I had" },
        { "KPWR-GD", "you went" },
        { "SKWHRAO-G", "she will go" },
        { "SKWHRAO*G", "she will not go" },
        { "SKWHREG", "she is going" },
        { "SKWHR-FG", "she has gone" },
        { "KPWR-PBT", "you know that" },
        { "SKWHR-PBG", "she thinks" },
        { "SKWHR-PBGD", "she thought" },
        { "SKWHR-PBGT", "she thinks that" },
        { "SKWHR-PBGTD", "she thought that" },
        { "SKWHR-BS", "she says" },
        { "SKWHR-BSD", "she said" },
        { "SKWHR-BTS", "she says that" },
        { "SKWHR-BTSD", "she said that" },
        { "SKWHR-RLT", "she tells" },
        { "SKWHR-RLTD", "she told" },
        { "SKWHR-FPL", "she holds" },
        { "SKWHR-FPLD", "she held" },
        { "SKWHR-LS", "she sells" },
        { "SKWHR-LSD", "she sold" },
        { "SKWHR-PLS", "she spells" },
        { "SKWHR-PLSD", "she spelled" },
        { "SKWHR-RPBTS", "she keeps" },
        { "SKWHR-RPBTSD", "she kept" },
        { "TWH-TS", "they have to" },
    };
    for (size_t i = 0; i < sizeof(final_verb_long_cases) / sizeof(final_verb_long_cases[0]); ++i) {
        ok = ok && expect_phrase_stroke_output(
            &steno,
            &config,
            &output,
            "final verb long forms",
            final_verb_long_cases[i].stroke,
            STENO_PHRASE_MODE_VERBS,
            final_verb_long_cases[i].expected);
    }

    const struct {
        const char *stroke;
        const char *expected;
    } final_verb_contraction_cases[] = {
        { "#SKWHR-B", "she's" },
        { "#SKWHR*E", "she isn't" },
        { "#SKWHR*ED", "she wasn't" },
        { "#SKWHRAO-G", "she'll go" },
        { "#SKWHRAO*G", "she won't go" },
        { "#SWR-F", "I've" },
        { "#KWHR-FG", "he's gone" },
        { "#TWHAO-G", "they'll go" },
    };
    for (size_t i = 0; i < sizeof(final_verb_contraction_cases) / sizeof(final_verb_contraction_cases[0]); ++i) {
        ok = ok && expect_phrase_stroke_output(
            &steno,
            &config,
            &output,
            "final verb contractions",
            final_verb_contraction_cases[i].stroke,
            STENO_PHRASE_MODE_VERBS,
            final_verb_contraction_cases[i].expected);
    }

    Steno_Config follow_on_config = config;
    follow_on_config.phrasing_path = "phrasing.json";
    const char *follow_on_form_strokes[] = { "", "-D", "E", "-G", "U", "A", "A-D" };
    const struct {
        const char *stem;
        const char *forms[7];
    } follow_on_iv_rows[] = {
        { "TK", { "does", "did", "do", "doing", "to do", "can do", "could do" } },
        { "TKPW", { "goes", "went", "go", "going", "to go", "can go", "could go" } },
        { "W", { "wants", "wanted", "want", "wanting", "to want", "can want", "could want" } },
        { "SK", { "asks", "asked", "ask", "asking", "to ask", "can ask", "could ask" } },
        { "SP", { "happens", "happened", "happen", "happening", "to happen", "can happen", "could happen" } },
        { "SW", { "feels", "felt", "feel", "feeling", "to feel", "can feel", "could feel" } },
        { "K", { "comes", "came", "come", "coming", "to come", "can come", "could come" } },
        { "TPH", { "knows", "knew", "know", "knowing", "to know", "can know", "could know" } },
        { "TKPWH", { "gets", "got", "get", "getting", "to get", "can get", "could get" } },
        { "PWHR", { "believes", "believed", "believe", "believing", "to believe", "can believe", "could believe" } },
        { "KW", { "becomes", "became", "become", "becoming", "to become", "can become", "could become" } },
        { "R", { "runs", "ran", "run", "running", "to run", "can run", "could run" } },
        { "KPL", { "makes", "made", "make", "making", "to make", "can make", "could make" } },
        { "PH", { "takes", "took", "take", "taking", "to take", "can take", "could take" } },
        { "TP", { "finds", "found", "find", "finding", "to find", "can find", "could find" } },
        { "STP", { "gives", "gave", "give", "giving", "to give", "can give", "could give" } },
        { "STW", { "uses", "used", "use", "using", "to use", "can use", "could use" } },
        { "WR", { "works", "worked", "work", "working", "to work", "can work", "could work" } },
        { "SKP", { "needs", "needed", "need", "needing", "to need", "can need", "could need" } },
        { "SKW", { "remembers", "remembered", "remember", "remembering", "to remember", "can remember", "could remember" } },
        { "SKH", { "understands", "understood", "understand", "understanding", "to understand", "can understand", "could understand" } },
        { "TR", { "tries", "tried", "try", "trying", "to try", "can try", "could try" } },
        { "TKP", { "expects", "expected", "expect", "expecting", "to expect", "can expect", "could expect" } },
    };
    uint64_t follow_on_tail_bits = 0;
    ok = ok && stroke_string_to_bits("-RT", &follow_on_tail_bits);
    for (size_t i = 0; ok && i < sizeof(follow_on_iv_rows) / sizeof(follow_on_iv_rows[0]); ++i) {
        for (size_t j = 0; ok && j < sizeof(follow_on_form_strokes) / sizeof(follow_on_form_strokes[0]); ++j) {
            uint64_t stem_bits = 0;
            uint64_t form_bits = 0;
            ok = ok && stroke_string_to_bits(follow_on_iv_rows[i].stem, &stem_bits);
            if (follow_on_form_strokes[j][0] != '\0') {
                ok = ok && stroke_string_to_bits(follow_on_form_strokes[j], &form_bits);
            }
            ok = ok && reset_test_steno(&steno, &follow_on_config);
            clear_test_output(&output);
            reset_output_log(&output);
            ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
                .bits = stem_bits | form_bits | follow_on_tail_bits,
                .phrase_mode = STENO_PHRASE_MODE_VERBS,
                .phrase_namespace = true,
            }));
            char expected[128] = {0};
            char name[128] = {0};
            snprintf(expected, sizeof(expected), "%s that", follow_on_iv_rows[i].forms[j]);
            snprintf(name, sizeof(name), "verb follow-on IV %s %s", follow_on_iv_rows[i].stem, follow_on_form_strokes[j]);
            ok = ok && expect_string(name, output.text, expected);
        }
    }

    const struct {
        const char *id;
        const char *ender;
        const char *past_ender;
        const char *forms[5];
    } follow_on_fv_rows[] = {
        { "happen", "-PZ", "-PDZ", { "happen", "happens", "happened", "happening", "happened" } },
        { "feel", "-LT", "-LTD", { "feel", "feels", "felt", "feeling", "felt" } },
        { "come", "-BG", "-BGD", { "come", "comes", "came", "coming", "come" } },
        { "believe", "-BL", "-BLD", { "believe", "believes", "believed", "believing", "believed" } },
        { "become", "-RPBG", "-RPBGD", { "become", "becomes", "became", "becoming", "become" } },
        { "run", "-R", "-RD", { "run", "runs", "ran", "running", "run" } },
        { "make", "-RPBL", "-RPBLD", { "make", "makes", "made", "making", "made" } },
        { "take", "-RBT", "-RBTD", { "take", "takes", "took", "taking", "taken" } },
        { "give", "-GZ", "-GDZ", { "give", "gives", "gave", "giving", "given" } },
        { "use", "-Z", "-DZ", { "use", "uses", "used", "using", "used" } },
        { "work", "-RBG", "-RBGD", { "work", "works", "worked", "working", "worked" } },
        { "remember", "-RPL", "-RPLD", { "remember", "remembers", "remembered", "remembering", "remembered" } },
        { "understand", "-RPB", "-RPBD", { "understand", "understands", "understood", "understanding", "understood" } },
        { "expect", "-PGS", "-PGSD", { "expect", "expects", "expected", "expecting", "expected" } },
        { "ask", "-RBS", "-RBSD", { "ask", "asks", "asked", "asking", "asked" } },
    };
    const struct {
        const char *starter;
        const char *structure;
        bool past;
        const char *prefix;
    } follow_on_fv_forms[] = {
        { "SWR", "", false, "I " },
        { "SKWHR", "", false, "she " },
        { "SKWHR", "", true, "she " },
        { "SKWHR", "E", false, "she is " },
        { "SKWHR", "-F", false, "she has " },
    };
    for (size_t i = 0; ok && i < sizeof(follow_on_fv_rows) / sizeof(follow_on_fv_rows[0]); ++i) {
        for (size_t j = 0; ok && j < sizeof(follow_on_fv_forms) / sizeof(follow_on_fv_forms[0]); ++j) {
            uint64_t starter_bits = 0;
            uint64_t structure_bits = 0;
            uint64_t ender_bits = 0;
            ok = ok && stroke_string_to_bits(follow_on_fv_forms[j].starter, &starter_bits);
            if (follow_on_fv_forms[j].structure[0] != '\0') {
                ok = ok && stroke_string_to_bits(follow_on_fv_forms[j].structure, &structure_bits);
            }
            ok = ok && stroke_string_to_bits(
                follow_on_fv_forms[j].past ? follow_on_fv_rows[i].past_ender : follow_on_fv_rows[i].ender,
                &ender_bits
            );
            ok = ok && reset_test_steno(&steno, &follow_on_config);
            clear_test_output(&output);
            reset_output_log(&output);
            ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
                .bits = starter_bits | structure_bits | ender_bits,
                .phrase_mode = STENO_PHRASE_MODE_VERBS,
                .phrase_namespace = true,
            }));
            char expected[128] = {0};
            char name[128] = {0};
            snprintf(expected, sizeof(expected), "%s%s", follow_on_fv_forms[j].prefix, follow_on_fv_rows[i].forms[j]);
            snprintf(name, sizeof(name), "verb follow-on FV %s form %zu", follow_on_fv_rows[i].id, j);
            ok = ok && expect_string(name, output.text, expected);
        }
    }

    const struct {
        const char *stroke;
        const char *expected;
    } verb_follow_on_cases[] = {
        { "STKH-B", "this is" },
        { "STKH-BD", "this was" },
        { "STKH-FG", "this has gone" },
        { "STKHAO*", "this will not" },
        { "#STKHAO", "this'll" },
        { "STKH-PGS", "this expects" },
        { "STWH-B", "that is" },
        { "STWH-BD", "that was" },
        { "#STWH-B", "that's" },
        { "#STWH-FG", "that's gone" },
        { "STWHAO*", "that will not" },
        { "#STWHAO", "that'll" },
        { "STPHR-B", "there are" },
        { "STPHR-BD", "there were" },
        { "STPHRE", "there are" },
        { "STPHRED", "there were" },
        { "#STPHR-B", "there're" },
        { "#STPHR*ED", "there weren't" },
        { "STPHRAO*", "there will not" },
        { "#STPHRAO", "there'll" },
        { "STPHRE-F", "there have been" },
        { "#STPHRE-F", "there've been" },
        { "STPHR-BG", "there come" },
        { "STPHR-BGD", "there came" },
        { "STPHR-PZ", "there happen" },
        { "STPHR-PDZ", "there happened" },
        { "STPHR-RBS", "STPHR-RBS" },
        { "THRE", "there is" },
        { "THRED", "there was" },
        { "THR*E", "there is not" },
        { "THR*ED", "there was not" },
        { "#THRE", "there's" },
        { "#THR*ED", "there wasn't" },
        { "THRAO*", "there will not" },
        { "#THRAO", "there'll" },
        { "#THRAO*", "there won't" },
        { "THRAOE", "there will be" },
        { "THRE-F", "there has been" },
        { "#THRE-F", "there's been" },
        { "THR-BT", "there is a" },
        { "THR-BTD", "there was a" },
        { "THR-TS", "there has to" },
        { "THR-TSD", "there had to" },
        { "THR-G", "there goes" },
        { "THR-GD", "there went" },
        { "THRAO-GT", "there will go to" },
        { "THR-GTD", "there went to" },
        { "THR-L", "there looks" },
        { "THR-LD", "there looked" },
        { "THR-PZ", "there happens" },
        { "THR-PDZ", "there happened" },
        { "THR-LTS", "there feels like" },
        { "THR-LTSD", "there felt like" },
        { "THRAO-BG", "there will come" },
        { "THR-BGD", "there came" },
        { "THR-BGT", "there comes to" },
        { "THR-BGTD", "there came to" },
        { "THR-RPBG", "there becomes" },
        { "THR-RPBGD", "there became" },
        { "THR-RPBGT", "there becomes a" },
        { "THR-RPBGTD", "there became a" },
        { "THRAO-R", "there will run" },
        { "THRE-RD", "there was running" },
        { "THR-RBT", "there takes" },
        { "THR-RBTD", "there took" },
        { "THR-RPGT", "there needs to" },
        { "THR-RPGTD", "there needed to" },
        { "THR-GTS", "there gets to" },
        { "THR-GTSD", "there got to" },
        { "THR-RBG", "there works" },
        { "THR-RBGD", "there worked" },
        { "THR-RBGT", "there works on" },
        { "THR-RBGTD", "there worked on" },
        { "THR-B", "tells a" },
        { "THR-BD", "told a" },
        { "THR-GT", "telling the" },
        { "THR-BG", "telling a" },
        { "THR-R", "tells her" },
        { "THR-RBS", "THR-RBS" },
        { "TK-P", "does it" },
        { "TK-PD", "did it" },
        { "TKE-P", "do it" },
        { "TK-PG", "doing it" },
        { "TKU-P", "to do it" },
        { "TKA-P", "can do it" },
        { "TKA-PD", "could do it" },
        { "TKPW-LTD", "went at" },
        { "W-PG", "wanting it" },
        { "SKU-PLT", "to ask me" },
        { "SP-LTD", "happened at" },
        { "SW-RT", "feels that" },
        { "KE-LT", "come at" },
        { "TPHA-RT", "can know that" },
        { "TKPWH-PG", "getting it" },
        { "PWHR-RTD", "believed that" },
        { "KW-B", "becomes a" },
        { "PH-PG", "taking it" },
        { "TPA-RT", "can find that" },
        { "STP-PD", "gave it" },
        { "STWE-P", "use it" },
        { "WR-LGT", "working at" },
        { "SKPU-P", "to need it" },
        { "SKW-RTD", "remembered that" },
        { "SKHE-RT", "understand that" },
        { "TRA-PD", "could try it" },
        { "TKP-RT", "expects that" },
        { "KH-P", "catches it" },
        { "R-P", "runs it" },
        { "KPL-RTD", "made that" },
        { "KPL-P", "keeps my" },
        { "SP-PLT", "SP-PLT" },
        { "K-S", "K-S" },
        { "KW-LT", "KWLT" },
        { "STW-FB", "STWFB" },
        { "SKWHR-RB", "she catches" },
        { "SKWHR-RBS", "she asks" },
        { "SKWHR-RBSD", "she asked" },
        { "SKWHR-PZ", "she happens" },
        { "SKWHR-PDZ", "she happened" },
        { "SKWHR-LT", "she feels" },
        { "SKWHR-LTD", "she felt" },
        { "SKWHR-LTS", "she feels like" },
        { "SKWHR-LTSD", "she felt like" },
        { "SKWHR-BG", "she comes" },
        { "SKWHR-BGD", "she came" },
        { "SKWHR-BGT", "she comes to" },
        { "SKWHR-BGTD", "she came to" },
        { "SKWHR-BL", "she believes" },
        { "SKWHR-BLD", "she believed" },
        { "SKWHR-BLT", "she believes that" },
        { "SKWHR-BLTD", "she believed that" },
        { "SKWHR-RPBG", "she becomes" },
        { "SKWHR-RPBGD", "she became" },
        { "SKWHR-RPBGT", "she becomes a" },
        { "SKWHR-RPBGTD", "she became a" },
        { "SKWHR-R", "she runs" },
        { "SKWHR-RD", "she ran" },
        { "SKWHR-RPBL", "she makes" },
        { "SKWHR-RPBLD", "she made" },
        { "SKWHR-RPBLT", "she makes a" },
        { "SKWHR-RPBLTD", "she made a" },
        { "SKWHR-RBT", "she takes" },
        { "SKWHR-RBTD", "she took" },
        { "SKWHR-GZ", "she gives" },
        { "SKWHR-GDZ", "she gave" },
        { "SKWHR-Z", "she uses" },
        { "SKWHR-DZ", "she used" },
        { "SKWHR-RBG", "she works" },
        { "SKWHR-RBGD", "she worked" },
        { "SKWHR-RBGT", "she works on" },
        { "SKWHR-RBGTD", "she worked on" },
        { "SKWHR-RPL", "she remembers" },
        { "SKWHR-RPLD", "she remembered" },
        { "SKWHR-RPLT", "she remembers that" },
        { "SKWHR-RPLTD", "she remembered that" },
        { "SKWHR-RPB", "she understands" },
        { "SKWHR-RPBD", "she understood" },
        { "SKWHR-RPBT", "she understands the" },
        { "SKWHR-RPBTD", "she understood the" },
        { "SKWHR-PGS", "she expects" },
        { "SKWHR-PGSD", "she expected" },
        { "SKWHR-PGTS", "she expects that" },
        { "SKWHR-PGTSD", "she expected that" },
        { "SKWHR-FBG", "she has come" },
        { "SKWHR-FR", "she has run" },
        { "SKWHR-FRPBL", "she has made" },
        { "SKWHR-FRBT", "she has taken" },
        { "SKWHR-FGZ", "she has given" },
        { "SKWHR-FRPB", "she has understood" },
        { "SKWHRE-BG", "she is coming" },
        { "SKWHRE-R", "she is running" },
        { "SKWHRE-Z", "she is using" },
    };
    for (size_t i = 0; i < sizeof(verb_follow_on_cases) / sizeof(verb_follow_on_cases[0]); ++i) {
        ok = ok && expect_phrase_stroke_output(
            &steno,
            &follow_on_config,
            &output,
            "verb follow-on coverage",
            verb_follow_on_cases[i].stroke,
            STENO_PHRASE_MODE_VERBS,
            verb_follow_on_cases[i].expected);
    }

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_phrase_test_stroke(steno, "PW-PB", STENO_PHRASE_MODE_VERBS);
    ok = ok && expect_string("unassigned IV stroke falls back to raw outline", output.text, "PW-PB");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_phrase_test_stroke(steno, "#SKWHR-BD", STENO_PHRASE_MODE_VERBS);
    ok = ok && expect_string("unnatural contraction form stays unassigned", output.text, "#SKWHRBD");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && handle_phrase_test_stroke(steno, "SAO", STENO_PHRASE_MODE_ALL);
    ok = ok && expect_string("phrase miss falls back to raw outline", output.text, "SAO");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_fallback_test_bits,
        .phrase_mode = STENO_PHRASE_MODE_ALL,
        .phrase_namespace = true,
    }));
    ok = ok && expect_string("phrase miss skips dictionary lookup", output.text, "#KW");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_is_a_bits,
        .phrase_mode = STENO_PHRASE_MODE_VERBS,
        .phrase_namespace = true,
    }));
    ok = ok && steno_handle_stroke_bits(steno, phrase_fallback_test_bits);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && expect_string(
        "phrases and dictionary words interleave normally",
        output.text,
        "is a test dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    steno_set_phrase_namespace_enabled(steno, true);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke_bits(steno, phrase_is_a_bits);
    ok = ok && expect_string("phrase namespace normal stroke skips phrase lookup", output.text, "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_is_a_bits,
        .phrase = true,
        .phrase_namespace = true,
    }));
    ok = ok && expect_string("phrase namespace phrase stroke uses phrase lookup", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_is_a_bits,
        .phrase_mode = STENO_PHRASE_MODE_VERBS,
        .phrase_namespace = true,
    }));
    ok = ok && expect_string("phrase namespace verb pedal uses verb lookup", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = phrase_fallback_test_bits,
        .phrase = true,
        .phrase_namespace = true,
    }));
    ok = ok && expect_string("phrase namespace phrase miss skips dictionary lookup", output.text, "#KW");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && steno_handle_stroke_bits(steno, cat_bits);
    ok = ok && steno_handle_stroke(steno, ((Stroke_Input) {
        .bits = toggle_star_bits,
        .phrase = true,
        .phrase_namespace = true,
    }));
    ok = ok && expect_string("phrase namespace star falls back to dictionary", output.text, "kitty");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "e", true);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "k", false);
    ok = ok && expect_string("qwerty gathered chord ignores inactive phrase lookup", output.text, "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    steno_set_phrase_namespace_enabled(steno, true);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "e", true);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "k", false);
    ok = ok && expect_string("qwerty phrase namespace normal chord skips phrase lookup", output.text, "dictionary is a");

    ok = ok && reset_test_steno(&steno, &config);
    steno_set_phrase_namespace_enabled(steno, true);
    clear_test_output(&output);
    ok = ok && send_key_event(steno, "e", true);
    steno_set_phrase_mode(steno, true);
    steno_set_phrase_mode(steno, false);
    ok = ok && send_key_event(steno, "d", true);
    ok = ok && send_key_event(steno, "k", true);
    ok = ok && send_key_event(steno, "e", false);
    ok = ok && send_key_event(steno, "d", false);
    ok = ok && send_key_event(steno, "k", false);
    ok = ok && expect_string("qwerty phrase namespace latches phrase during chord", output.text, "is a");

    ok = ok && reset_test_steno(&steno, &config);
    clear_test_output(&output);
    ok = ok && !send_key_event(steno, "left_shift", true);
    ok = ok && !send_key_event(steno, "left_shift", false);
    ok = ok && send_key_event(steno, "u", true);
    ok = ok && send_key_event(steno, "u", false);
    ok = ok && expect_string("qwerty shift tap has no phrasing side effect", output.text, "fee");

    Steno_Config modal_config = config;
    modal_config.modal_dictionary_path = "tests/test-phrase-preference-dictionary.json";
    Steno *modal_steno = steno_create(&modal_config);
    ok = ok && modal_steno != NULL;
    if (modal_steno != NULL) {
        const char *modal_fallback_lookup = NULL;
        ok = ok && steno_lookup_stroke(
            modal_steno,
            "STPHULGDZ",
            &modal_fallback_lookup);
        ok = ok && expect_string(
            "single-stroke lookup delegates to modal dictionary",
            modal_fallback_lookup,
            "snuggling");
        const char *modal_multi_lookup = NULL;
        ok = ok && !steno_lookup_stroke(modal_steno, "H/R", &modal_multi_lookup);

        clear_test_output(&output);
        reset_output_log(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TKPWHREUF");
        ok = ok && expect_string("modal phrase preference first phrase", output.text, "but if");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "EUBG");
        ok = ok && expect_string(
            "modal phrase preference beats conflicting longer outline",
            output.text,
            "but if I can");
        ok = ok && steno_handle_stroke_bits(modal_steno, undo_bits);
        ok = ok && expect_string("modal phrase preference undo restores prior plan", output.text, "but if");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "S");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "T");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "K");
        ok = ok && expect_string(
            "modal phrase preference enumerates three-stroke partitions",
            output.text,
            "one two three words");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "P");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "W");
        ok = ok && expect_string(
            "modal phrase preference tie uses fewer segments",
            output.text,
            "two words");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "H");
        ok = ok && expect_string("modal provisional raw stroke", output.text, "H");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "R");
        ok = ok && expect_string("modal raw stroke replaced by later outline", output.text, "hello");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "A");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "O");
        ok = ok && expect_string(
            "modal formatter entry bypasses phrase scoring",
            output.text,
            "glyphic");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "F");
        ok = ok && expect_string(
            "modal formatter fallback remains open for a longer outline",
            output.text,
            "longer");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TP");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "WH");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "R");
        ok = ok && expect_string(
            "modal formatted suffix can start at a preferred-plan boundary",
            output.text,
            "alpha replacement");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_test_stroke(modal_steno, "TP-PL");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "L");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "G");
        ok = ok && expect_string(
            "modal formatted full outline replays run base case state",
            output.text,
            ". World");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "KAT");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "HR");
        ok = ok && handle_test_stroke(modal_steno, "TO");
        ok = ok && expect_string(
            "modal retro replacement preserves dictionary provenance",
            output.text,
            "MODAL cat to");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "KAT");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "-RBGS");
        ok = ok && handle_test_stroke(modal_steno, "TO");
        ok = ok && expect_string(
            "modal stitch replacement preserves dictionary provenance",
            output.text,
            "modal c-a-t to");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "KAT");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "#*");
        ok = ok && expect_string(
            "modal retro asterisk retranslates in the modal dictionary",
            output.text,
            "star cat");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TO");
        ok = ok && expect_string(
            "modal retro asterisk keeps the corrected run open",
            output.text,
            "corrected phrase has five words");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "SR");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TR");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "#*");
        ok = ok && expect_string(
            "modal retro asterisk preserves a corrected run prefix",
            output.text,
            "alpha corrected beta");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "KR");
        ok = ok && expect_string(
            "modal corrected multi-stroke run remains extensible",
            output.text,
            "corrected full phrase has six words");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "KAT");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "WR");
        ok = ok && handle_test_stroke(modal_steno, "TO");
        ok = ok && expect_string(
            "modal repeat command preserves dictionary provenance",
            output.text,
            "modal cat modal cat to");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        uint64_t replay_failure_bits = 0;
        ok = ok && stroke_string_to_bits("P", &replay_failure_bits);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "SK");
        output.fail_next_send = true;
        ok = ok && !steno_handle_stroke(modal_steno, ((Stroke_Input) {
            .bits = replay_failure_bits,
            .modal_dictionary = true,
        }));
        reset_output_log(&output);
        ok = ok && handle_test_stroke(modal_steno, "F");
        ok = ok && expect_string(
            "failed modal replay restores caller case state",
            output.last_send,
            "Fee");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_test_stroke(modal_steno, "STPHULGDZ");
        ok = ok && expect_string(
            "undefined main stroke delegates to modal dictionary",
            output.text,
            "snuggling");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_test_stroke(modal_steno, "KAT");
        ok = ok && expect_string("main dictionary wins over modal fallback", output.text, "cat");
        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "F");
        ok = ok && expect_string("modal miss never falls through to main dictionary", output.text, "F");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_test_stroke(modal_steno, "KAT");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TO");
        ok = ok && expect_string(
            "modal lookup cannot consume preceding main stroke",
            output.text,
            "cat modal to");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "KAT");
        ok = ok && handle_test_stroke(modal_steno, "TO");
        ok = ok && expect_string(
            "main lookup cannot consume preceding modal stroke",
            output.text,
            "modal cat to");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TKPWHREUF");
        ok = ok && handle_test_stroke(modal_steno, "F");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "EUBG");
        ok = ok && expect_string(
            "normal stroke closes modal phrase run",
            output.text,
            "but if fee I can");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && steno_handle_stroke(modal_steno, ((Stroke_Input) {
            .bits = phrase_is_a_bits,
            .phrase = true,
            .phrase_namespace = true,
            .modal_dictionary = true,
        }));
        ok = ok && expect_string(
            "modal dictionary takes precedence over phrase namespace",
            output.text,
            "modal is a");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && send_key_event(modal_steno, "e", true);
        steno_set_modal_dictionary_mode(modal_steno, true);
        steno_set_modal_dictionary_mode(modal_steno, false);
        ok = ok && send_key_event(modal_steno, "d", true);
        ok = ok && send_key_event(modal_steno, "k", true);
        ok = ok && send_key_event(modal_steno, "e", false);
        ok = ok && send_key_event(modal_steno, "d", false);
        ok = ok && send_key_event(modal_steno, "k", false);
        ok = ok && expect_string(
            "qwerty chord latches modal dictionary during stroke",
            output.text,
            "modal is a");

        ok = ok && reset_test_steno(&modal_steno, &modal_config);
        clear_test_output(&output);
        ok = ok && handle_test_stroke(modal_steno, "TP-PL");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "TKPWHREUF");
        ok = ok && handle_modal_dictionary_test_stroke(modal_steno, "EUBG");
        ok = ok && expect_string(
            "modal phrase recomputation preserves run base case state",
            output.text,
            ". But if I can");

        steno_destroy(modal_steno);
    }

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

    test_output_destroy(&output);
    steno_destroy(steno);

    if (!ok) {
        return 1;
    }

    puts("test_steno: ok");
    return 0;
}
