package main

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"
	"time"
)

func testApp(t *testing.T) *App {
	t.Helper()
	app, err := NewApp(t.TempDir() + "/srs.sqlite3")
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

func TestIngestGroupsSkipsDuplicateWordsAcrossDeck(t *testing.T) {
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
	stats, err := app.ingestGroups(ctx, deckID, []ImportGroup{{Name: "briefs", Words: []string{"a", "the"}}}, "test", "two")
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

	for i := 0; i < introRepetitions; i++ {
		if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "unless", Correct: true}}); err != nil {
			t.Fatal(err)
		}
	}
	var intro int
	var interval float64
	if err := app.db.QueryRow(`SELECT intro_remaining, interval_days FROM items WHERE id = ?`, itemID).Scan(&intro, &interval); err != nil {
		t.Fatal(err)
	}
	if intro != 0 || interval != 1 {
		t.Fatalf("expected learned item at 1 day interval, got intro=%d interval=%v", intro, interval)
	}

	if err := app.applyReviewBatch(ctx, []ReviewResult{{ItemID: itemID, Prompt: "unless", Answer: "", Correct: false}}); err != nil {
		t.Fatal(err)
	}
	if err := app.db.QueryRow(`SELECT intro_remaining, interval_days FROM items WHERE id = ?`, itemID).Scan(&intro, &interval); err != nil {
		t.Fatal(err)
	}
	if intro != introRepetitions || interval != 0 {
		t.Fatalf("expected reset item, got intro=%d interval=%v", intro, interval)
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

func contains(s string, sub string) bool {
	return strings.Contains(s, sub)
}
