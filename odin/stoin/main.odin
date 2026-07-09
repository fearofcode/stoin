package stoin

import "core:fmt"
import "core:os"

print_help :: proc() {
	fmt.println("stoin Odin port checkpoint")
	fmt.println("")
	fmt.println("Usage:")
	fmt.println("  stoin [--help]")
	fmt.println("  stoin [--config PATH] [--dictionary PATH...] --lookup OUTLINE [--lookup OUTLINE...]")
	fmt.println("  stoin [--config PATH] [--dictionary PATH...] --dump-dictionary [OUTPUT_PATH]")
	fmt.println("  stoin [--config PATH] [--dictionary PATH...] [--word-list PATH] [--phrasing PATH --phrase-mode all|verbs|nonverbs] [--print-suggestions] [--suggestion-log PATH] --translate OUTLINE [OUTLINE...]")
	fmt.println("  stoin --input qwerty [--config PATH] [--dictionary PATH...] [--keymap PATH] [--word-list PATH] [--phrasing PATH [--phrase-mode all|verbs|nonverbs] [--phrase-toggle KEY] [--nonverb-phrase-toggle KEY]] [--print-suggestions] [--suggestion-log PATH] [--trace-key-events] [--time-translations] [--trace-strokes|--no-trace-strokes]")
	fmt.println("  stoin --input tx-bolt|gemini-pr|stentura [--config PATH] [--dictionary PATH...] [--serial-port PATH] [--serial-baud BAUD] [--multiple-inputs] [--multi-input-window-ms MS] [--word-list PATH] [--phrasing PATH [--phrase-mode all|verbs|nonverbs] [--phrase-toggle KEY] [--nonverb-phrase-toggle KEY]] [--print-suggestions] [--suggestion-log PATH] [--trace-key-events] [--time-translations] [--trace-strokes|--no-trace-strokes]")
	fmt.println("  stoin --raw-serial [--serial-port PATH] [--serial-baud BAUD]")
	fmt.println("")
	fmt.println("The Odin port is not the default release yet, but the listed lookup, translate, macOS qwerty, macOS serial, and raw serial paths are implemented checkpoints.")
}

main :: proc() {
	config, ok := parse_cli_args(os.args)
	defer cli_config_destroy(&config)

	if !ok {
		fmt.eprintln("stoin:", config.error_message)
		print_help()
		os.exit(2)
	}

	switch config.mode {
	case .Help:
		print_help()
		return
	case .Lookup:
		if !run_lookup_cli(&config) {
			os.exit(1)
		}
		return
	case .Dump_Dictionary:
		if !run_dump_dictionary_cli(&config) {
			os.exit(1)
		}
		return
	case .Translate:
		if !run_translate_cli(&config) {
			os.exit(1)
		}
		return
	case .Qwerty:
		if !run_qwerty_cli(&config) {
			os.exit(1)
		}
		return
	case .Tx_Bolt:
		if !run_tx_bolt_cli(&config) {
			os.exit(1)
		}
		return
	case .Gemini_Pr:
		if !run_gemini_pr_cli(&config) {
			os.exit(1)
		}
		return
	case .Stentura:
		if !run_stentura_cli(&config) {
			os.exit(1)
		}
		return
	case .Raw_Serial:
		if !run_raw_serial_cli(&config) {
			os.exit(1)
		}
		return
	case .Scaffold:
	}

	fmt.println("stoin Odin port checkpoint")
	fmt.println("Use --help for implemented Odin-port checkpoints.")
}
