# Odin Port Plan

This is the migration plan for a full 1:1 Odin port of the stoin core app. The
C implementation remains the source of truth until the Odin implementation has
feature parity, matching tests, and working platform builds.

Current local Odin toolchain:

- Path: `/Users/wkh/Downloads/odin-macos-arm64-nightly+2026-07-06/odin`
- Version: `dev-2026-07-nightly:ab0131c`
- Fish `fish_user_paths` has been updated so this nightly is first.
- The downloaded binary needed `com.apple.quarantine` removed before macOS would
  execute it.

## Migration Rules

- No functionality loss. Every C feature either lands in Odin or remains served
  by the C binary until the Odin path is complete.
- Keep the C implementation and tests runnable throughout the port.
- Port logic in layers, starting with pure deterministic modules before
  touching platform input/output.
- Use Odin's built-in maps and dynamic arrays instead of `stb_ds`.
- Prefer slices and fixed arrays for stroke/key data; keep `u64` stroke bitsets
  as the canonical identity model.
- Use `context.temp_allocator` or an explicit arena for per-stroke scratch
  strings and candidate buffers, then free/reset after the stroke.
- Use normal long-lived allocators for dictionary stacks, phrasing tables,
  keymaps, translation history, and platform state.
- Use Odin `@(test)` tests and `odin test`; keep golden behavior parallel to
  the current C tests.
- Use Odin foreign bindings only where the core/vendor packages do not already
  provide the needed OS API.

## Proposed Package Layout

- `odin/stoin` or `src_odin/stoin`: main executable package.
- `odin/core`: pure steno engine packages:
  - `stroke`: steno bit positions, outline parsing/formatting, number-bar aliases.
  - `dictionary`: JSON loading, canonical outline maps, reverse text lookup.
  - `dictionary_stack`: layered dictionaries, enable/disable, hot reload stamps.
  - `format`: Plover-style formatting commands and text case/spacing behavior.
  - `orthography`: suffix rules and word-list integration.
  - `translation`: match finding, history, retroactive replacement, undo/repeat.
  - `phrasing`: phrasing JSON loader and IV/FV/NV lookup.
  - `suggestions`: brevity suggestions and JSONL logging.
- `odin/protocol`: TX Bolt, Gemini PR, Stentura, raw serial helpers.
- `odin/platform`: platform abstraction and per-OS implementations.
- `odin/tests`: shared test fixtures if splitting tests across packages gets
  awkward; otherwise keep tests next to the packages they exercise.

Exact directory names can change once the first Odin package exists, but keep
pure logic separate from platform code so Linux/Windows catch-up is possible.

## Phase 0: Tooling And Skeleton

Status: implemented as the initial Odin scaffold.

Current commands:

- `make odin`
- `make odin-test`
- `make odin-release`

- Add build targets without replacing C targets:
  - `make odin`
  - `make odin-test`
  - optional `make odin-release`
- Decide whether the repo pins `ODIN ?= odin` or the absolute local nightly path.
  Prefer `ODIN ?= odin` now that Fish resolves to the July nightly.
- Create an Odin hello-world executable that can parse `--help`.
- Create one minimal `@(test)` smoke test and run it with `odin test`.
- Add CI/local docs for the exact Odin commands.

Acceptance:

- C `make test` still passes.
- Odin smoke binary builds.
- Odin smoke tests run.

## Phase 1: Pure Stroke And Protocol Units

Status: implemented for pure stroke parsing/formatting, TX Bolt byte decoding,
Gemini PR packet decoding, Stentura packet helpers, and stroke merge behavior.

Port the deterministic lowest-level modules first:

- Steno key enum and bitset helpers.
- Outline parse/format, including right-hand hyphen disambiguation.
- Number-bar aliases.
- TX Bolt byte decoder.
- Gemini PR packet decoder.
- Stentura stroke decode, CRC, request construction, and response validation.
- Stroke merge timing logic.

Acceptance:

- Odin tests mirror the current `test_core_units` coverage.
- Canonical outline strings match C output exactly for all current test cases.
- Protocol byte fixtures pass unchanged.

## Phase 2: Dictionary And Formatting Core

Status: started. Dictionary JSON loading, canonical outline storage, layered
loading, exact lookup, native Odin dictionary tests, and a temporary manual
`--dict PATH --lookup OUTLINE` checkpoint are implemented. A temporary
`--dict PATH --translate OUTLINE...` checkpoint now handles exact lookups,
retroactive longest-match replacement, simple spacing, attach suffixes, glue,
punctuation, a focused port of the current orthographic suffix rules, and
same-stroke suffix-key fallback. It also handles `=undo`,
`=repeat_last_translation`, and `--print-suggestions` brevity hints with JSONL
logging. Basic stitch and stitch-last-word commands, key-combo command modeling,
case/carry formatting commands, layered dictionary loading, and modal dictionary
toggle commands are also implemented. Word-list-backed orthography is wired into
the simple engine and temporary CLI. Remaining formatting edge cases, platform
key-combo output, and other plover side-effect commands are still on the C side.

Port the pure translation data path:

- JSON dictionary loading using `core:encoding/json`.
- Canonical outline storage using `map[string]string`.
- Multi-stroke dictionary lookup and longest suffix matching.
- Reverse text-to-outline lookup for brevity suggestions.
- Plover formatting parser and formatter.
- Orthography suffix rules and word-list loading.
- Translation history and compaction.
- Undo/repeat, retro commands, stitch commands, key-combo command modeling.

Acceptance:

- Odin tests cover all dictionary, formatting, orthography, retro, stitch,
  history, and suggestion cases currently in `tests/test_dictionary_runtime.c`
  and `tests/test_steno.c`.
- Output text, delete text, inserted text, trace labels, and suggestion lines
  match the C behavior exactly.

## Phase 3: Phrasing Core

Status: started. The Odin phrasing loader parses and validates the current JSON
schema into owned IV, FV, and NV tables, including duplicate-stroke validation
and tail/verb reference checks. Initial-verb, nonverb, and final-verb lookup are
implemented, and the simple engine can apply phrase-mode strokes with raw-miss
fallback. The temporary `--translate` CLI can load phrasing data and run all,
verb, or nonverb phrase mode. Runtime pedal integration is started for macOS
qwerty and TX Bolt checkpoints; hot reload is still pending.

Port the phrasing system before platform pedals:

- `phrasing.json` loader.
- IV, FV, and NV namespaces.
- Per-verb forms, operators, tails, starters, structures, contractions.
- Duplicate-stroke validation.
- Phrase fallback behavior for star/toggle strokes.
- Hot-reload-safe replacement of phrasing tables.

Acceptance:

- All existing phrase tests pass in Odin.
- Phrase namespace behavior matches C when phrase mode is disabled, verb-only,
  nonverb-only, and all-phrase.
- Adding a new phrase data row should not require new code or tests unless the
  schema/logic changes.

## Phase 4: Engine Runtime Without Platform Input

Status: started. A callback-style Odin runtime now wraps the simple engine for
direct stroke handling, phrase namespace gating, minimal text delete/insert
output, key-combo callbacks, trace lines, and brevity suggestion output/log
lines. It also compacts old translation history while preserving retroactive
matches over the retained suffix. A path-owning runtime wrapper loads dictionary
stacks, orthography, and phrasing, and supports explicit dictionary/phrasing
reloads while keeping previous phrasing after failed reloads. It is covered by
fake-output tests. The temporary `--translate` CLI now uses the runtime callback
path. C/Odin fixture log parity is still pending.

Assemble the Odin `Steno` runtime around callback-style output:

- Engine config.
- Dictionary stack load/reload.
- Phrasing load/reload.
- Session enable/disable.
- Stroke handling from `Stroke_Input`.
- Trace output.
- Suggestion output/logging.
- Translation history cleanup.

Use tests with fake send/delete/key-combo callbacks, mirroring the current C
test harness.

Acceptance:

- Odin engine tests cover direct `handle_stroke_bits` behavior.
- The C and Odin engines produce identical fake-output logs on the same fixture
  stroke sequences.

## Phase 5: macOS QWERTY And Output

Status: started. The Odin port now has a keymap loader and synthetic qwerty
event/chord-gathering path wired into the runtime, including Ctrl+Esc capture
toggle, shortcut-modifier pass-through, phrase-mode latching during a chord,
phrase namespace gating, and phrase/nonverb pedal toggles. Darwin-only
CoreGraphics output bindings are started for text, delete, key-combo emission,
generated-event marking, and a live macOS qwerty event tap behind `--input
qwerty`, including normal outline/translation trace output. File watching is
still pending.

Implement the first real platform layer on macOS:

- QWERTY keymap loader.
- macOS event tap for qwerty chord gathering.
- macOS text/key-combo emission.
- Ctrl+Esc capture toggle.
- Shortcut-modified key pass-through.
- Phrase namespace/pedal state interaction for qwerty input.
- File watcher for dictionary and phrasing hot reload.

Likely bindings:

- Odin foreign imports for ApplicationServices/CoreFoundation APIs that are not
  already exposed in the Odin core/vendor packages.
- CoreFoundation run loop/event tap lifetime must be tested carefully, given the
  previous keyboard-tap shutdown crash in C.

Acceptance:

- macOS qwerty mode works with the same keymap and dictionary files.
- Current qwerty tests can be run either as unit tests or as platform integration
  tests.
- No phrase lookup happens unless phrase namespace and active phrase state say
  it should.

## Phase 6: Serial And Pedals On macOS

Status: started. A POSIX serial layer now handles serial device discovery,
8N1-style termios setup, nonblocking byte reads, writes, flush, close, and
basic baud validation on Darwin/Linux. The Odin CLI also has a macOS
`--input tx-bolt`, `--input gemini-pr`, and `--input stentura` checkpoints that
load the runtime, reconnect to serial devices, decode strokes, consume
phrase/nonverb pedal latches from a background listen-only keyboard tap, and
emit through the macOS output callbacks. Raw serial diagnostics are wired behind
`--raw-serial`. Hot reload and multi-input merge are still pending.

Port the current real-machine path:

- POSIX serial open/configuration/read loop.
- TX Bolt serial mode.
- Raw serial diagnostics.
- Phrase and nonverb pedal toggles via keyboard state.
- Pedal polling thread/atomic bit design, preserving the fixed behavior from C.
- Stroke merge for multiple input sources.

Use Odin `core:sys/posix/termios`, `core:sync`, and atomic intrinsics or
`core:c/libc/stdatomic` wrappers as appropriate.

Acceptance:

- Lever machine TX Bolt input works on macOS.
- Phrase pedal only latches phrase mode during chord gathering.
- Nonverb pedal is a separate key and validated distinct from verb phrase pedal.
- Ctrl+C/shutdown does not crash.

## Phase 7: Remaining Protocol And Platform Parity

Port features that are not needed for the first macOS qwerty/TX Bolt loop but
must exist before declaring parity:

- Gemini PR runtime input beyond the macOS serial checkpoint.
- Stentura archive/import parity beyond the macOS realtime serial checkpoint.
- Linux qwerty/input/output/file watcher.
- Linux serial path.
- Windows qwerty/input/output/file watcher.
- Windows serial path.
- Platform atomic wrappers if direct Odin intrinsics are not clean enough.
- Any remaining setup docs and build scripts.

Acceptance:

- Linux build and tests pass on Linux.
- Windows build and tests pass on Windows.
- All documented command-line options work or have a deliberate compatibility
  note during the transition.

## Phase 8: Cutover

Only after full parity:

- Make Odin binary the default build artifact.
- Keep C binary buildable for at least one transition period, or archive it on a
  dedicated legacy path.
- Remove `stb_ds` only when no C build needs it.
- Update setup docs, README/help output, and troubleshooting docs.
- Keep dictionary data format compatible with existing Plover/Lapwing/Stoin
  dictionaries.

Acceptance:

- `make test`, `make odin-test`, and all platform smoke tests pass.
- User-facing behavior is unchanged except for deliberately documented bug fixes.
- No feature from the current C app is missing.

## Parity Checklist

- Dictionary JSON compatibility.
- Layered dictionary stack and modal dictionary toggles.
- Word list and orthography behavior.
- Plover-style formatting commands.
- Retro commands: toggle star, delete space, insert space, case changes.
- Stitch commands.
- Key-combo commands.
- Translation history, undo, repeat, compaction.
- Raw untranslated stroke emission.
- Trace output including `[phrase]` and `[phase fallback]`.
- Brevity suggestions and JSONL suggestion log.
- Dictionary dump.
- Hot reload for dictionaries and phrasing.
- Phrasing IV/FV/NV namespaces.
- Verb and nonverb phrase pedals.
- QWERTY input.
- TX Bolt input and raw serial.
- Gemini PR input.
- Stentura support.
- macOS output and event tap.
- Linux output/input/file watcher.
- Windows output/input/file watcher.
- Runtime config file behavior.
- CLI flags and help text.
- SRS hint lookup compatibility with the running dictionary configuration.

## First Implementation Slice

Start with `stroke` plus Odin test plumbing:

1. Add `odin/stoin_core/stroke.odin` with steno bit positions and
   outline parse/format.
2. Add `odin/stoin_core/stroke_test.odin` with the current stroke formatting
   and number-bar cases.
3. Add `make odin-test` using `ODIN ?= odin`.
4. Keep all C tests running unchanged.

This creates a low-risk Odin foothold without touching platform behavior.
