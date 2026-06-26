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

Each deck page shows groups and words. Select whole groups or individual words,
then choose:

- `Review selected`: saves all results in one batch when you submit at the end.
- `Practice selected`: does not update due dates.
- `Practice all`: practices every word in the deck without changing the current
  checkbox selection.

Practice mode accepts a practice count before starting the session.
