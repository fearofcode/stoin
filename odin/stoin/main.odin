package stoin

import "core:fmt"
import "core:os"
import "core:testing"

APP_NAME :: "stoin"

should_show_help :: proc(args: []string) -> bool {
	for arg in args[1:] {
		if arg == "--help" || arg == "-h" {
			return true
		}
	}
	return false
}

print_help :: proc() {
	fmt.println("stoin Odin port scaffold")
	fmt.println("")
	fmt.println("Usage:")
	fmt.println("  stoin [--help]")
	fmt.println("")
	fmt.println("This binary is Phase 0 of the Odin port. Use the C binary for stenography until parity is complete.")
}

main :: proc() {
	if should_show_help(os.args) {
		print_help()
		return
	}

	fmt.println("stoin Odin port scaffold")
	fmt.println("Use --help for details. Use the C binary for stenography until the Odin port reaches parity.")
}

@(test)
test_should_show_help :: proc(t: ^testing.T) {
	help_args := [?]string{APP_NAME, "--help"}
	short_help_args := [?]string{APP_NAME, "-h"}
	plain_args := [?]string{APP_NAME}

	testing.expect(t, should_show_help(help_args[:]))
	testing.expect(t, should_show_help(short_help_args[:]))
	testing.expect(t, !should_show_help(plain_args[:]))
}
