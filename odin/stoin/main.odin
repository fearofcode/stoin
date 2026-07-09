package stoin

import "core:fmt"
import "core:os"

print_help :: proc() {
	fmt.println("stoin Odin port scaffold")
	fmt.println("")
	fmt.println("Usage:")
	fmt.println("  stoin [--help]")
	fmt.println("  stoin --dict PATH --lookup OUTLINE [--lookup OUTLINE...]")
	fmt.println("  stoin --dict PATH [--dict PATH...] [--orthography PATH] [--phrasing PATH --phrase-mode all|verbs|nonverbs] [--print-suggestions] [--suggestion-log PATH] --translate OUTLINE [OUTLINE...]")
	fmt.println("  stoin --input qwerty --dict PATH [--dict PATH...] [--keymap PATH] [--orthography PATH] [--phrasing PATH [--phrase-mode all|verbs|nonverbs] [--phrase-toggle KEY] [--nonverb-phrase-toggle KEY]] [--print-suggestions] [--suggestion-log PATH]")
	fmt.println("  stoin --input tx-bolt --dict PATH [--dict PATH...] [--serial-port PATH] [--serial-baud BAUD] [--orthography PATH] [--phrasing PATH [--phrase-mode all|verbs|nonverbs] [--phrase-toggle KEY] [--nonverb-phrase-toggle KEY]] [--print-suggestions] [--suggestion-log PATH]")
	fmt.println("  stoin --raw-serial [--serial-port PATH] [--serial-baud BAUD]")
	fmt.println("")
	fmt.println("This binary is Phase 0 of the Odin port. Use the C binary for stenography until parity is complete.")
	fmt.println("The lookup, translate, macOS qwerty, macOS TX Bolt, and raw serial paths are temporary manual checkpoints for the Odin port.")
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
	case .Raw_Serial:
		if !run_raw_serial_cli(&config) {
			os.exit(1)
		}
		return
	case .Scaffold:
	}

	fmt.println("stoin Odin port scaffold")
	fmt.println("Use --help for details. Use the C binary for stenography until the Odin port reaches parity.")
}
