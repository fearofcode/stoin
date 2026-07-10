package main

import (
	"context"
	"database/sql"
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

	app, err := NewAppWithOptions(filepath.Join(dir, "srs.sqlite3"), "../../phrasing.json", configPath)
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
		`name="session_order" value="group_random"`,
		">Hint<",
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected session body to contain %q, got %q", want, body)
		}
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
	if got := strings.Count(body, `name="session_index"`); got != 2 {
		t.Fatalf("expected 2 review items, including the non-due item, got %d", got)
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
	if !strings.Contains(body, "fetch('/phrasing-data.json'") ||
		!strings.Contains(body, "validatePhraseData") ||
		!strings.Contains(body, "initial_verbs") ||
		!strings.Contains(body, "final_verbs") ||
		!strings.Contains(body, "nonverbs") ||
		!strings.Contains(body, "generateFinalVerbPrompts") ||
		!strings.Contains(body, "combineStrokeParts") ||
		!strings.Contains(body, "phraseFilterInput") ||
		!strings.Contains(body, "phraseShowOutlines") ||
		!strings.Contains(body, "phraseStorageKey") ||
		!strings.Contains(body, "localStorage") ||
		!strings.Contains(body, "restorePhraseSettings") ||
		!strings.Contains(body, "savePhraseSettings") ||
		!strings.Contains(body, "repeatedShuffledPasses") ||
		!strings.Contains(body, "repeatedPromptBlocks") ||
		!strings.Contains(body, "normalizedPromptPhrase") {
		t.Fatalf("expected trainer script contents, got %q", rec.Body.String())
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
		`"KHR"`,
		`"SRAO*E"`,
		`"though"`,
	} {
		if !strings.Contains(body, want) {
			t.Fatalf("expected phrasing data to contain %q, got %q", want, body)
		}
	}
}

func contains(s string, sub string) bool {
	return strings.Contains(s, sub)
}
