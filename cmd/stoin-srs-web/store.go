package main

import (
	"context"
	"database/sql"
	"errors"
	"strings"
	"time"
)

var ErrDuplicateItem = errors.New("item already exists in deck")

func (a *App) initSchema(ctx context.Context) error {
	_, err := a.db.ExecContext(ctx, `
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS decks (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL UNIQUE,
	created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS groups (
	id INTEGER PRIMARY KEY,
	deck_id INTEGER NOT NULL REFERENCES decks(id) ON DELETE CASCADE,
	name TEXT NOT NULL,
	created_at TEXT NOT NULL,
	UNIQUE(deck_id, name)
);

CREATE TABLE IF NOT EXISTS imports (
	id INTEGER PRIMARY KEY,
	deck_id INTEGER NOT NULL REFERENCES decks(id) ON DELETE CASCADE,
	content_hash TEXT NOT NULL,
	created_at TEXT NOT NULL,
	UNIQUE(deck_id, content_hash)
);

CREATE TABLE IF NOT EXISTS ingest_batches (
	id INTEGER PRIMARY KEY,
	deck_id INTEGER NOT NULL REFERENCES decks(id) ON DELETE CASCADE,
	group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
	source TEXT,
	created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS items (
	id INTEGER PRIMARY KEY,
	group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
	text TEXT NOT NULL,
	created_at TEXT NOT NULL,
	updated_at TEXT NOT NULL,
	last_ingest_batch_id INTEGER REFERENCES ingest_batches(id),
	intro_remaining INTEGER NOT NULL DEFAULT 5,
	schedule_stage INTEGER NOT NULL DEFAULT 0,
	interval_days REAL NOT NULL DEFAULT 0,
	due_at TEXT NOT NULL,
	review_count INTEGER NOT NULL DEFAULT 0,
	correct_count INTEGER NOT NULL DEFAULT 0,
	incorrect_count INTEGER NOT NULL DEFAULT 0,
	UNIQUE(group_id, text)
);

CREATE TABLE IF NOT EXISTS reviews (
	id INTEGER PRIMARY KEY,
	item_id INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,
	mode TEXT NOT NULL,
	prompt TEXT NOT NULL,
	answer TEXT NOT NULL,
	correct INTEGER NOT NULL,
	created_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_groups_deck ON groups(deck_id);
CREATE INDEX IF NOT EXISTS idx_items_group ON items(group_id);
CREATE INDEX IF NOT EXISTS idx_items_due ON items(due_at);
CREATE INDEX IF NOT EXISTS idx_items_intro ON items(intro_remaining);
CREATE INDEX IF NOT EXISTS idx_imports_deck ON imports(deck_id);
`)
	return err
}

func (a *App) indexData(ctx context.Context, issues []ParseIssue, form ImportFormData) (IndexPageData, error) {
	decks, err := a.listDecks(ctx)
	if err != nil {
		return IndexPageData{}, err
	}
	dueCount, err := a.countDue(ctx, 0)
	if err != nil {
		return IndexPageData{}, err
	}
	learning, err := a.learningStats(ctx, 0)
	if err != nil {
		return IndexPageData{}, err
	}
	return IndexPageData{
		Decks:          decks,
		Errors:         issues,
		Form:           form,
		DueLimit:       reviewAllDueLimit,
		DueCount:       dueCount,
		LearningCount:  learning.Count,
		IntroRemaining: learning.IntroRemaining,
	}, nil
}

func (a *App) deckData(ctx context.Context, deckID int64, issues []ParseIssue, form ImportFormData) (DeckPageData, error) {
	deck, err := a.deckByID(ctx, deckID)
	if err != nil {
		return DeckPageData{}, err
	}
	groups, err := a.groupsForDeck(ctx, deckID)
	if err != nil {
		return DeckPageData{}, err
	}
	dueCount, err := a.countDue(ctx, deckID)
	if err != nil {
		return DeckPageData{}, err
	}
	learning, err := a.learningStats(ctx, deckID)
	if err != nil {
		return DeckPageData{}, err
	}
	total := 0
	for _, group := range groups {
		total += len(group.Items)
	}
	form.DeckID = deckID
	return DeckPageData{
		Deck:           deck,
		Groups:         groups,
		Errors:         issues,
		Form:           form,
		TotalItems:     total,
		DueCount:       dueCount,
		LearningCount:  learning.Count,
		IntroRemaining: learning.IntroRemaining,
	}, nil
}

func (a *App) listDecks(ctx context.Context) ([]Deck, error) {
	rows, err := a.db.QueryContext(ctx, `
SELECT id, name, created_at
FROM decks
ORDER BY name`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var decks []Deck
	for rows.Next() {
		var deck Deck
		if err := rows.Scan(&deck.ID, &deck.Name, &deck.CreatedAt); err != nil {
			return nil, err
		}
		decks = append(decks, deck)
	}
	return decks, rows.Err()
}

func (a *App) deckByID(ctx context.Context, id int64) (Deck, error) {
	var deck Deck
	err := a.db.QueryRowContext(ctx, `
SELECT id, name, created_at
FROM decks
WHERE id = ?`, id).Scan(&deck.ID, &deck.Name, &deck.CreatedAt)
	return deck, err
}

func (a *App) groupsForDeck(ctx context.Context, deckID int64) ([]Group, error) {
	rows, err := a.db.QueryContext(ctx, `
SELECT
	g.id,
	g.name,
	g.created_at,
	i.id,
	i.text,
	i.intro_remaining,
	i.schedule_stage,
	i.interval_days,
	i.due_at,
	i.review_count,
	i.correct_count,
	i.incorrect_count
FROM groups g
LEFT JOIN items i ON i.group_id = g.id
WHERE g.deck_id = ?
ORDER BY g.name, i.id`, deckID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	groupIndex := map[int64]int{}
	var groups []Group
	for rows.Next() {
		var (
			groupID        int64
			groupName      string
			groupCreated   string
			itemID         sql.NullInt64
			itemText       sql.NullString
			intro          sql.NullInt64
			stage          sql.NullInt64
			interval       sql.NullFloat64
			dueAt          sql.NullString
			reviewCount    sql.NullInt64
			correctCount   sql.NullInt64
			incorrectCount sql.NullInt64
		)
		if err := rows.Scan(
			&groupID,
			&groupName,
			&groupCreated,
			&itemID,
			&itemText,
			&intro,
			&stage,
			&interval,
			&dueAt,
			&reviewCount,
			&correctCount,
			&incorrectCount,
		); err != nil {
			return nil, err
		}
		index, ok := groupIndex[groupID]
		if !ok {
			index = len(groups)
			groupIndex[groupID] = index
			groups = append(groups, Group{
				ID:        groupID,
				DeckID:    deckID,
				Name:      groupName,
				CreatedAt: groupCreated,
			})
		}
		if itemID.Valid {
			due, _ := parseDBTime(dueAt.String)
			introRemaining := int(intro.Int64)
			groups[index].Items = append(groups[index].Items, Item{
				ID:             itemID.Int64,
				GroupID:        groupID,
				GroupName:      groupName,
				Text:           itemText.String,
				IntroRemaining: introRemaining,
				ScheduleStage:  int(stage.Int64),
				IntervalDays:   interval.Float64,
				DueAt:          due,
				ReviewCount:    int(reviewCount.Int64),
				CorrectCount:   int(correctCount.Int64),
				IncorrectCount: int(incorrectCount.Int64),
			})
			if introRemaining > 0 {
				groups[index].LearningCount++
				groups[index].IntroRemaining += introRemaining
			}
		}
	}
	return groups, rows.Err()
}

func (a *App) countDue(ctx context.Context, deckID int64) (int, error) {
	args := []any{formatDBTime(time.Now().UTC())}
	filter := ""
	if deckID > 0 {
		filter = " AND g.deck_id = ?"
		args = append(args, deckID)
	}
	var count int
	err := a.db.QueryRowContext(ctx, `
SELECT COUNT(*)
FROM items i
JOIN groups g ON g.id = i.group_id
WHERE (i.intro_remaining > 0 OR i.due_at <= ?)`+filter, args...).Scan(&count)
	return count, err
}

func (a *App) learningStats(ctx context.Context, deckID int64) (LearningStats, error) {
	args := []any{}
	filter := ""
	if deckID > 0 {
		filter = " AND g.deck_id = ?"
		args = append(args, deckID)
	}
	var stats LearningStats
	err := a.db.QueryRowContext(ctx, `
SELECT COUNT(*), COALESCE(SUM(i.intro_remaining), 0)
FROM items i
JOIN groups g ON g.id = i.group_id
WHERE i.intro_remaining > 0`+filter, args...).Scan(&stats.Count, &stats.IntroRemaining)
	return stats, err
}

func (a *App) validateImportDeck(ctx context.Context, form ImportFormData) (int64, []ParseIssue, error) {
	if form.DeckName != "" {
		return 0, nil, nil
	}
	if form.DeckID > 0 {
		_, err := a.deckByID(ctx, form.DeckID)
		if errors.Is(err, sql.ErrNoRows) {
			return 0, []ParseIssue{{Line: 1, Message: "selected deck does not exist"}}, nil
		}
		return form.DeckID, nil, err
	}
	return 0, []ParseIssue{{Line: 1, Message: "choose an existing deck or enter a new deck name"}}, nil
}

func (a *App) getOrCreateDeck(ctx context.Context, name string, now time.Time) (int64, error) {
	if name == "" {
		return 0, errors.New("deck name is empty")
	}
	if _, err := a.db.ExecContext(ctx, `
INSERT OR IGNORE INTO decks(name, created_at)
VALUES(?, ?)`, name, formatDBTime(now)); err != nil {
		return 0, err
	}
	var id int64
	err := a.db.QueryRowContext(ctx, `SELECT id FROM decks WHERE name = ?`, name).Scan(&id)
	return id, err
}

func (a *App) getOrCreateGroup(ctx context.Context, tx *sql.Tx, deckID int64, name string, now time.Time) (int64, error) {
	if _, err := tx.ExecContext(ctx, `
INSERT OR IGNORE INTO groups(deck_id, name, created_at)
VALUES(?, ?, ?)`, deckID, name, formatDBTime(now)); err != nil {
		return 0, err
	}
	var id int64
	err := tx.QueryRowContext(ctx, `
SELECT id FROM groups
WHERE deck_id = ? AND name = ?`, deckID, name).Scan(&id)
	return id, err
}

func (a *App) ingestGroups(ctx context.Context, deckID int64, groups []ImportGroup, source string, contentHash string) (ImportStats, error) {
	now := time.Now().UTC()
	stats := ImportStats{Groups: len(groups)}

	tx, err := a.db.BeginTx(ctx, nil)
	if err != nil {
		return stats, err
	}
	defer tx.Rollback()

	var already int
	if err := tx.QueryRowContext(ctx, `
SELECT COUNT(*)
FROM imports
WHERE deck_id = ? AND content_hash = ?`, deckID, contentHash).Scan(&already); err != nil {
		return stats, err
	}
	if already > 0 {
		stats.DuplicateImport = true
		return stats, tx.Commit()
	}
	if _, err := tx.ExecContext(ctx, `
INSERT INTO imports(deck_id, content_hash, created_at)
VALUES(?, ?, ?)`, deckID, contentHash, formatDBTime(now)); err != nil {
		return stats, err
	}

	for _, group := range groups {
		groupID, err := a.getOrCreateGroup(ctx, tx, deckID, group.Name, now)
		if err != nil {
			return stats, err
		}
		res, err := tx.ExecContext(ctx, `
INSERT INTO ingest_batches(deck_id, group_id, source, created_at)
VALUES(?, ?, ?, ?)`, deckID, groupID, source, formatDBTime(now))
		if err != nil {
			return stats, err
		}
		batchID, err := res.LastInsertId()
		if err != nil {
			return stats, err
		}
		for _, word := range group.Words {
			stats.ItemsRead++
			itemID, exists, err := existingDeckItem(ctx, tx, deckID, word)
			if err != nil {
				return stats, err
			}
			if exists {
				_ = itemID
				stats.Existing++
				continue
			}
			if _, err := tx.ExecContext(ctx, `
INSERT INTO items(
	group_id,
	text,
	created_at,
	updated_at,
	last_ingest_batch_id,
	intro_remaining,
	schedule_stage,
	interval_days,
	due_at
)
VALUES(?, ?, ?, ?, ?, ?, 0, 0, ?)`,
				groupID,
				word,
				formatDBTime(now),
				formatDBTime(now),
				batchID,
				introRepetitions,
				formatDBTime(now),
			); err != nil {
				return stats, err
			}
			stats.Added++
		}
	}

	return stats, tx.Commit()
}

func existingDeckItem(ctx context.Context, tx *sql.Tx, deckID int64, word string) (int64, bool, error) {
	var id int64
	err := tx.QueryRowContext(ctx, `
SELECT i.id
FROM items i
JOIN groups g ON g.id = i.group_id
WHERE g.deck_id = ? AND i.text = ?
ORDER BY i.id
LIMIT 1`, deckID, word).Scan(&id)
	if errors.Is(err, sql.ErrNoRows) {
		return 0, false, nil
	}
	if err != nil {
		return 0, false, err
	}
	return id, true, nil
}

func (a *App) updateItemText(ctx context.Context, deckID int64, itemID int64, text string) error {
	tx, err := a.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	var groupID int64
	if err := tx.QueryRowContext(ctx, `
SELECT i.group_id
FROM items i
JOIN groups g ON g.id = i.group_id
WHERE i.id = ? AND g.deck_id = ?`, itemID, deckID).Scan(&groupID); err != nil {
		return err
	}

	var duplicateID int64
	err = tx.QueryRowContext(ctx, `
SELECT i.id
FROM items i
JOIN groups g ON g.id = i.group_id
WHERE g.deck_id = ? AND i.text = ? AND i.id <> ?
LIMIT 1`, deckID, text, itemID).Scan(&duplicateID)
	if err == nil {
		return ErrDuplicateItem
	}
	if !errors.Is(err, sql.ErrNoRows) {
		return err
	}

	if _, err := tx.ExecContext(ctx, `
UPDATE items
SET text = ?, updated_at = ?
WHERE id = ?`,
		text,
		formatDBTime(time.Now().UTC()),
		itemID,
	); err != nil {
		if strings.Contains(strings.ToLower(err.Error()), "constraint") {
			return ErrDuplicateItem
		}
		return err
	}
	return tx.Commit()
}

func (a *App) deleteItem(ctx context.Context, deckID int64, itemID int64) error {
	res, err := a.db.ExecContext(ctx, `
DELETE FROM items
WHERE id IN (
	SELECT i.id
	FROM items i
	JOIN groups g ON g.id = i.group_id
	WHERE i.id = ? AND g.deck_id = ?
)`, itemID, deckID)
	if err != nil {
		return err
	}
	affected, err := res.RowsAffected()
	if err != nil {
		return err
	}
	if affected == 0 {
		return sql.ErrNoRows
	}
	return nil
}

func (a *App) itemsByID(ctx context.Context, ids []int64) ([]SessionItem, error) {
	if len(ids) == 0 {
		return nil, nil
	}
	placeholders := strings.TrimRight(strings.Repeat("?,", len(ids)), ",")
	args := make([]any, 0, len(ids))
	for _, id := range ids {
		args = append(args, id)
	}

	rows, err := a.db.QueryContext(ctx, `
SELECT
	i.id,
	i.text,
	d.name,
	g.name
FROM items i
JOIN groups g ON g.id = i.group_id
JOIN decks d ON d.id = g.deck_id
WHERE i.id IN (`+placeholders+`)`, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	byID := map[int64]SessionItem{}
	for rows.Next() {
		var item SessionItem
		if err := rows.Scan(&item.ID, &item.Text, &item.DeckName, &item.GroupName); err != nil {
			return nil, err
		}
		byID[item.ID] = item
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}

	items := make([]SessionItem, 0, len(ids))
	for _, id := range ids {
		if item, ok := byID[id]; ok {
			items = append(items, item)
		}
	}
	return items, nil
}

func (a *App) itemsForDeck(ctx context.Context, deckID int64) ([]SessionItem, error) {
	rows, err := a.db.QueryContext(ctx, `
SELECT
	i.id,
	i.text,
	d.name,
	g.name
FROM items i
JOIN groups g ON g.id = i.group_id
JOIN decks d ON d.id = g.deck_id
WHERE d.id = ?
ORDER BY g.name, i.id`, deckID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var items []SessionItem
	for rows.Next() {
		var item SessionItem
		if err := rows.Scan(&item.ID, &item.Text, &item.DeckName, &item.GroupName); err != nil {
			return nil, err
		}
		items = append(items, item)
	}
	return items, rows.Err()
}

func (a *App) dueItems(ctx context.Context, deckID int64, limit int) ([]SessionItem, error) {
	args := []any{formatDBTime(time.Now().UTC())}
	filter := ""
	if deckID > 0 {
		filter = " AND g.deck_id = ?"
		args = append(args, deckID)
	}
	args = append(args, limit)
	rows, err := a.db.QueryContext(ctx, `
SELECT
	i.id,
	i.text,
	d.name,
	g.name
FROM items i
JOIN groups g ON g.id = i.group_id
JOIN decks d ON d.id = g.deck_id
WHERE (i.intro_remaining > 0 OR i.due_at <= ?)`+filter+`
ORDER BY i.due_at, i.id
LIMIT ?`, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var items []SessionItem
	for rows.Next() {
		var item SessionItem
		if err := rows.Scan(&item.ID, &item.Text, &item.DeckName, &item.GroupName); err != nil {
			return nil, err
		}
		items = append(items, item)
	}
	return items, rows.Err()
}

func (a *App) applyReviewBatch(ctx context.Context, results []ReviewResult) error {
	now := time.Now().UTC()
	tx, err := a.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	for _, result := range results {
		if err := applyOneReview(ctx, tx, result, now); err != nil {
			return err
		}
	}
	return tx.Commit()
}

func applyOneReview(ctx context.Context, tx *sql.Tx, result ReviewResult, now time.Time) error {
	var introRemaining int
	var stage int
	if err := tx.QueryRowContext(ctx, `
SELECT intro_remaining, schedule_stage
FROM items
WHERE id = ?`, result.ItemID).Scan(&introRemaining, &stage); err != nil {
		return err
	}

	intervalDays := 0.0
	dueAt := now
	if result.Correct {
		if introRemaining > 1 {
			introRemaining--
		} else if introRemaining == 1 {
			introRemaining = 0
			stage = 0
			intervalDays = float64(scheduleDays[stage])
			dueAt = now.AddDate(0, 0, scheduleDays[stage])
		} else {
			if stage < len(scheduleDays)-1 {
				stage++
			}
			intervalDays = float64(scheduleDays[stage])
			dueAt = now.AddDate(0, 0, scheduleDays[stage])
		}
	} else {
		introRemaining = introRepetitions
		stage = 0
		intervalDays = 0
		dueAt = now
	}

	if _, err := tx.ExecContext(ctx, `
UPDATE items
SET
	intro_remaining = ?,
	schedule_stage = ?,
	interval_days = ?,
	due_at = ?,
	review_count = review_count + 1,
	correct_count = correct_count + ?,
	incorrect_count = incorrect_count + ?,
	updated_at = ?
WHERE id = ?`,
		introRemaining,
		stage,
		intervalDays,
		formatDBTime(dueAt),
		boolInt(result.Correct),
		boolInt(!result.Correct),
		formatDBTime(now),
		result.ItemID,
	); err != nil {
		return err
	}

	_, err := tx.ExecContext(ctx, `
INSERT INTO reviews(item_id, mode, prompt, answer, correct, created_at)
VALUES(?, 'text', ?, ?, ?, ?)`,
		result.ItemID,
		result.Prompt,
		result.Answer,
		boolInt(result.Correct),
		formatDBTime(now),
	)
	return err
}
