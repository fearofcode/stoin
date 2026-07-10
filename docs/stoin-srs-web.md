# Stoin SRS Web App

Run the SQLite-backed browser app:

```sh
make srs-web
```

The default URL is:

```text
http://127.0.0.1:8080
```

Use a custom database or address directly:

```sh
go run ./cmd/stoin-srs-web --db practice.sqlite3 --addr 127.0.0.1:8090
```

The app uses `stoin-config.json` by default for review/practice hint outlines.
Point it at another active dictionary configuration with:

```sh
go run ./cmd/stoin-srs-web --config my-stoin-config.json
```

## Backups

The app exposes a live SQL dump at `/backup`, so you can back up a running app
without copying the SQLite file directly:

```sh
curl -fsS http://127.0.0.1:8080/backup -o stoin-srs-backup.sql
```

There is also a small helper script:

```sh
scripts/stoin-srs-backup.sh
scripts/stoin-srs-backup.sh http://127.0.0.1:8090/backup practice-backup.sql
```

## Import Format

Paste grouped text into the import form:

```text
words:
a
at
ate

brief practice:
is
the
and
```

Group names can contain spaces. A word ending in `:` is allowed inside a group;
a group header is only recognized at the start of the text or after a blank
line. For a plain newline-separated list, enter a plain-list group name in the
form.

Invalid imports show line-specific errors and do not create decks or words.

## Review And Practice

The root page lists decks and has a button to review all due words across all
decks. That cross-deck review pulls at most 100 words per session.

Newly imported words are in a learning/intro state for 5 correct reviews. They
still count as due during that phase, and the deck/root summaries show how many
items are learning plus the total intro repetitions remaining.

Each deck page shows groups and words. Select whole groups or individual words,
then choose:

- `Review selected`: saves all results in one batch when you submit at the end.
- `Review all`: reviews every word in the deck without changing the current
  checkbox selection.
- `Practice selected`: correct answers do not update due dates; hinted or
  skipped words reset to the intro schedule when the session is submitted.
- `Practice all`: practices every word in the deck without changing the current
  checkbox selection.

Practice mode accepts a practice count before starting the session.

During review or practice, `Hint` shows the current word's outline from the
configured dictionary stack. Requesting a hint marks that item as missed for the
session, but leaves the word active until you type it correctly. In review and
practice modes, that means the item is scheduled the same way as a skipped item
when the session is submitted.

At the end of a practice session, any hinted or skipped items appear once each
in a plain-text list beside the submit controls. The list can be copied for
review outside the app.

After a review submit, the app checks for more due words in the same review
scope. Deck reviews continue with due words from that deck; the root review-all
flow continues across all decks. A fully correct review session auto-submits
after a short countdown, while sessions with skipped entries wait for manual
submit.

## Phrasing Trainer

The root page links to `/phrasing`, a non-SRS trainer for Stoin's keyboard-only
phrasing assignments. It reads the same `initial_verbs`, `final_verbs`, and
`nonverbs` data used by the app and generates drills from selectable banks,
including `#` contraction forms.

Choose how many repetitions to practice, select IV/FV/NV banks, then practice
shuffled passes, random prompts, or selected bank-order blocks.
