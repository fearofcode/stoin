package main

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func testApp(t *testing.T) *App {
	t.Helper()
	app, err := NewAppWithPhrasing(t.TempDir()+"/srs.sqlite3", "../../phrasing.json")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if err := app.db.Close(); err != nil {
			t.Fatal(err)
		}
	})
	return app
}

func TestInitSchemaMigratesPausedDeckColumn(t *testing.T) {
	t.Parallel()
	dbPath := filepath.Join(t.TempDir(), "legacy.sqlite3")
	legacy, err := sql.Open("sqlite", dbPath)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := legacy.Exec(`
CREATE TABLE decks (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL UNIQUE,
	created_at TEXT NOT NULL
);
INSERT INTO decks(id, name, created_at) VALUES(1, 'legacy deck', '2026-01-01T00:00:00Z');`); err != nil {
		legacy.Close()
		t.Fatal(err)
	}
	if err := legacy.Close(); err != nil {
		t.Fatal(err)
	}

	app, err := NewAppWithPhrasing(dbPath, "../../phrasing.json")
	if err != nil {
		t.Fatal(err)
	}
	defer app.db.Close()

	deck, err := app.deckByID(context.Background(), 1)
	if err != nil {
		t.Fatal(err)
	}
	if deck.Name != "legacy deck" || deck.Paused {
		t.Fatalf("expected migrated active legacy deck, got %#v", deck)
	}
	var defaultValue sql.NullString
	if err := app.db.QueryRow(`
SELECT dflt_value
FROM pragma_table_info('decks')
WHERE name = 'paused'`).Scan(&defaultValue); err != nil {
		t.Fatal(err)
	}
	if !defaultValue.Valid || defaultValue.String != "0" {
		t.Fatalf("expected paused default 0, got %#v", defaultValue)
	}
}

func TestInitSchemaMigratesRecoveryDaysColumn(t *testing.T) {
	t.Parallel()
	dbPath := filepath.Join(t.TempDir(), "legacy.sqlite3")
	legacy, err := sql.Open("sqlite", dbPath)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := legacy.Exec(`
CREATE TABLE decks (
	id INTEGER PRIMARY KEY,
	name TEXT NOT NULL UNIQUE,
	created_at TEXT NOT NULL,
	paused INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE groups (
	id INTEGER PRIMARY KEY,
	deck_id INTEGER NOT NULL,
	name TEXT NOT NULL,
	created_at TEXT NOT NULL,
	UNIQUE(deck_id, name)
);
CREATE TABLE items (
	id INTEGER PRIMARY KEY,
	group_id INTEGER NOT NULL,
	text TEXT NOT NULL,
	created_at TEXT NOT NULL,
	updated_at TEXT NOT NULL,
	last_ingest_batch_id INTEGER,
	intro_remaining INTEGER NOT NULL DEFAULT 5,
	schedule_stage INTEGER NOT NULL DEFAULT 0,
	interval_days REAL NOT NULL DEFAULT 0,
	due_at TEXT NOT NULL,
	review_count INTEGER NOT NULL DEFAULT 0,
	correct_count INTEGER NOT NULL DEFAULT 0,
	incorrect_count INTEGER NOT NULL DEFAULT 0,
	UNIQUE(group_id, text)
);
INSERT INTO decks(id, name, created_at) VALUES(1, 'legacy deck', '2026-01-01T00:00:00Z');
INSERT INTO groups(id, deck_id, name, created_at) VALUES(1, 1, 'words', '2026-01-01T00:00:00Z');
INSERT INTO items(id, group_id, text, created_at, updated_at, due_at)
VALUES(1, 1, 'legacy item', '2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z', '2026-01-01T00:00:00Z');`); err != nil {
		legacy.Close()
		t.Fatal(err)
	}
	if err := legacy.Close(); err != nil {
		t.Fatal(err)
	}

	app, err := NewAppWithPhrasing(dbPath, "../../phrasing.json")
	if err != nil {
		t.Fatal(err)
	}
	defer app.db.Close()

	var defaultValue sql.NullString
	if err := app.db.QueryRow(`
SELECT dflt_value
FROM pragma_table_info('items')
WHERE name = 'recovery_days_remaining'`).Scan(&defaultValue); err != nil {
		t.Fatal(err)
	}
	if !defaultValue.Valid || defaultValue.String != "0" {
		t.Fatalf("expected recovery default 0, got %#v", defaultValue)
	}
	var recoveryDays int
	if err := app.db.QueryRow(`SELECT recovery_days_remaining FROM items WHERE id = 1`).Scan(&recoveryDays); err != nil {
		t.Fatal(err)
	}
	if recoveryDays != 0 {
		t.Fatalf("expected migrated item not to enter recovery, got %d days", recoveryDays)
	}
}

func TestParseGroupedImportAllowsColonWordsAndGroupSpaces(t *testing.T) {
	groups, issues := parseGroupedImport("common words:\na\nliteral:\nat\n\nbrief practice:\nis\nthe\n")
	if len(issues) != 0 {
		t.Fatalf("unexpected issues: %#v", issues)
	}
	if len(groups) != 2 {
		t.Fatalf("expected 2 groups, got %d", len(groups))
	}
	if groups[0].Name != "common words" || groups[1].Name != "brief practice" {
		t.Fatalf("unexpected groups: %#v", groups)
	}
	want := []string{"a", "literal:", "at"}
	for i, word := range want {
		if groups[0].Words[i] != word {
			t.Fatalf("word %d: want %q got %q", i, word, groups[0].Words[i])
		}
	}
}

func TestParseGroupedImportReportsLineErrors(t *testing.T) {
	_, issues := parseGroupedImport("orphan\n\nwords:\na\na\n\n:\n\nempty:\n")
	if len(issues) != 4 {
		t.Fatalf("expected 4 issues, got %#v", issues)
	}
	checks := []struct {
		line int
		text string
	}{
		{1, "word appears before any group header"},
		{5, "duplicate word"},
		{7, "group header is empty"},
		{9, "contains no words"},
	}
	for i, check := range checks {
		if issues[i].Line != check.line {
			t.Fatalf("issue %d line: want %d got %d", i, check.line, issues[i].Line)
		}
		if !contains(issues[i].Message, check.text) {
			t.Fatalf("issue %d message %q does not contain %q", i, issues[i].Message, check.text)
		}
	}
}

func TestParseDeduplicatedImportKeepsFirstEntries(t *testing.T) {
	groups, issues, duplicates := parseDeduplicatedImportText("alpha\nbeta\nalpha\n", "words")
	if len(issues) != 0 || duplicates != 1 {
		t.Fatalf("expected one removed plain-list duplicate, got groups %#v issues %#v duplicates %d", groups, issues, duplicates)
	}
	if len(groups) != 1 || len(groups[0].Words) != 2 || groups[0].Words[0] != "alpha" || groups[0].Words[1] != "beta" {
		t.Fatalf("unexpected deduplicated plain-list groups: %#v", groups)
	}

	groups, issues, duplicates = parseDeduplicatedImportText("first:\nalpha\n\nsecond:\nalpha\n", "")
	if len(issues) != 0 || duplicates != 1 {
		t.Fatalf("expected one removed grouped duplicate, got groups %#v issues %#v duplicates %d", groups, issues, duplicates)
	}
	if len(groups) != 1 || groups[0].Name != "first" || len(groups[0].Words) != 1 || groups[0].Words[0] != "alpha" {
		t.Fatalf("expected duplicate-only group to be omitted, got %#v", groups)
	}
}

func TestIngestGroupsSkipsDuplicateWordsAcrossDecks(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	firstDeckID, err := app.getOrCreateDeck(ctx, "first", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	secondDeckID, err := app.getOrCreateDeck(ctx, "second", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}

	_, err = app.ingestGroups(ctx, firstDeckID, []ImportGroup{{Name: "words", Words: []string{"a"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	stats, err := app.ingestGroups(ctx, secondDeckID, []ImportGroup{{Name: "briefs", Words: []string{"a", "the"}}}, "test", "two")
	if err != nil {
		t.Fatal(err)
	}
	if stats.Added != 1 || stats.Existing != 1 {
		t.Fatalf("unexpected stats: %#v", stats)
	}

	var count int
	if err := app.db.QueryRow(`SELECT COUNT(*) FROM items`).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 2 {
		t.Fatalf("expected 2 items, got %d", count)
	}
	var duplicateOwner int64
	if err := app.db.QueryRow(`
SELECT g.deck_id
FROM items i
JOIN groups g ON g.id = i.group_id
WHERE i.text = 'a'`).Scan(&duplicateOwner); err != nil {
		t.Fatal(err)
	}
	if duplicateOwner != firstDeckID {
		t.Fatalf("expected original deck %d to retain a, got deck %d", firstDeckID, duplicateOwner)
	}
}

func TestApplyReviewBatchAdvancesAndResetsSchedule(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"unless"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}
	if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "", Correct: false}}); err != nil {
		t.Fatal(err)
	}
	var intro int
	var stage int
	var recoveryDays int
	var interval float64
	if err := app.db.QueryRow(`
SELECT intro_remaining, schedule_stage, recovery_days_remaining, interval_days
FROM items
WHERE id = ?`, itemID).Scan(&intro, &stage, &recoveryDays, &interval); err != nil {
		t.Fatal(err)
	}
	if intro != introRepetitions || stage != 0 || recoveryDays != 0 || interval != 0 {
		t.Fatalf(
			"expected intro miss not to add recovery, got intro=%d stage=%d recovery=%d interval=%v",
			intro, stage, recoveryDays, interval,
		)
	}

	for i := 0; i < introRepetitions; i++ {
		if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "unless", Correct: true}}); err != nil {
			t.Fatal(err)
		}
	}
	if err := app.db.QueryRow(`
SELECT intro_remaining, schedule_stage, recovery_days_remaining, interval_days
FROM items
WHERE id = ?`, itemID).Scan(&intro, &stage, &recoveryDays, &interval); err != nil {
		t.Fatal(err)
	}
	if intro != 0 || stage != 0 || recoveryDays != 0 || interval != 1 {
		t.Fatalf(
			"expected learned item at ordinary 1 day interval, got intro=%d stage=%d recovery=%d interval=%v",
			intro, stage, recoveryDays, interval,
		)
	}

	if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "", Correct: false}}); err != nil {
		t.Fatal(err)
	}
	if err := app.db.QueryRow(`
SELECT intro_remaining, schedule_stage, recovery_days_remaining, interval_days
FROM items
WHERE id = ?`, itemID).Scan(&intro, &stage, &recoveryDays, &interval); err != nil {
		t.Fatal(err)
	}
	if intro != introRepetitions || stage != 0 || recoveryDays != recoveryPracticeDays || interval != 0 {
		t.Fatalf(
			"expected reset problem item, got intro=%d stage=%d recovery=%d interval=%v",
			intro, stage, recoveryDays, interval,
		)
	}

	for i := 0; i < introRepetitions; i++ {
		if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "unless", Correct: true}}); err != nil {
			t.Fatal(err)
		}
	}
	if err := app.db.QueryRow(`
SELECT intro_remaining, schedule_stage, recovery_days_remaining, interval_days
FROM items
WHERE id = ?`, itemID).Scan(&intro, &stage, &recoveryDays, &interval); err != nil {
		t.Fatal(err)
	}
	if intro != 0 || stage != 0 || recoveryDays != recoveryPracticeDays || interval != 1 {
		t.Fatalf(
			"expected daily recovery after relearning, got intro=%d stage=%d recovery=%d interval=%v",
			intro, stage, recoveryDays, interval,
		)
	}

	for i := 0; i < recoveryPracticeDays-1; i++ {
		if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "unless", Correct: true}}); err != nil {
			t.Fatal(err)
		}
	}
	if err := app.db.QueryRow(`
SELECT schedule_stage, recovery_days_remaining, interval_days
FROM items
WHERE id = ?`, itemID).Scan(&stage, &recoveryDays, &interval); err != nil {
		t.Fatal(err)
	}
	if stage != 0 || recoveryDays != 1 || interval != 1 {
		t.Fatalf("expected one daily recovery left, got stage=%d recovery=%d interval=%v", stage, recoveryDays, interval)
	}

	if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "unless", Correct: true}}); err != nil {
		t.Fatal(err)
	}
	if err := app.db.QueryRow(`
SELECT schedule_stage, recovery_days_remaining, interval_days
FROM items
WHERE id = ?`, itemID).Scan(&stage, &recoveryDays, &interval); err != nil {
		t.Fatal(err)
	}
	if stage != 1 || recoveryDays != 0 || interval != 3 {
		t.Fatalf("expected ordinary schedule after recovery, got stage=%d recovery=%d interval=%v", stage, recoveryDays, interval)
	}
}

func TestLearningStatsCountsIntroReps(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}

	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'a'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 2; i++ {
		if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "a", Answer: "a", Correct: true}}); err != nil {
			t.Fatal(err)
		}
	}

	stats, err := app.learningStats(ctx, deckID)
	if err != nil {
		t.Fatal(err)
	}
	if stats.Count != 2 || stats.IntroRemaining != 8 {
		t.Fatalf("expected 2 learning items with 8 intro reps left, got %#v", stats)
	}
}

func TestPausedDecksAreExcludedFromScheduledReview(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	activeID, err := app.getOrCreateDeck(ctx, "active", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	pausedID, err := app.getOrCreateDeck(ctx, "paused", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	if _, err := app.ingestGroups(ctx, activeID, []ImportGroup{{Name: "words", Words: []string{"active word"}}}, "test", "active"); err != nil {
		t.Fatal(err)
	}
	if _, err := app.ingestGroups(ctx, pausedID, []ImportGroup{{Name: "words", Words: []string{"paused word"}}}, "test", "paused"); err != nil {
		t.Fatal(err)
	}
	if err := app.setDeckPaused(ctx, pausedID, true); err != nil {
		t.Fatal(err)
	}

	due, err := app.countDue(ctx, 0)
	if err != nil {
		t.Fatal(err)
	}
	if due != 1 {
		t.Fatalf("expected only active deck in global due count, got %d", due)
	}
	pausedDue, err := app.countDue(ctx, pausedID)
	if err != nil {
		t.Fatal(err)
	}
	if pausedDue != 1 {
		t.Fatalf("expected paused deck to retain one due word, got %d", pausedDue)
	}

	learning, err := app.learningStats(ctx, 0)
	if err != nil {
		t.Fatal(err)
	}
	if learning.Count != 1 || learning.IntroRemaining != introRepetitions {
		t.Fatalf("expected global learning stats from active deck only, got %#v", learning)
	}
	pausedLearning, err := app.learningStats(ctx, pausedID)
	if err != nil {
		t.Fatal(err)
	}
	if pausedLearning.Count != 1 || pausedLearning.IntroRemaining != introRepetitions {
		t.Fatalf("expected paused deck learning state to be preserved, got %#v", pausedLearning)
	}

	items, err := app.dueItems(ctx, 0, reviewAllDueLimit)
	if err != nil {
		t.Fatal(err)
	}
	if len(items) != 1 || items[0].Text != "active word" {
		t.Fatalf("expected only active due item, got %#v", items)
	}
	pausedItems, err := app.dueItems(ctx, pausedID, reviewAllDueLimit)
	if err != nil {
		t.Fatal(err)
	}
	if len(pausedItems) != 0 {
		t.Fatalf("expected no scheduled review items for paused deck, got %#v", pausedItems)
	}
	var pausedItemID int64
	if err := app.db.QueryRow(`SELECT i.id FROM items i JOIN groups g ON g.id = i.group_id WHERE g.deck_id = ?`, pausedID).Scan(&pausedItemID); err != nil {
		t.Fatal(err)
	}
	selected, err := app.reviewItemsByID(ctx, []int64{pausedItemID})
	if err != nil {
		t.Fatal(err)
	}
	if len(selected) != 0 {
		t.Fatalf("expected crafted paused selection to be filtered, got %#v", selected)
	}
	result := ReviewResult{ItemID: pausedItemID, Prompt: "paused word", Answer: "paused word", Correct: true}
	if err := app.applyScheduledReviewBatch(ctx, []ReviewResult{result}); !errors.Is(err, ErrReviewItemUnavailable) {
		t.Fatalf("expected paused scheduled review to be rejected transactionally, got %v", err)
	}
	var reviewCount int
	if err := app.db.QueryRow(`SELECT review_count FROM items WHERE id = ?`, pausedItemID).Scan(&reviewCount); err != nil {
		t.Fatal(err)
	}
	if reviewCount != 0 {
		t.Fatalf("expected rejected paused review not to mutate scheduling, got review_count %d", reviewCount)
	}
}

func TestDueItemsHonorsLimit(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "big", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	var words []string
	for i := 0; i < reviewAllDueLimit+5; i++ {
		words = append(words, fmt.Sprintf("word %03d", i))
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: words}}, "test", "many")
	if err != nil {
		t.Fatal(err)
	}
	items, err := app.dueItems(ctx, 0, reviewAllDueLimit)
	if err != nil {
		t.Fatal(err)
	}
	if len(items) != reviewAllDueLimit {
		t.Fatalf("expected %d due items, got %d", reviewAllDueLimit, len(items))
	}
}

func TestReviewSubmitContinuesWhenDueItemsRemain(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"put"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'put'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("mode", "review")
	form.Set("return", fmt.Sprintf("/deck?id=%d", deckID))
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Add("session_index", "0")
	form.Set("item_id_0", fmt.Sprint(itemID))
	form.Set("prompt_0", "put")
	form.Set("answer_0", "put")
	form.Set("result_0", "correct")
	req := httptest.NewRequest(http.MethodPost, "/session/submit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleSessionSubmit(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected continued review session, got %d", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "<h1>Review</h1>") || !strings.Contains(body, `name="deck_id"`) {
		t.Fatalf("expected next review session for deck, got body %q", body)
	}
}

func TestSelectedReviewContinuesWithinSelectedSubset(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"put", "other"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var selectedID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'put'`).Scan(&selectedID); err != nil {
		t.Fatal(err)
	}

	startForm := url.Values{}
	startForm.Set("deck_id", fmt.Sprint(deckID))
	startForm.Set("mode", "review")
	startForm.Set("session_order", "listed")
	startForm.Add("item_id", fmt.Sprint(selectedID))
	startReq := httptest.NewRequest(http.MethodPost, "/session/start", strings.NewReader(startForm.Encode()))
	startReq.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	startRec := httptest.NewRecorder()
	app.handleSessionStart(startRec, startReq)
	if startRec.Code != http.StatusOK {
		t.Fatalf("expected selected review session, got %d with body %q", startRec.Code, startRec.Body.String())
	}
	startBody := startRec.Body.String()
	if !strings.Contains(startBody, `name="review_selected" value="1"`) || strings.Contains(startBody, `value="other"`) {
		t.Fatalf("expected selected review scope containing only put, got %q", startBody)
	}

	submitForm := url.Values{}
	submitForm.Set("mode", "review")
	submitForm.Set("return", fmt.Sprintf("/deck?id=%d", deckID))
	submitForm.Set("deck_id", fmt.Sprint(deckID))
	submitForm.Set("session_order", "listed")
	submitForm.Set("review_selected", "1")
	submitForm.Add("session_index", "0")
	submitForm.Set("item_id_0", fmt.Sprint(selectedID))
	submitForm.Set("prompt_0", "put")
	submitForm.Set("answer_0", "put")
	submitForm.Set("result_0", "correct")
	submitReq := httptest.NewRequest(http.MethodPost, "/session/submit", strings.NewReader(submitForm.Encode()))
	submitReq.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	submitRec := httptest.NewRecorder()
	app.handleSessionSubmit(submitRec, submitReq)
	if submitRec.Code != http.StatusOK {
		t.Fatalf("expected selected subset to continue, got %d with body %q", submitRec.Code, submitRec.Body.String())
	}
	submitBody := submitRec.Body.String()
	if !strings.Contains(submitBody, `value="put"`) || strings.Contains(submitBody, `value="other"`) {
		t.Fatalf("expected continued review to stay within selected subset, got %q", submitBody)
	}
	if !strings.Contains(submitBody, `name="review_selected" value="1"`) {
		t.Fatalf("expected continued review to retain selected scope, got %q", submitBody)
	}
}

func TestReviewSubmitRedirectsWhenNoDueItemsRemain(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"put"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'put'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}
	if _, err := app.db.Exec(`UPDATE items SET intro_remaining = 1 WHERE id = ?`, itemID); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("mode", "review")
	form.Set("return", fmt.Sprintf("/deck?id=%d", deckID))
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Add("session_index", "0")
	form.Set("item_id_0", fmt.Sprint(itemID))
	form.Set("prompt_0", "put")
	form.Set("answer_0", "put")
	form.Set("result_0", "correct")
	req := httptest.NewRequest(http.MethodPost, "/session/submit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleSessionSubmit(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected redirect after final due item, got %d with body %q", rec.Code, rec.Body.String())
	}
	if location := rec.Header().Get("Location"); !strings.HasPrefix(location, fmt.Sprintf("/deck?id=%d", deckID)) {
		t.Fatalf("unexpected redirect location %q", location)
	}
}

func TestPracticeSubmitResetsEachMissedItemOnce(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}

	itemIDs := map[string]int64{}
	rows, err := app.db.Query(`SELECT id, text FROM items`)
	if err != nil {
		t.Fatal(err)
	}
	for rows.Next() {
		var id int64
		var text string
		if err := rows.Scan(&id, &text); err != nil {
			rows.Close()
			t.Fatal(err)
		}
		itemIDs[text] = id
	}
	if err := rows.Close(); err != nil {
		t.Fatal(err)
	}
	if _, err := app.db.Exec(`
UPDATE items
SET intro_remaining = 0,
	schedule_stage = 3,
	interval_days = 14,
	due_at = ?,
	review_count = 4,
	correct_count = 3,
	incorrect_count = 1`, formatDBTime(time.Now().UTC().AddDate(0, 0, 30))); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("mode", "practice")
	form.Set("return", fmt.Sprintf("/deck?id=%d", deckID))
	form.Add("session_index", "0")
	form.Set("item_id_0", fmt.Sprint(itemIDs["a"]))
	form.Set("prompt_0", "a")
	form.Set("answer_0", "a")
	form.Set("result_0", "missed")
	form.Add("session_index", "1")
	form.Set("item_id_1", fmt.Sprint(itemIDs["a"]))
	form.Set("prompt_1", "a")
	form.Set("answer_1", "a")
	form.Set("result_1", "missed")
	form.Add("session_index", "2")
	form.Set("item_id_2", fmt.Sprint(itemIDs["the"]))
	form.Set("prompt_2", "the")
	form.Set("answer_2", "the")
	form.Set("result_2", "correct")
	req := httptest.NewRequest(http.MethodPost, "/session/submit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleSessionSubmit(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected redirect after practice, got %d with body %q", rec.Code, rec.Body.String())
	}
	if location := rec.Header().Get("Location"); !strings.Contains(location, "missed+words+reset") {
		t.Fatalf("expected reset notice, got redirect %q", location)
	}

	assertSchedule := func(
		text string,
		wantIntro int,
		wantStage int,
		wantRecovery int,
		wantInterval float64,
		wantReviews int,
		wantCorrect int,
		wantIncorrect int,
	) {
		t.Helper()
		var intro, stage, recovery, reviews, correct, incorrect int
		var interval float64
		if err := app.db.QueryRow(`
SELECT intro_remaining, schedule_stage, recovery_days_remaining, interval_days, review_count, correct_count, incorrect_count
FROM items
WHERE id = ?`, itemIDs[text]).Scan(&intro, &stage, &recovery, &interval, &reviews, &correct, &incorrect); err != nil {
			t.Fatal(err)
		}
		if intro != wantIntro || stage != wantStage || recovery != wantRecovery || interval != wantInterval ||
			reviews != wantReviews || correct != wantCorrect || incorrect != wantIncorrect {
			t.Fatalf(
				"%s schedule: got intro=%d stage=%d recovery=%d interval=%v reviews=%d correct=%d incorrect=%d",
				text, intro, stage, recovery, interval, reviews, correct, incorrect,
			)
		}
	}
	assertSchedule("a", introRepetitions, 0, recoveryPracticeDays, 0, 5, 3, 2)
	assertSchedule("the", 0, 3, 0, 14, 4, 3, 1)

	var missedReviews int
	if err := app.db.QueryRow(`SELECT COUNT(*) FROM reviews WHERE item_id = ? AND correct = 0`, itemIDs["a"]).Scan(&missedReviews); err != nil {
		t.Fatal(err)
	}
	if missedReviews != 1 {
		t.Fatalf("expected one deduplicated missed review, got %d", missedReviews)
	}
	var correctPracticeReviews int
	if err := app.db.QueryRow(`SELECT COUNT(*) FROM reviews WHERE item_id = ?`, itemIDs["the"]).Scan(&correctPracticeReviews); err != nil {
		t.Fatal(err)
	}
	if correctPracticeReviews != 0 {
		t.Fatalf("correct practice should not create reviews, got %d", correctPracticeReviews)
	}
}

func TestHintRouteUsesConfiguredDictionaryStack(t *testing.T) {
	dir := t.TempDir()
	firstDict := filepath.Join(dir, "first.json")
	secondDict := filepath.Join(dir, "second.json")
	configPath := filepath.Join(dir, "stoin-config.json")
	if err := os.WriteFile(firstDict, []byte(`{"A": "alpha", "PW": "first only"}`), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(secondDict, []byte(`{"A": "beta", "PW": "alpha", "SKWR": "alpha"}`), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(configPath, []byte(fmt.Sprintf(`{"dictionaries": [%q, %q]}`, firstDict, secondDict)), 0o644); err != nil {
		t.Fatal(err)
	}

	app, err := NewAppWithOptions(filepath.Join(dir, "srs.sqlite3"), "../../phrasing.json", configPath, "")
	if err != nil {
		t.Fatal(err)
	}
	defer app.db.Close()

	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"alpha"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'alpha'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}

	mux := http.NewServeMux()
	app.routes(mux)
	req := httptest.NewRequest(http.MethodGet, fmt.Sprintf("/hint?item_id=%d", itemID), nil)
	rec := httptest.NewRecorder()
	mux.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected hint response, got %d with body %q", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	if !strings.Contains(body, `"found":true`) || !strings.Contains(body, `"PW"`) {
		t.Fatalf("expected alpha hint from later dictionary override, got %q", body)
	}
	if strings.Contains(body, `"A"`) {
		t.Fatalf("outline A should have been overridden to beta, got %q", body)
	}
	if !strings.Contains(body, `"source":"dictionary"`) {
		t.Fatalf("expected dictionary hint source, got %q", body)
	}
}

func TestHintRoutePrefersRunningStoinPhraseIndex(t *testing.T) {
	dir := t.TempDir()
	dictionaryPath := filepath.Join(dir, "fallback.json")
	configPath := filepath.Join(dir, "stoin-config.json")
	hintIndexPath := filepath.Join(dir, "runtime-hints.json")
	if err := os.WriteFile(dictionaryPath, []byte(`{"A": "is he"}`), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(configPath, []byte(fmt.Sprintf(`{"dictionaries": [%q]}`, dictionaryPath)), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(
		hintIndexPath,
		[]byte(`{"version":1,"hints":{"is he":{"outline":"SKPORPB","source":"initial_verb"}}}`),
		0o644,
	); err != nil {
		t.Fatal(err)
	}

	app, err := NewAppWithOptions(
		filepath.Join(dir, "srs.sqlite3"),
		"../../phrasing.json",
		configPath,
		hintIndexPath,
	)
	if err != nil {
		t.Fatal(err)
	}
	defer app.db.Close()

	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "phrases", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"is he"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'is he'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}

	mux := http.NewServeMux()
	app.routes(mux)
	lookup := func() string {
		req := httptest.NewRequest(http.MethodGet, fmt.Sprintf("/hint?item_id=%d", itemID), nil)
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, req)
		if rec.Code != http.StatusOK {
			t.Fatalf("expected hint response, got %d with body %q", rec.Code, rec.Body.String())
		}
		return rec.Body.String()
	}

	body := lookup()
	if !strings.Contains(body, `"outline":"SKPORPB"`) ||
		!strings.Contains(body, `"source":"initial_verb"`) ||
		strings.Contains(body, `"outline":"A"`) {
		t.Fatalf("expected running Stoin phrase hint to replace config fallback, got %q", body)
	}
	if err := os.WriteFile(
		hintIndexPath,
		[]byte(`{"version":1,"hints":{"is he":{"outline":"TWR-RPB","source":"final_verb"}}}`),
		0o644,
	); err != nil {
		t.Fatal(err)
	}
	body = lookup()
	if !strings.Contains(body, `"outline":"TWR-RPB"`) ||
		!strings.Contains(body, `"source":"final_verb"`) {
		t.Fatalf("expected refreshed running Stoin phrase hint, got %q", body)
	}

	if err := os.Remove(hintIndexPath); err != nil {
		t.Fatal(err)
	}
	body = lookup()
	if !strings.Contains(body, `"outline":"A"`) ||
		!strings.Contains(body, `"source":"dictionary"`) {
		t.Fatalf("expected config fallback after Stoin index disappeared, got %q", body)
	}
}

func TestSessionPageIncludesHintControls(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"put"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	items, err := app.itemsForDeck(ctx, deckID)
	if err != nil {
		t.Fatal(err)
	}
	rec := httptest.NewRecorder()
	app.renderSession(rec, SessionPageData{
		Mode:       "practice",
		DeckID:     deckID,
		ReturnURL:  fmt.Sprintf("/deck?id=%d", deckID),
		Items:      items,
		IsPractice: true,
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("expected session page, got %d", rec.Code)
	}
	body := rec.Body.String()
	for _, want := range []string{
		`class="session-hint-button"`,
		`class="session-hint"`,
		`id="practice-missed-summary" hidden`,
		`id="practice-missed-list"`,
		`id="copy-practice-missed"`,
		`>Hinted or skipped items<`,
		`name="session_order" value="group_random"`,
		">Hint<",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected session body to contain %q, got %q", want, body)
		}
	}
}

func TestReviewSessionOmitsPracticeMissedSummary(t *testing.T) {
	app := testApp(t)
	rec := httptest.NewRecorder()
	app.renderSession(rec, SessionPageData{
		Mode:      "review",
		ReturnURL: "/",
		Items:     []SessionItem{{ID: 1, Text: "put"}},
		IsReview:  true,
	})
	if rec.Code != http.StatusOK {
		t.Fatalf("expected session page, got %d", rec.Code)
	}
	if body := rec.Body.String(); strings.Contains(body, `id="practice-missed-summary"`) {
		t.Fatalf("review session should omit practice summary, got %q", body)
	}
}

func TestDeckPageIncludesSessionOrderOptions(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}

	req := httptest.NewRequest(http.MethodGet, fmt.Sprintf("/deck?id=%d", deckID), nil)
	rec := httptest.NewRecorder()
	app.handleDeck(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected deck page, got %d with body %q", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	for _, want := range []string{
		`name="session_order"`,
		`value="group_random" selected`,
		`value="total_random"`,
		`value="listed"`,
		`type="submit" name="mode" value="review_all">Review all</button>`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected deck body to contain %q, got %q", want, body)
		}
	}
}

func TestChunkSessionLinesKeepsShortWordsTogether(t *testing.T) {
	items := []SessionItem{
		{Text: "a"},
		{Text: "the"},
		{Text: "it"},
		{Text: "and"},
		{Text: "longer phrase"},
	}

	lines := chunkSessionLines(items, len("a the it and"))
	if len(lines) != 2 {
		t.Fatalf("expected 2 lines, got %#v", lines)
	}
	if lines[0].StartIndex != 0 || lines[0].EndIndex != 3 || len(lines[0].Items) != 4 {
		t.Fatalf("expected first line to contain a/the/it/and, got %#v", lines[0])
	}
	if lines[1].StartIndex != 4 || lines[1].EndIndex != 4 || len(lines[1].Items) != 1 {
		t.Fatalf("expected second line to contain the long phrase, got %#v", lines[1])
	}
}

func TestOrderSessionItemsShufflesWithinGroupsByDefault(t *testing.T) {
	items := []SessionItem{
		{Text: "a1", DeckName: "deck", GroupName: "alpha"},
		{Text: "a2", DeckName: "deck", GroupName: "alpha"},
		{Text: "b1", DeckName: "deck", GroupName: "beta"},
		{Text: "b2", DeckName: "deck", GroupName: "beta"},
	}
	reverse := func(items []SessionItem) {
		for i, j := 0, len(items)-1; i < j; i, j = i+1, j-1 {
			items[i], items[j] = items[j], items[i]
		}
	}

	ordered := orderSessionItems(items, sessionOrderGroupRandom, reverse)
	got := []string{ordered[0].Text, ordered[1].Text, ordered[2].Text, ordered[3].Text}
	want := []string{"a2", "a1", "b2", "b1"}
	if strings.Join(got, ",") != strings.Join(want, ",") {
		t.Fatalf("expected group-preserving shuffle %v, got %v", want, got)
	}

	total := orderSessionItems(items, sessionOrderTotalRandom, reverse)
	got = []string{total[0].Text, total[1].Text, total[2].Text, total[3].Text}
	want = []string{"b2", "b1", "a2", "a1"}
	if strings.Join(got, ",") != strings.Join(want, ",") {
		t.Fatalf("expected total shuffle %v, got %v", want, got)
	}

	listed := orderSessionItems(items, sessionOrderListed, reverse)
	got = []string{listed[0].Text, listed[1].Text, listed[2].Text, listed[3].Text}
	want = []string{"a1", "a2", "b1", "b2"}
	if strings.Join(got, ",") != strings.Join(want, ",") {
		t.Fatalf("expected listed order %v, got %v", want, got)
	}
}

func TestPracticeAllStartsDeckSessionWithoutSelection(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Set("mode", "practice_all")
	form.Set("practice_count", "2")
	req := httptest.NewRequest(http.MethodPost, "/session/start", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleSessionStart(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected ok, got %d with body %q", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	if !strings.Contains(body, "<h1>Practice</h1>") {
		t.Fatalf("expected practice session, got body %q", body)
	}
	if got := strings.Count(body, `name="session_index"`); got != 4 {
		t.Fatalf("expected 4 repeated session items, got %d", got)
	}
}

func TestReviewAllStartsDeckSessionWithoutSelection(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	otherDeckID, err := app.getOrCreateDeck(ctx, "other deck", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, otherDeckID, []ImportGroup{{Name: "words", Words: []string{"other"}}}, "test", "two")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := app.db.Exec(
		`UPDATE items
		 SET intro_remaining = 0, due_at = ?
		 WHERE group_id IN (SELECT id FROM groups WHERE deck_id = ?) AND text = 'the'`,
		formatDBTime(time.Now().UTC().AddDate(0, 0, 30)),
		deckID,
	); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Set("mode", "review_all")
	form.Set("session_order", "listed")
	req := httptest.NewRequest(http.MethodPost, "/session/start", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleSessionStart(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected ok, got %d with body %q", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	for _, want := range []string{
		"<h1>Review</h1>",
		`name="mode" value="review"`,
		fmt.Sprintf(`name="deck_id" value="%d"`, deckID),
		`value="a"`,
		`value="the"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected review session body to contain %q, got %q", want, body)
		}
	}
	if strings.Contains(body, `value="other"`) {
		t.Fatalf("review all should not include another deck's items, got body %q", body)
	}
	if strings.Contains(body, `name="review_selected"`) {
		t.Fatalf("review all should not be restricted to its first round, got body %q", body)
	}
	if got := strings.Count(body, `name="session_index"`); got != 2 {
		t.Fatalf("expected 2 review items, including the non-due item, got %d", got)
	}
}

func TestDeckEditHandlerRenamesPausesAndValidatesName(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	if _, err := app.getOrCreateDeck(ctx, "existing deck", time.Now().UTC()); err != nil {
		t.Fatal(err)
	}

	post := func(id int64, name string, paused bool) *httptest.ResponseRecorder {
		t.Helper()
		form := url.Values{
			"deck_id": {fmt.Sprint(id)},
			"name":    {name},
		}
		if paused {
			form.Set("paused", "1")
		}
		req := httptest.NewRequest(http.MethodPost, "/deck/edit", strings.NewReader(form.Encode()))
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		rec := httptest.NewRecorder()
		app.handleDeckEdit(rec, req)
		return rec
	}

	if rec := post(deckID, "  renamed briefs  ", true); rec.Code != http.StatusSeeOther {
		t.Fatalf("expected pause redirect, got %d with body %q", rec.Code, rec.Body.String())
	}
	deck, err := app.deckByID(ctx, deckID)
	if err != nil {
		t.Fatal(err)
	}
	if !deck.Paused {
		t.Fatal("expected deck to be paused")
	}
	if deck.Name != "renamed briefs" {
		t.Fatalf("expected trimmed renamed deck, got %q", deck.Name)
	}
	if rec := post(deckID, "renamed briefs", false); rec.Code != http.StatusSeeOther {
		t.Fatalf("expected resume redirect, got %d with body %q", rec.Code, rec.Body.String())
	}
	deck, err = app.deckByID(ctx, deckID)
	if err != nil {
		t.Fatal(err)
	}
	if deck.Paused {
		t.Fatal("expected deck to be resumed")
	}

	for _, invalidName := range []string{"   ", "existing deck"} {
		rec := post(deckID, invalidName, true)
		if rec.Code != http.StatusSeeOther || !strings.Contains(rec.Header().Get("Location"), "deck_error=") {
			t.Fatalf("expected invalid name %q to reopen deck edit with an error, got %d location %q", invalidName, rec.Code, rec.Header().Get("Location"))
		}
	}
	deck, err = app.deckByID(ctx, deckID)
	if err != nil {
		t.Fatal(err)
	}
	if deck.Name != "renamed briefs" || deck.Paused {
		t.Fatalf("invalid edits changed deck to %#v", deck)
	}

	if rec := post(deckID+999, "missing", true); rec.Code != http.StatusNotFound {
		t.Fatalf("expected unknown deck edit to return 404, got %d", rec.Code)
	}
}

func TestPausedDeckBlocksReviewButAllowsPractice(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "paused briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	if _, err := app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one"); err != nil {
		t.Fatal(err)
	}
	if err := app.setDeckPaused(ctx, deckID, true); err != nil {
		t.Fatal(err)
	}

	start := func(mode string) *httptest.ResponseRecorder {
		t.Helper()
		form := url.Values{}
		form.Set("deck_id", fmt.Sprint(deckID))
		form.Set("mode", mode)
		form.Set("practice_count", "1")
		req := httptest.NewRequest(http.MethodPost, "/session/start", strings.NewReader(form.Encode()))
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		rec := httptest.NewRecorder()
		app.handleSessionStart(rec, req)
		return rec
	}

	if rec := start("review_all"); rec.Code != http.StatusSeeOther || !strings.Contains(rec.Header().Get("Location"), "paused") {
		t.Fatalf("expected paused review redirect, got %d location %q", rec.Code, rec.Header().Get("Location"))
	}
	practice := start("practice_all")
	if practice.Code != http.StatusOK {
		t.Fatalf("expected paused-deck practice, got %d with body %q", practice.Code, practice.Body.String())
	}
	if body := practice.Body.String(); !strings.Contains(body, "<h1>Practice</h1>") || !strings.Contains(body, `value="a"`) {
		t.Fatalf("expected practice session for paused deck, got %q", body)
	}
}

func TestIndexAndDeckPagesPresentPausedDeckControls(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	activeID, err := app.getOrCreateDeck(ctx, "Alpha active", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	pausedID, err := app.getOrCreateDeck(ctx, "Zulu paused", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	if _, err := app.ingestGroups(ctx, activeID, []ImportGroup{{Name: "words", Words: []string{"active"}}}, "test", "active"); err != nil {
		t.Fatal(err)
	}
	if _, err := app.ingestGroups(ctx, pausedID, []ImportGroup{{Name: "words", Words: []string{"paused"}}}, "test", "paused"); err != nil {
		t.Fatal(err)
	}
	if err := app.setDeckPaused(ctx, pausedID, true); err != nil {
		t.Fatal(err)
	}

	indexReq := httptest.NewRequest(http.MethodGet, "/", nil)
	indexRec := httptest.NewRecorder()
	app.handleIndex(indexRec, indexReq)
	if indexRec.Code != http.StatusOK {
		t.Fatalf("expected index page, got %d", indexRec.Code)
	}
	indexBody := indexRec.Body.String()
	activeAt := strings.Index(indexBody, "Alpha active")
	pausedSectionAt := strings.Index(indexBody, `class="paused-decks"`)
	pausedAt := strings.Index(indexBody, "Zulu paused")
	if activeAt < 0 || pausedSectionAt < 0 || pausedAt < pausedSectionAt || activeAt > pausedSectionAt {
		t.Fatalf("expected active deck before separate paused section, got %q", indexBody)
	}
	for _, want := range []string{`class="paused-deck"`, "practice only", "Zulu paused (paused)"} {
		if !strings.Contains(indexBody, want) {
			t.Fatalf("expected index body to contain %q, got %q", want, indexBody)
		}
	}

	deckReq := httptest.NewRequest(http.MethodGet, fmt.Sprintf("/deck?id=%d", pausedID), nil)
	deckRec := httptest.NewRecorder()
	app.handleDeck(deckRec, deckReq)
	if deckRec.Code != http.StatusOK {
		t.Fatalf("expected paused deck page, got %d", deckRec.Code)
	}
	deckBody := deckRec.Body.String()
	for _, want := range []string{
		`deck-status deck-status-paused`,
		`value="review" disabled`,
		`value="review_all" disabled`,
		`value="practice_all">Practice all`,
		"due when resumed",
		"edit_deck=1",
	} {
		if !strings.Contains(deckBody, want) {
			t.Fatalf("expected paused deck body to contain %q, got %q", want, deckBody)
		}
	}

	editReq := httptest.NewRequest(http.MethodGet, fmt.Sprintf("/deck?id=%d&edit_deck=1&deck_error=Rename+failed", pausedID), nil)
	editRec := httptest.NewRecorder()
	app.handleDeck(editRec, editReq)
	if editRec.Code != http.StatusOK {
		t.Fatalf("expected deck edit page, got %d", editRec.Code)
	}
	if body := editRec.Body.String(); !strings.Contains(body, `action="/deck/edit"`) ||
		!strings.Contains(body, `name="name" value="Zulu paused"`) ||
		!strings.Contains(body, `name="paused" value="1" checked`) ||
		!strings.Contains(body, "Rename failed") {
		t.Fatalf("expected checked paused toggle in deck edit form, got %q", body)
	}
}

func TestEditItemUpdatesTextAndRejectsDuplicates(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}

	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'a'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Set("item_id", fmt.Sprint(itemID))
	form.Set("text", "an")
	req := httptest.NewRequest(http.MethodPost, "/item/edit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleItemEdit(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected redirect after edit, got %d with body %q", rec.Code, rec.Body.String())
	}
	if location := rec.Header().Get("Location"); !strings.HasPrefix(location, fmt.Sprintf("/deck?id=%d", deckID)) {
		t.Fatalf("unexpected edit redirect %q", location)
	}
	var text string
	if err := app.db.QueryRow(`SELECT text FROM items WHERE id = ?`, itemID).Scan(&text); err != nil {
		t.Fatal(err)
	}
	if text != "an" {
		t.Fatalf("expected edited text, got %q", text)
	}

	form.Set("text", "the")
	req = httptest.NewRequest(http.MethodPost, "/item/edit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec = httptest.NewRecorder()

	app.handleItemEdit(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected redirect after duplicate edit, got %d with body %q", rec.Code, rec.Body.String())
	}
	location := rec.Header().Get("Location")
	if !strings.Contains(location, "edit_item_id=") || !strings.Contains(location, "item_error=") {
		t.Fatalf("expected duplicate edit redirect to reopen row with error, got %q", location)
	}
	if err := app.db.QueryRow(`SELECT text FROM items WHERE id = ?`, itemID).Scan(&text); err != nil {
		t.Fatal(err)
	}
	if text != "an" {
		t.Fatalf("duplicate edit should not change text, got %q", text)
	}

	otherDeckID, err := app.getOrCreateDeck(ctx, "other deck", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	if _, err := app.ingestGroups(
		ctx,
		otherDeckID,
		[]ImportGroup{{Name: "words", Words: []string{"outside"}}},
		"test",
		"other"); err != nil {
		t.Fatal(err)
	}
	form.Set("text", "outside")
	req = httptest.NewRequest(http.MethodPost, "/item/edit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec = httptest.NewRecorder()

	app.handleItemEdit(rec, req)
	if rec.Code != http.StatusSeeOther || !strings.Contains(rec.Header().Get("Location"), "item_error=") {
		t.Fatalf("expected cross-deck duplicate edit error, got %d location %q", rec.Code, rec.Header().Get("Location"))
	}
	if err := app.db.QueryRow(`SELECT text FROM items WHERE id = ?`, itemID).Scan(&text); err != nil {
		t.Fatal(err)
	}
	if text != "an" {
		t.Fatalf("cross-deck duplicate edit should not change text, got %q", text)
	}
}

func TestEditItemRejectsEmptyText(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'a'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Set("item_id", fmt.Sprint(itemID))
	form.Set("text", "   ")
	req := httptest.NewRequest(http.MethodPost, "/item/edit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleItemEdit(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected redirect after empty edit, got %d", rec.Code)
	}
	if location := rec.Header().Get("Location"); !strings.Contains(location, "item_error=") {
		t.Fatalf("expected empty edit redirect with error, got %q", location)
	}
	var text string
	if err := app.db.QueryRow(`SELECT text FROM items WHERE id = ?`, itemID).Scan(&text); err != nil {
		t.Fatal(err)
	}
	if text != "a" {
		t.Fatalf("empty edit should not change text, got %q", text)
	}
}

func TestDeleteItemRemovesItFromDeck(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"a", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	var itemID int64
	if err := app.db.QueryRow(`SELECT id FROM items WHERE text = 'a'`).Scan(&itemID); err != nil {
		t.Fatal(err)
	}

	form := url.Values{}
	form.Set("deck_id", fmt.Sprint(deckID))
	form.Set("item_id", fmt.Sprint(itemID))
	req := httptest.NewRequest(http.MethodPost, "/item/delete", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleItemDelete(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected redirect after delete, got %d with body %q", rec.Code, rec.Body.String())
	}
	var count int
	if err := app.db.QueryRow(`SELECT COUNT(*) FROM items WHERE text = 'a'`).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 0 {
		t.Fatalf("expected deleted item to be gone, got count %d", count)
	}
}

func TestParseSubmittedResultsUsesSessionRows(t *testing.T) {
	form := url.Values{}
	form.Add("session_index", "0")
	form.Add("session_index", "1")
	form.Set("item_id_0", "7")
	form.Set("prompt_0", "the")
	form.Set("answer_0", "the")
	form.Set("result_0", "correct")
	form.Set("item_id_1", "7")
	form.Set("prompt_1", "the")
	form.Set("answer_1", "")
	form.Set("result_1", "missed")
	req := httptest.NewRequest(http.MethodPost, "/session/submit", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	if err := req.ParseForm(); err != nil {
		t.Fatal(err)
	}

	results, err := parseSubmittedResults(req)
	if err != nil {
		t.Fatal(err)
	}
	if len(results) != 2 {
		t.Fatalf("expected 2 results, got %#v", results)
	}
	if results[0].ItemID != 7 || results[0].Prompt != "the" || results[0].Answer != "the" || !results[0].Correct {
		t.Fatalf("unexpected first result: %#v", results[0])
	}
	if results[1].ItemID != 7 || results[1].Prompt != "the" || results[1].Answer != "" || results[1].Correct {
		t.Fatalf("unexpected second result: %#v", results[1])
	}
}

func TestInvalidImportDoesNotCreateDeck(t *testing.T) {
	app := testApp(t)
	form := url.Values{}
	form.Set("deck_name", "new deck")
	form.Set("content", "word before header\n")
	req := httptest.NewRequest(http.MethodPost, "/import", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleImport(rec, req)
	if rec.Code != http.StatusBadRequest {
		t.Fatalf("expected bad request, got %d", rec.Code)
	}
	var count int
	if err := app.db.QueryRow(`SELECT COUNT(*) FROM decks`).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 0 {
		t.Fatalf("expected no decks after invalid import, got %d", count)
	}
}

func TestNewDeckImportDropsDuplicateEntries(t *testing.T) {
	app := testApp(t)
	form := url.Values{}
	form.Set("deck_name", "new deck")
	form.Set("content", "first:\nalpha\nbeta\n\nsecond:\nalpha\ngamma\nalpha\n")
	req := httptest.NewRequest(http.MethodPost, "/import", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()

	app.handleImport(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("expected successful import redirect, got %d with body %q", rec.Code, rec.Body.String())
	}
	location, err := url.Parse(rec.Header().Get("Location"))
	if err != nil {
		t.Fatal(err)
	}
	if notice := location.Query().Get("notice"); !strings.Contains(notice, "Removed 2 duplicate item(s).") {
		t.Fatalf("expected duplicate removal count in notice, got %q", notice)
	}

	rows, err := app.db.Query(`
SELECT g.name, i.text
FROM items i
JOIN groups g ON g.id = i.group_id
ORDER BY i.id`)
	if err != nil {
		t.Fatal(err)
	}
	defer rows.Close()
	var got []string
	for rows.Next() {
		var group string
		var text string
		if err := rows.Scan(&group, &text); err != nil {
			t.Fatal(err)
		}
		got = append(got, group+":"+text)
	}
	if err := rows.Err(); err != nil {
		t.Fatal(err)
	}
	want := []string{"first:alpha", "first:beta", "second:gamma"}
	if len(got) != len(want) {
		t.Fatalf("expected %v, got %v", want, got)
	}
	for index := range want {
		if got[index] != want[index] {
			t.Fatalf("entry %d: want %q, got %q", index, want[index], got[index])
		}
	}
}

func TestBackupRouteDumpsRestorableSQL(t *testing.T) {
	app := testApp(t)
	ctx := context.Background()
	deckID, err := app.getOrCreateDeck(ctx, "briefs", time.Now().UTC())
	if err != nil {
		t.Fatal(err)
	}
	_, err = app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "words", Words: []string{"can't", "the"}}}, "test", "one")
	if err != nil {
		t.Fatal(err)
	}
	if err := app.setDeckPaused(ctx, deckID, true); err != nil {
		t.Fatal(err)
	}

	mux := http.NewServeMux()
	app.routes(mux)
	req := httptest.NewRequest(http.MethodGet, "/backup", nil)
	rec := httptest.NewRecorder()

	mux.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected backup, got %d with body %q", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	for _, want := range []string{
		"CREATE TABLE decks",
		`INSERT INTO "decks"`,
		`INSERT INTO "items"`,
		`'can''t'`,
		`"paused"`,
		"COMMIT;",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected backup to contain %q, got %q", want, body)
		}
	}

	restoreDB, err := sql.Open("sqlite", t.TempDir()+"/restore.sqlite3")
	if err != nil {
		t.Fatal(err)
	}
	defer restoreDB.Close()
	if _, err := restoreDB.Exec(body); err != nil {
		t.Fatalf("backup did not restore: %v\n%s", err, body)
	}
	var restoredCount int
	if err := restoreDB.QueryRow(`SELECT COUNT(*) FROM items`).Scan(&restoredCount); err != nil {
		t.Fatal(err)
	}
	if restoredCount != 2 {
		t.Fatalf("expected restored backup to contain 2 items, got %d", restoredCount)
	}
	var restoredPaused int
	if err := restoreDB.QueryRow(`SELECT paused FROM decks WHERE id = ?`, deckID).Scan(&restoredPaused); err != nil {
		t.Fatal(err)
	}
	if restoredPaused != 1 {
		t.Fatalf("expected restored backup to preserve paused deck, got %d", restoredPaused)
	}
}

func TestPhrasingTrainerPage(t *testing.T) {
	app := testApp(t)
	req := httptest.NewRequest(http.MethodGet, "/phrasing", nil)
	rec := httptest.NewRecorder()

	app.handlePhrasingTrainer(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected phrasing trainer page, got %d", rec.Code)
	}
	body := rec.Body.String()
	for _, want := range []string{
		"<h1>Phrasing Trainer</h1>",
		`src="/static/phrasing-trainer.js"`,
		"Repetitions",
		"Prompt source",
		"pasted phrase list",
		"Phrases, one per line",
		`id="phrase-pasted-list"`,
		"selected bank order",
		"Search banks",
		"phrase-show-outlines",
		"Select none",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected body to contain %q, got %q", want, body)
		}
	}
}

func TestStaticPhrasingTrainerScript(t *testing.T) {
	app := testApp(t)
	mux := http.NewServeMux()
	app.routes(mux)
	req := httptest.NewRequest(http.MethodGet, "/static/phrasing-trainer.js", nil)
	rec := httptest.NewRecorder()

	mux.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected static trainer script, got %d", rec.Code)
	}
	body := rec.Body.String()
	if !strings.Contains(body, "new URL('/phrasing-data.json'") ||
		!strings.Contains(body, "dataURL.searchParams.set('_', Date.now() + '-' + Math.random())") ||
		!strings.Contains(body, "fetch(dataURL") ||
		!strings.Contains(body, "cache: 'no-store'") ||
		!strings.Contains(body, "'Cache-Control': 'no-cache'") ||
		!strings.Contains(body, "validatePhraseData") ||
		!strings.Contains(body, "initial_verbs") ||
		!strings.Contains(body, "final_verbs") ||
		!strings.Contains(body, "nonverbs") ||
		!strings.Contains(body, "generateFinalVerbPrompts") ||
		!strings.Contains(body, "generateNonverbPrompts") ||
		!strings.Contains(body, "pastedPhrasePool") ||
		!strings.Contains(body, "allPhrasingPromptLookup") ||
		!strings.Contains(body, "phraseSourceSelect") ||
		!strings.Contains(body, "phrasePastedList") ||
		!strings.Contains(body, "Not found in the phrase system") ||
		!strings.Contains(body, "combineStrokeParts(['U', starter.stroke") ||
		!strings.Contains(body, "combineStrokeParts(['E', prefix.stroke") ||
		!strings.Contains(body, "tail.text || 'no tail'") ||
		!strings.Contains(body, "initialFormLabel") ||
		!strings.Contains(body, "case '*E': return 'to-infinitive") ||
		!strings.Contains(body, "case 'AE': return 'base form") ||
		!strings.Contains(body, "case 'A*E': return 'plural present of be") ||
		!strings.Contains(body, "third-person singular present (he/she/it goes)") ||
		!strings.Contains(body, "modal could + base form (could go)") ||
		!strings.Contains(body, "modal auxiliary (can / could)") ||
		!strings.Contains(body, "progressive (a form of be + present participle: is going)") ||
		!strings.Contains(body, "perfect (a form of have + past participle: has gone)") ||
		!strings.Contains(body, "perfect progressive (a form of have + been + present participle: has been going)") ||
		!strings.Contains(body, "auxiliary only (main-verb slot empty: do not / can / be / have / have been)") ||
		!strings.Contains(body, "past-form selection") ||
		!strings.Contains(body, "starter.label") ||
		!strings.Contains(body, "combineStrokeParts") ||
		!strings.Contains(body, "phraseFilterInput") ||
		!strings.Contains(body, "phraseShowOutlines") ||
		!strings.Contains(body, "phraseStorageKey") ||
		!strings.Contains(body, "localStorage") ||
		!strings.Contains(body, "restorePhraseSettings") ||
		!strings.Contains(body, "savePhraseSettings") ||
		!strings.Contains(body, "repeatedShuffledPasses") ||
		!strings.Contains(body, "for (let j = 0; j < pass.length; j++)") ||
		!strings.Contains(body, "repeatedPromptBlocks") ||
		!strings.Contains(body, "normalizedPromptPhrase") {
		t.Fatalf("expected trainer script contents, got %q", rec.Body.String())
	}
	if strings.Contains(body, "initialVerbStrokeBits") ||
		strings.Contains(body, "uniqueStrings(entry.texts).join(' / ')") ||
		strings.Contains(body, "examples.join(' / ')") {
		t.Fatalf("expected pedal-scoped prompts and compact IV labels, got %q", body)
	}
}

func TestStaticAndJSONResponsesAreNeverCached(t *testing.T) {
	app := testApp(t)
	mux := http.NewServeMux()
	app.routes(mux)

	for _, path := range []string{
		"/phrasing",
		"/static/phrasing-trainer.js",
		"/phrasing-data.json",
	} {
		t.Run(path, func(t *testing.T) {
			req := httptest.NewRequest(http.MethodGet, path, nil)
			req.Header.Set("If-None-Match", `"stale"`)
			req.Header.Set("If-Modified-Since", time.Now().Add(24*time.Hour).Format(http.TimeFormat))
			req.Header.Set("Range", "bytes=0-9")
			rec := httptest.NewRecorder()

			mux.ServeHTTP(rec, req)

			if rec.Code != http.StatusOK {
				t.Fatalf("expected fresh 200 response, got %d", rec.Code)
			}
			if got := rec.Header().Get("Cache-Control"); got != "no-store, no-cache, must-revalidate, max-age=0" {
				t.Fatalf("expected no-store Cache-Control, got %q", got)
			}
			if got := rec.Header().Get("CDN-Cache-Control"); got != "no-store" {
				t.Fatalf("expected no-store CDN-Cache-Control, got %q", got)
			}
			if got := rec.Header().Get("Pragma"); got != "no-cache" {
				t.Fatalf("expected no-cache Pragma, got %q", got)
			}
			if got := rec.Header().Get("Expires"); got != "0" {
				t.Fatalf("expected expired response, got %q", got)
			}
			if got := rec.Header().Get("Content-Range"); got != "" {
				t.Fatalf("expected full response without Content-Range, got %q", got)
			}
		})
	}
}

func TestPhrasingDataIsReadFreshOnEveryRequest(t *testing.T) {
	dir := t.TempDir()
	phrasingPath := filepath.Join(dir, "phrasing.json")
	if err := os.WriteFile(phrasingPath, []byte(`{"version":"first"}`), 0o644); err != nil {
		t.Fatal(err)
	}
	app, err := NewAppWithPhrasing(filepath.Join(dir, "srs.sqlite3"), phrasingPath)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if err := app.db.Close(); err != nil {
			t.Fatal(err)
		}
	})
	mux := http.NewServeMux()
	app.routes(mux)

	readVersion := func() string {
		t.Helper()
		req := httptest.NewRequest(http.MethodGet, "/phrasing-data.json", nil)
		req.Header.Set("If-None-Match", `"stale"`)
		req.Header.Set("If-Modified-Since", time.Now().Add(24*time.Hour).Format(http.TimeFormat))
		rec := httptest.NewRecorder()
		mux.ServeHTTP(rec, req)
		if rec.Code != http.StatusOK {
			t.Fatalf("expected fresh 200 response, got %d", rec.Code)
		}
		return strings.TrimSpace(rec.Body.String())
	}

	if got := readVersion(); got != `{"version":"first"}` {
		t.Fatalf("expected first file contents, got %q", got)
	}
	if err := os.WriteFile(phrasingPath, []byte(`{"version":"second"}`), 0o644); err != nil {
		t.Fatal(err)
	}
	if got := readVersion(); got != `{"version":"second"}` {
		t.Fatalf("expected updated file contents, got %q", got)
	}
}

func TestStaticSessionScriptIncludesHints(t *testing.T) {
	app := testApp(t)
	mux := http.NewServeMux()
	app.routes(mux)
	req := httptest.NewRequest(http.MethodGet, "/static/session.js", nil)
	rec := httptest.NewRecorder()

	mux.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected static session script, got %d", rec.Code)
	}
	body := rec.Body.String()
	for _, want := range []string{
		"session-hint-button",
		"hinted[index]",
		"fetch('/hint?item_id='",
		"Outline: ",
		"Initial verb phrase",
		"Final verb phrase",
		"Non-verb phrase",
		"uniqueMissedItemTexts",
		"seen.has(items[itemIndex].id)",
		"missedTexts.join('\\n')",
		"navigator.clipboard.writeText",
		"document.execCommand('copy')",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected session script to contain %q, got %q", want, body)
		}
	}
}

func TestPhrasingDataRoute(t *testing.T) {
	app := testApp(t)
	mux := http.NewServeMux()
	app.routes(mux)
	req := httptest.NewRequest(http.MethodGet, "/phrasing-data.json", nil)
	rec := httptest.NewRecorder()

	mux.ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("expected phrasing data, got %d with body %q", rec.Code, rec.Body.String())
	}
	body := rec.Body.String()
	for _, want := range []string{
		`"initial_verbs"`,
		`"final_verbs"`,
		`"nonverbs"`,
		`"id": "an"`,
		`"text": "an"`,
		`"id": "at"`,
		`"id": "like"`,
		`"stroke": "-BL"`,
		`"stroke": "PH"`,
		`"stroke": "SWR"`,
		`"stroke": "WH"`,
		`"stroke": "STP"`,
		`"stroke": "KH"`,
		`"stroke": "SKP"`,
		`"stroke": "TPW"`,
		`"stroke": "TPR"`,
		`"stroke": "WR"`,
		`"stroke": "TKP"`,
		`"stroke": "PR"`,
		`"stroke": "SKH"`,
		`"though"`,
		`"stroke": "KHR"`,
		`"stroke": "TKPWH"`,
		`"stroke": "SK"`,
		`"stroke": "TP"`,
		`"stroke": "ST"`,
		`"stroke": "TKW"`,
		`"label": "there (plural)"`,
		`"text": "there"`,
		`"could expect"`,
		`"-PGTS"`,
		`"suffix": "like"`,
		`"present_participle"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected phrasing data to contain %q, got %q", want, body)
		}
	}
}

func contains(s string, sub string) bool {
	return strings.Contains(s, sub)
}
