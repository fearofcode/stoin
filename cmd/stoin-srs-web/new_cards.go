package main

import (
	"context"
	"strings"
	"time"
)

func limitedNewCardGroupPredicate(qualifier string) string {
	return `TRIM(` + qualifier + `name) COLLATE NOCASE IN ('Mandatories', 'Briefs', 'Phrases')`
}

func isLimitedNewCardGroup(name string) bool {
	switch strings.ToLower(strings.TrimSpace(name)) {
	case "mandatories", "briefs", "phrases":
		return true
	default:
		return false
	}
}

func (a *App) currentTime() time.Time {
	if a.now != nil {
		return a.now()
	}
	return time.Now()
}

func localDayBounds(now time.Time) (time.Time, time.Time) {
	local := now.In(now.Location())
	start := time.Date(local.Year(), local.Month(), local.Day(), 0, 0, 0, 0, local.Location())
	return start.UTC(), start.AddDate(0, 0, 1).UTC()
}

// introduceDailyItems admits one shared daily batch from Mandatories, Briefs,
// and Phrases. introduced_at is permanent, so an admitted card never returns
// to the queue if the user does not finish it that day.
func (a *App) introduceDailyItems(ctx context.Context) error {
	if a.dailyNewLimit <= 0 {
		return nil
	}
	now := a.currentTime()
	dayStart, dayEnd := localDayBounds(now)

	tx, err := a.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()

	// If a previous version admitted cards without accounting for the active
	// learning load, safely return today's untouched excess cards to the queue.
	// Cards with a recorded review after admission have already entered the
	// user's learning flow and are never withdrawn.
	var activeLearning int
	if err := tx.QueryRowContext(ctx, `
SELECT COUNT(*)
FROM items i
JOIN groups g ON g.id = i.group_id
JOIN decks d ON d.id = g.deck_id
WHERE `+limitedNewCardGroupPredicate("g.")+`
  AND d.paused = 0
  AND i.introduced_at IS NOT NULL
  AND i.intro_remaining > 0`).Scan(&activeLearning); err != nil {
		return err
	}
	if excess := activeLearning - a.dailyNewLimit; excess > 0 {
		if _, err := tx.ExecContext(ctx, `
UPDATE items
SET introduced_at = NULL
WHERE id IN (
	SELECT i.id
	FROM items i
	JOIN groups g ON g.id = i.group_id
	JOIN decks d ON d.id = g.deck_id
	WHERE `+limitedNewCardGroupPredicate("g.")+`
	  AND d.paused = 0
	  AND i.intro_remaining > 0
	  AND i.introduced_at >= ?
	  AND i.introduced_at < ?
	  AND NOT EXISTS (
		SELECT 1
		FROM reviews r
		WHERE r.item_id = i.id
		  AND r.created_at >= i.introduced_at
	  )
	ORDER BY i.introduced_at DESC, i.id DESC
	LIMIT ?
)`, formatDBTime(dayStart), formatDBTime(dayEnd), excess); err != nil {
			return err
		}
	}

	var introducedToday int
	if err := tx.QueryRowContext(ctx, `
SELECT
	COALESCE(SUM(CASE
		WHEN i.introduced_at >= ? AND i.introduced_at < ? THEN 1
		ELSE 0
	END), 0),
	COALESCE(SUM(CASE
		WHEN d.paused = 0 AND i.introduced_at IS NOT NULL AND i.intro_remaining > 0 THEN 1
		ELSE 0
	END), 0)
FROM items i
JOIN groups g ON g.id = i.group_id
JOIN decks d ON d.id = g.deck_id
WHERE `+limitedNewCardGroupPredicate("g."),
		formatDBTime(dayStart),
		formatDBTime(dayEnd),
	).Scan(&introducedToday, &activeLearning); err != nil {
		return err
	}

	remaining := a.dailyNewLimit - introducedToday
	if learningCapacity := a.dailyNewLimit - activeLearning; remaining > learningCapacity {
		remaining = learningCapacity
	}
	if remaining > 0 {
		if _, err := tx.ExecContext(ctx, `
UPDATE items
SET introduced_at = ?
WHERE id IN (
	SELECT i.id
	FROM items i
	JOIN groups g ON g.id = i.group_id
	JOIN decks d ON d.id = g.deck_id
	WHERE i.introduced_at IS NULL
	  AND d.paused = 0
	  AND `+limitedNewCardGroupPredicate("g.")+`
	ORDER BY i.created_at, i.id
	LIMIT ?
)`, formatDBTime(now.UTC()), remaining); err != nil {
			return err
		}
	}
	return tx.Commit()
}
