package stoin

import "core:fmt"
import "core:os"

print_help :: proc() {
	fmt.println("stoin Odin port scaffold")
	fmt.println("")
	fmt.println("Usage:")
	fmt.println("  stoin [--help]")
	fmt.println("  stoin --dict PATH --lookup OUTLINE [--lookup OUTLINE...]")
	fmt.println("")
	fmt.println("This binary is Phase 0 of the Odin port. Use the C binary for stenography until parity is complete.")
	fmt.println("The --dict/--lookup path is a temporary manual checkpoint for exact dictionary lookups.")
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
	case .Scaffold:
	}

	fmt.println("stoin Odin port scaffold")
	fmt.println("Use --help for details. Use the C binary for stenography until the Odin port reaches parity.")
}
