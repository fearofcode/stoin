#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'EOF'
usage: scripts/stoin-srs-restore.sh DUMP.sql [DATABASE]

Restore a Stoin SRS SQL dump into DATABASE (default: stoin-srs-web.sqlite3).
Stop the SRS web app before running this script. If DATABASE already exists,
the script first creates a timestamped .before-restore backup beside it.
EOF
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
	usage
	exit 0
fi
if (( $# < 1 || $# > 2 )); then
	usage >&2
	exit 2
fi
if ! command -v sqlite3 >/dev/null 2>&1; then
	printf 'error: sqlite3 is required to restore a database dump\n' >&2
	exit 1
fi

dump=$1
database=${2:-stoin-srs-web.sqlite3}
if [[ ! -f $dump || ! -r $dump ]]; then
	printf 'error: dump is not a readable file: %s\n' "$dump" >&2
	exit 1
fi

database_dir=$(dirname "$database")
if [[ ! -d $database_dir ]]; then
	printf 'error: database directory does not exist: %s\n' "$database_dir" >&2
	exit 1
fi

tmp=$(mktemp "${database}.restore.XXXXXX")
cleanup() {
	rm -f "$tmp" "${tmp}-wal" "${tmp}-shm"
}
trap cleanup EXIT

if ! sqlite3 -batch -bail "$tmp" < "$dump"; then
	printf 'error: SQL dump could not be restored; target database was not changed\n' >&2
	exit 1
fi

while read -r table columns; do
	exists=$(sqlite3 -batch -noheader "$tmp" \
		"SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = '$table';")
	if [[ $exists != "1" ]]; then
		printf 'error: restored dump is missing required table %s; target database was not changed\n' "$table" >&2
		exit 1
	fi
	for column in $columns; do
		exists=$(sqlite3 -batch -noheader "$tmp" \
			"SELECT COUNT(*) FROM pragma_table_info('$table') WHERE name = '$column';")
		if [[ $exists != "1" ]]; then
			printf 'error: restored dump is missing required column %s.%s; target database was not changed\n' "$table" "$column" >&2
			exit 1
		fi
	done
done <<'EOF'
decks id name created_at
groups id deck_id name created_at
imports id deck_id content_hash created_at
ingest_batches id deck_id group_id source created_at
items id group_id text created_at updated_at last_ingest_batch_id intro_remaining schedule_stage interval_days due_at review_count correct_count incorrect_count
reviews id item_id mode prompt answer correct created_at
EOF

integrity=$(sqlite3 -batch -noheader "$tmp" 'PRAGMA integrity_check;')
if [[ $integrity != "ok" ]]; then
	printf 'error: restored database failed integrity_check:\n%s\n' "$integrity" >&2
	exit 1
fi
foreign_key_errors=$(sqlite3 -batch -noheader "$tmp" 'PRAGMA foreign_key_check;')
if [[ -n $foreign_key_errors ]]; then
	printf 'error: restored database failed foreign_key_check:\n%s\n' "$foreign_key_errors" >&2
	exit 1
fi

backup=""
if [[ -e $database ]]; then
	backup=$(mktemp "${database}.before-restore-$(date -u +%Y%m%dT%H%M%SZ).XXXXXX")
	escaped_backup=${backup//\\/\\\\}
	escaped_backup=${escaped_backup//\"/\\\"}
	if ! sqlite3 -batch -bail "$database" ".backup \"$escaped_backup\""; then
		rm -f "$backup" "${backup}-wal" "${backup}-shm"
		printf 'error: could not back up existing target; target database was not changed\n' >&2
		exit 1
	fi
fi

# The app must be stopped: removing stale sidecars while it is running can lose
# writes. Keeping this operation next to the atomic replacement narrows the
# window and leaves the original target untouched until validation succeeds.
rm -f "${database}-wal" "${database}-shm"
mv -f "$tmp" "$database"
trap - EXIT

printf 'restored %s from %s\n' "$database" "$dump"
if [[ -n $backup ]]; then
	printf 'previous database backed up to %s\n' "$backup"
fi
