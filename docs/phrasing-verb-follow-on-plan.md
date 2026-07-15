# Phrasing Verb Follow-On Status

The verb follow-on work is complete. Both initial-verb and final-verb phrases
remain available after the non-pedal redesign; neither family needed to be
dropped in favor of a final-verb-only Jeff layout.

The semantic inventory from the original follow-on work is unchanged. The
collision-safe chord migration made these mechanical changes:

- every IV stem uses a Phoenix-safe `O`-bearing starter, and the progressive
  form uses `*`;
- FV subjects use Phoenix-empty unique starter banks;
- FV contractions add `U` instead of the number bar;
- the FV `hold` ender is `-PBL` / `-PBLD`, leaving `-F` exclusively available
  for the perfect structure; and
- NV prefixes use Phoenix-safe two- and three-key left-hand starters.

The existing per-verb tail allowlists remain in place. Expanding them blindly
would still introduce Phoenix collisions such as `SPROUT` and `TWROET`, despite
the removal of the number bar.

The complete current assignments and the required collision audit are in
[`phrasing-keyboard-only-design.md`](phrasing-keyboard-only-design.md). The
machine-readable source of truth remains [`phrasing.json`](../phrasing.json).
