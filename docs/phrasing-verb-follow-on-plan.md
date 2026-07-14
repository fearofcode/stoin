# Phrasing Verb Follow-On Status

The non-deferred assignments from the original follow-on plan are implemented
in `phrasing.json`. The complete active IV and FV assignments are documented in
`phrasing-keyboard-only-design.md`.

Each newly implemented IV verb has every ordinary grammatical form supported
by the layout: third-person present, simple past, base, present participle, `to`
infinitive, `can`, and `could`. Each implemented FV verb has its base and past
ender plus the Jeff-style optional continuation when one applies (`a`, `it`,
`like`, `on`, `that`, `the`, or `to`). `Used to` is not treated as an ordinary
`use` continuation because it needs fixed-form handling.

New IV stems declare per-verb tail allowlists. Runtime lookup and the trainer
both honor them, so the expanded bank excludes ungrammatical combinations while
retaining every useful tail available to that verb.

Three planned strokes required collision-safe adjustments:

| Verb | Planned Assignment | Implemented Assignment |
| --- | --- | --- |
| to ask, FV | `RB` | `RBS`; `RB` remains `catch` |
| to run, IV | `KH` | `R`; `KH` remains `catch` |
| to make, IV | `KPL` | `KPL` with only collision-free tails (`a`, `an`, `us`, `her`, `his`, `of`, `that`) |

`To call` remains unchanged: its IV stem is implemented as `KHR`, while its FV
ender is still deferred.
