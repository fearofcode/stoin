#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
usage: scripts/reset_phrases.sh [DATABASE]

Reset SRS progress and review history for every item in a group named
"Phrases" (case-insensitive), including any daily recovery period. DATABASE
defaults to stoin-srs-web.sqlite3.

The script previews the number of affected items and reviews, requires
confirmation, and creates a timestamped SQLite backup before changing anything.
EOF
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
	usage
	exit 0
fi
if (( $# > 1 )); then
	usage >&2
	exit 2
fi
if ! command -v sqlite3 >/dev/null 2>&1; then
	printf 'error: sqlite3 is required to reset phrase progress\n' >&2
	exit 1
fi

database=${1:-stoin-srs-web.sqlite3}
if [[ ! -f $database || ! -r $database || ! -w $database ]]; then
	printf 'error: database is not a readable and writable file: %s\n' "$database" >&2
	exit 1
fi

for table in groups items reviews; do
	exists=$(sqlite3 -batch -noheader "$database" \
		"SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = '$table';")
	if [[ $exists != "1" ]]; then
		printf 'error: database is missing required table %s\n' "$table" >&2
		exit 1
	fi
done

read -r phrase_items phrase_reviews < <(
	sqlite3 -batch -noheader -separator ' ' "$database" '
SELECT COUNT(DISTINCT i.id), COUNT(r.id)
FROM items i
JOIN groups g ON g.id = i.group_id
LEFT JOIN reviews r ON r.item_id = i.id
WHERE lower(trim(g.name)) = "phrases";
'
)

has_recovery_column=$(sqlite3 -batch -noheader "$database" \
	"SELECT COUNT(*) FROM pragma_table_info('items') WHERE name = 'recovery_days_remaining';")
recovery_reset_sql=""
if [[ $has_recovery_column == "1" ]]; then
	recovery_reset_sql=$'\trecovery_days_remaining = 0,\n'
fi

if [[ $phrase_items == "0" ]]; then
	printf 'no items found in groups named "Phrases" in %s\n' "$database"
	exit 0
fi

printf 'database: %s\n' "$database"
printf 'phrase items to reset: %s\n' "$phrase_items"
printf 'phrase reviews to delete: %s\n' "$phrase_reviews"
printf 'type reset to continue: '
read -r confirmation
if [[ $confirmation != "reset" ]]; then
	printf 'reset cancelled; database was not changed\n'
	exit 0
fi

backup=$(mktemp "${database}.before-phrase-reset-$(date -u +%Y%m%dT%H%M%SZ).XXXXXX")
escaped_backup=${backup//\\/\\\\}
escaped_backup=${escaped_backup//\"/\\\"}
if ! sqlite3 -batch -bail "$database" ".backup \"$escaped_backup\""; then
	rm -f "$backup" "${backup}-wal" "${backup}-shm"
	printf 'error: could not back up database; reset was not attempted\n' >&2
	exit 1
fi

reset_items=$(
	sqlite3 -batch -bail -noheader "$database" <<SQL
.timeout 10000
PRAGMA foreign_keys = ON;
BEGIN IMMEDIATE;

DELETE FROM reviews
WHERE item_id IN (
	SELECT i.id
	FROM items i
	JOIN groups g ON g.id = i.group_id
	WHERE lower(trim(g.name)) = 'phrases'
);

UPDATE items
SET
	intro_remaining = 5,
	schedule_stage = 0,
${recovery_reset_sql}	interval_days = 0,
	due_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
	review_count = 0,
	correct_count = 0,
	incorrect_count = 0,
	updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now')
WHERE group_id IN (
	SELECT id
	FROM groups
	WHERE lower(trim(name)) = 'phrases'
);

SELECT changes();
COMMIT;
SQL
)

integrity=$(sqlite3 -batch -bail -noheader "$database" 'PRAGMA integrity_check;')
foreign_key_errors=$(sqlite3 -batch -bail -noheader "$database" 'PRAGMA foreign_key_check;')
if [[ $integrity != "ok" || -n $foreign_key_errors ]]; then
	printf 'error: post-reset database validation failed\n' >&2
	printf 'backup: %s\n' "$backup" >&2
	exit 1
fi

printf 'reset %s phrase items and deleted %s phrase reviews\n' "$reset_items" "$phrase_reviews"
printf 'backup: %s\n' "$backup"
