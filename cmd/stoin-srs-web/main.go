package main

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"html/template"
	"log"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"

	_ "modernc.org/sqlite"
)

const (
	defaultDBPath       = "stoin-srs-web.sqlite3"
	defaultAddr         = "127.0.0.1:8080"
	introRepetitions    = 5
	reviewAllDueLimit   = 100
	sessionLineMaxRunes = 52
	maxRequestBodyBytes = 4 << 20
)

var scheduleDays = []int{1, 3, 7, 14, 30, 60, 120, 240}

type App struct {
	db        *sql.DB
	templates *template.Template
}

type Deck struct {
	ID        int64
	Name      string
	CreatedAt string
}

type Group struct {
	ID        int64
	DeckID    int64
	Name      string
	CreatedAt string
	Items     []Item
}

type Item struct {
	ID             int64
	GroupID        int64
	DeckName       string
	GroupName      string
	Text           string
	IntroRemaining int
	ScheduleStage  int
	IntervalDays   float64
	DueAt          time.Time
	ReviewCount    int
	CorrectCount   int
	IncorrectCount int
}

type ImportGroup struct {
	Name  string
	Line  int
	Words []string
}

type ParseIssue struct {
	Line    int
	Message string
}

type ImportStats struct {
	Groups          int
	ItemsRead       int
	Added           int
	Existing        int
	DuplicateImport bool
}

type IndexPageData struct {
	Decks    []Deck
	Errors   []ParseIssue
	Notice   string
	Form     ImportFormData
	DueLimit int
	DueCount int
}

type ImportFormData struct {
	DeckID    int64
	DeckName  string
	GroupName string
	Content   string
}

type DeckPageData struct {
	Deck       Deck
	Groups     []Group
	Errors     []ParseIssue
	Notice     string
	Form       ImportFormData
	TotalItems int
	DueCount   int
}

type SessionItem struct {
	ID        int64  `json:"id"`
	Text      string `json:"text"`
	DeckName  string `json:"deckName"`
	GroupName string `json:"groupName"`
}

type SessionLineItem struct {
	Index int
	Item  SessionItem
}

type SessionLine struct {
	Index      int
	StartIndex int
	EndIndex   int
	Items      []SessionLineItem
}

type SessionPageData struct {
	Mode       string
	DeckID     int64
	ReturnURL  string
	Items      []SessionItem
	Lines      []SessionLine
	ItemsJSON  template.JS
	IsReview   bool
	IsPractice bool
}

func main() {
	dbPath := flag.String("db", defaultDBPath, "SQLite database path")
	addr := flag.String("addr", defaultAddr, "HTTP listen address")
	flag.Parse()

	app, err := NewApp(*dbPath)
	if err != nil {
		log.Fatal(err)
	}
	defer app.db.Close()

	mux := http.NewServeMux()
	app.routes(mux)

	log.Printf("stoin-srs-web listening on http://%s", *addr)
	log.Fatal(http.ListenAndServe(*addr, mux))
}

func NewApp(dbPath string) (*App, error) {
	db, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)

	app := &App{
		db:        db,
		templates: template.Must(template.New("pages").Funcs(template.FuncMap{"dict": templateDict}).Parse(pageTemplates)),
	}
	if err := app.initSchema(context.Background()); err != nil {
		db.Close()
		return nil, err
	}
	return app, nil
}

func (a *App) routes(mux *http.ServeMux) {
	mux.HandleFunc("/", a.handleIndex)
	mux.HandleFunc("/deck", a.handleDeck)
	mux.HandleFunc("/import", a.handleImport)
	mux.HandleFunc("/session/start", a.handleSessionStart)
	mux.HandleFunc("/session/review-all-due", a.handleReviewAllDue)
	mux.HandleFunc("/session/submit", a.handleSessionSubmit)
}

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

func (a *App) handleIndex(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	if r.Method != http.MethodGet {
		methodNotAllowed(w)
		return
	}

	data, err := a.indexData(r.Context(), nil, importFormFromQuery(r.URL.Query()))
	if err != nil {
		serverError(w, err)
		return
	}
	data.Notice = r.URL.Query().Get("notice")
	a.render(w, "index", data)
}

func (a *App) handleDeck(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		methodNotAllowed(w)
		return
	}
	deckID, err := parseRequiredID(r.URL.Query().Get("id"), "deck id")
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	data, err := a.deckData(r.Context(), deckID, nil, importFormFromQuery(r.URL.Query()))
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			http.NotFound(w, r)
			return
		}
		serverError(w, err)
		return
	}
	data.Notice = r.URL.Query().Get("notice")
	a.render(w, "deck", data)
}

func (a *App) handleImport(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}
	r.Body = http.MaxBytesReader(w, r.Body, maxRequestBodyBytes)
	if err := r.ParseForm(); err != nil {
		http.Error(w, "could not read form", http.StatusBadRequest)
		return
	}

	form := ImportFormData{
		DeckID:    parseOptionalInt64(r.FormValue("deck_id")),
		DeckName:  strings.TrimSpace(r.FormValue("deck_name")),
		GroupName: strings.TrimSpace(r.FormValue("group_name")),
		Content:   r.FormValue("content"),
	}
	returnPath := r.FormValue("return")
	if returnPath == "" {
		returnPath = "/"
	}

	deckID, issues, err := a.validateImportDeck(r.Context(), form)
	if err != nil {
		serverError(w, err)
		return
	}
	if len(issues) == 0 && strings.TrimSpace(form.Content) == "" {
		issues = append(issues, ParseIssue{Line: 1, Message: "import text is empty"})
	}

	var groups []ImportGroup
	if len(issues) == 0 {
		groups, issues = parseImportText(form.Content, form.GroupName)
	}
	if len(issues) > 0 {
		a.renderImportErrors(w, r, returnPath, deckID, issues, form)
		return
	}

	if form.DeckName != "" {
		deckID, err = a.getOrCreateDeck(r.Context(), form.DeckName, time.Now().UTC())
		if err != nil {
			serverError(w, err)
			return
		}
	}

	stats, err := a.ingestGroups(r.Context(), deckID, groups, "web form", importHash(form.Content, form.GroupName))
	if err != nil {
		serverError(w, err)
		return
	}

	var notice string
	if stats.DuplicateImport {
		notice = "That exact import was already processed for this deck."
	} else {
		notice = fmt.Sprintf("Imported %d item(s); %d already present.", stats.Added, stats.Existing)
	}
	redirectWithNotice(w, r, returnPath, notice)
}

func (a *App) handleSessionStart(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}
	if err := r.ParseForm(); err != nil {
		http.Error(w, "could not read form", http.StatusBadRequest)
		return
	}

	rawMode := r.FormValue("mode")
	if rawMode != "practice" && rawMode != "review" && rawMode != "practice_all" {
		http.Error(w, "mode must be practice, practice_all, or review", http.StatusBadRequest)
		return
	}
	mode := rawMode
	practiceAll := rawMode == "practice_all"
	if practiceAll {
		mode = "practice"
	}

	deckID := parseOptionalInt64(r.FormValue("deck_id"))
	returnURL := "/"
	if deckID > 0 {
		returnURL = "/deck?id=" + strconv.FormatInt(deckID, 10)
	}

	count := 1
	if mode == "practice" {
		var err error
		count, err = positiveIntForm(r.FormValue("practice_count"), 1)
		if err != nil {
			redirectWithNotice(w, r, returnURL, err.Error())
			return
		}
	}

	var items []SessionItem
	var err error
	if practiceAll {
		if deckID <= 0 {
			http.Error(w, "practice all requires a deck", http.StatusBadRequest)
			return
		}
		items, err = a.itemsForDeck(r.Context(), deckID)
	} else {
		var ids []int64
		ids, err = selectedItemIDs(r)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		if len(ids) == 0 {
			redirectWithNotice(w, r, returnURL, "Select at least one word.")
			return
		}
		items, err = a.itemsByID(r.Context(), ids)
	}
	if err != nil {
		serverError(w, err)
		return
	}
	if len(items) == 0 {
		redirectWithNotice(w, r, returnURL, "No words in this deck.")
		return
	}
	items = repeatSessionItems(items, count)
	a.renderSession(w, SessionPageData{
		Mode:       mode,
		DeckID:     deckID,
		ReturnURL:  returnURL,
		Items:      items,
		IsReview:   mode == "review",
		IsPractice: mode == "practice",
	})
}

func (a *App) handleReviewAllDue(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}

	items, err := a.dueItems(r.Context(), reviewAllDueLimit)
	if err != nil {
		serverError(w, err)
		return
	}
	if len(items) == 0 {
		redirectWithNotice(w, r, "/", "No due words.")
		return
	}
	a.renderSession(w, SessionPageData{
		Mode:      "review",
		ReturnURL: "/",
		Items:     items,
		IsReview:  true,
	})
}

func (a *App) handleSessionSubmit(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}
	if err := r.ParseForm(); err != nil {
		http.Error(w, "could not read form", http.StatusBadRequest)
		return
	}

	mode := r.FormValue("mode")
	returnURL := r.FormValue("return")
	if returnURL == "" {
		returnURL = "/"
	}
	if mode != "review" {
		redirectWithNotice(w, r, returnURL, "Practice complete.")
		return
	}

	results, err := parseSubmittedResults(r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	if err := a.applyReviewBatch(r.Context(), results); err != nil {
		serverError(w, err)
		return
	}
	redirectWithNotice(w, r, returnURL, "Review saved.")
}

func (a *App) renderImportErrors(
	w http.ResponseWriter,
	r *http.Request,
	returnPath string,
	deckID int64,
	issues []ParseIssue,
	form ImportFormData,
) {
	if strings.HasPrefix(returnPath, "/deck") && deckID > 0 {
		data, err := a.deckData(r.Context(), deckID, issues, form)
		if err != nil {
			serverError(w, err)
			return
		}
		w.WriteHeader(http.StatusBadRequest)
		a.render(w, "deck", data)
		return
	}
	data, err := a.indexData(r.Context(), issues, form)
	if err != nil {
		serverError(w, err)
		return
	}
	w.WriteHeader(http.StatusBadRequest)
	a.render(w, "index", data)
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
	return IndexPageData{
		Decks:    decks,
		Errors:   issues,
		Form:     form,
		DueLimit: reviewAllDueLimit,
		DueCount: dueCount,
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
	total := 0
	for _, group := range groups {
		total += len(group.Items)
	}
	form.DeckID = deckID
	return DeckPageData{
		Deck:       deck,
		Groups:     groups,
		Errors:     issues,
		Form:       form,
		TotalItems: total,
		DueCount:   dueCount,
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
			groups[index].Items = append(groups[index].Items, Item{
				ID:             itemID.Int64,
				GroupID:        groupID,
				GroupName:      groupName,
				Text:           itemText.String,
				IntroRemaining: int(intro.Int64),
				ScheduleStage:  int(stage.Int64),
				IntervalDays:   interval.Float64,
				DueAt:          due,
				ReviewCount:    int(reviewCount.Int64),
				CorrectCount:   int(correctCount.Int64),
				IncorrectCount: int(incorrectCount.Int64),
			})
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

func (a *App) dueItems(ctx context.Context, limit int) ([]SessionItem, error) {
	rows, err := a.db.QueryContext(ctx, `
SELECT
	i.id,
	i.text,
	d.name,
	g.name
FROM items i
JOIN groups g ON g.id = i.group_id
JOIN decks d ON d.id = g.deck_id
WHERE i.intro_remaining > 0 OR i.due_at <= ?
ORDER BY i.due_at, i.id
LIMIT ?`, formatDBTime(time.Now().UTC()), limit)
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

type ReviewResult struct {
	ItemID  int64
	Prompt  string
	Answer  string
	Correct bool
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

func parseImportText(content string, groupName string) ([]ImportGroup, []ParseIssue) {
	if strings.TrimSpace(groupName) != "" {
		return parseSimpleImport(content, strings.TrimSpace(groupName))
	}
	return parseGroupedImport(content)
}

func parseSimpleImport(content string, groupName string) ([]ImportGroup, []ParseIssue) {
	var words []string
	seen := map[string]int{}
	var issues []ParseIssue
	for lineNumber, line := range strings.Split(content, "\n") {
		word := strings.TrimSpace(strings.TrimSuffix(line, "\r"))
		if word == "" {
			continue
		}
		if firstLine, ok := seen[word]; ok {
			issues = append(issues, ParseIssue{
				Line:    lineNumber + 1,
				Message: fmt.Sprintf("duplicate word in group %q (first seen on line %d)", groupName, firstLine),
			})
			continue
		}
		seen[word] = lineNumber + 1
		words = append(words, word)
	}
	if len(words) == 0 {
		issues = append(issues, ParseIssue{Line: 1, Message: "import text contains no words"})
	}
	if len(issues) > 0 {
		return nil, issues
	}
	return []ImportGroup{{Name: groupName, Line: 1, Words: words}}, nil
}

func parseGroupedImport(content string) ([]ImportGroup, []ParseIssue) {
	var groups []ImportGroup
	groupLines := map[string]int{}
	var issues []ParseIssue

	current := ImportGroup{}
	currentSeen := map[string]int{}
	currentValid := false
	previousBlank := true

	finishCurrent := func() {
		if current.Name == "" {
			return
		}
		if currentValid {
			if len(current.Words) == 0 {
				issues = append(issues, ParseIssue{
					Line:    current.Line,
					Message: fmt.Sprintf("group %q contains no words", current.Name),
				})
			} else {
				groups = append(groups, current)
			}
		}
		current = ImportGroup{}
		currentSeen = map[string]int{}
		currentValid = false
	}

	lines := strings.Split(content, "\n")
	for index, raw := range lines {
		lineNumber := index + 1
		stripped := strings.TrimSpace(strings.TrimSuffix(raw, "\r"))
		if stripped == "" {
			previousBlank = true
			continue
		}

		isHeader := strings.HasSuffix(stripped, ":") && previousBlank
		if isHeader {
			finishCurrent()
			name := strings.TrimSpace(strings.TrimSuffix(stripped, ":"))
			current = ImportGroup{Name: name, Line: lineNumber}
			if name == "" {
				issues = append(issues, ParseIssue{Line: lineNumber, Message: "group header is empty"})
			} else if firstLine, ok := groupLines[name]; ok {
				issues = append(issues, ParseIssue{
					Line:    lineNumber,
					Message: fmt.Sprintf("duplicate group %q (first seen on line %d)", name, firstLine),
				})
			} else {
				groupLines[name] = lineNumber
				currentValid = true
			}
			previousBlank = false
			continue
		}

		if current.Name == "" {
			issues = append(issues, ParseIssue{
				Line:    lineNumber,
				Message: "word appears before any group header; add a 'group name:' line or enter a plain-list group name",
			})
			previousBlank = false
			continue
		}
		if currentValid {
			if firstLine, ok := currentSeen[stripped]; ok {
				issues = append(issues, ParseIssue{
					Line:    lineNumber,
					Message: fmt.Sprintf("duplicate word in group %q (first seen on line %d)", current.Name, firstLine),
				})
			} else {
				currentSeen[stripped] = lineNumber
				current.Words = append(current.Words, stripped)
			}
		}
		previousBlank = false
	}
	finishCurrent()

	if len(groups) == 0 && len(issues) == 0 {
		issues = append(issues, ParseIssue{Line: 1, Message: "import text contains no groups"})
	}
	if len(issues) > 0 {
		return nil, issues
	}
	return groups, nil
}

func importHash(content string, groupName string) string {
	hash := sha256.Sum256([]byte(groupName + "\x00" + content))
	return hex.EncodeToString(hash[:])
}

func selectedItemIDs(r *http.Request) ([]int64, error) {
	var ids []int64
	seen := map[int64]bool{}
	for _, raw := range r.Form["item_id"] {
		id, err := strconv.ParseInt(raw, 10, 64)
		if err != nil || id <= 0 {
			return nil, fmt.Errorf("invalid item id %q", raw)
		}
		if !seen[id] {
			seen[id] = true
			ids = append(ids, id)
		}
	}
	return ids, nil
}

func repeatSessionItems(items []SessionItem, count int) []SessionItem {
	if count <= 1 || len(items) == 0 {
		return items
	}
	out := make([]SessionItem, 0, len(items)*count)
	for i := 0; i < count; i++ {
		out = append(out, items...)
	}
	return out
}

func chunkSessionLines(items []SessionItem, maxRunes int) []SessionLine {
	if maxRunes <= 0 {
		maxRunes = sessionLineMaxRunes
	}
	lines := []SessionLine{}
	current := SessionLine{}
	currentRunes := 0

	flush := func() {
		if len(current.Items) == 0 {
			return
		}
		current.Index = len(lines)
		lines = append(lines, current)
		current = SessionLine{}
		currentRunes = 0
	}

	for index, item := range items {
		itemRunes := utf8.RuneCountInString(item.Text)
		addedRunes := itemRunes
		if len(current.Items) > 0 {
			addedRunes++
		}
		if len(current.Items) > 0 && currentRunes+addedRunes > maxRunes {
			flush()
		}
		if len(current.Items) == 0 {
			current.StartIndex = index
			current.EndIndex = index
		} else {
			currentRunes++
		}
		current.Items = append(current.Items, SessionLineItem{Index: index, Item: item})
		current.EndIndex = index
		currentRunes += itemRunes
	}
	flush()
	return lines
}

func parseSubmittedResults(r *http.Request) ([]ReviewResult, error) {
	rawIndexes := r.Form["session_index"]
	if len(rawIndexes) == 0 {
		return nil, errors.New("missing session data")
	}
	results := make([]ReviewResult, 0, len(rawIndexes))
	for _, rawIndex := range rawIndexes {
		index, err := strconv.Atoi(rawIndex)
		if err != nil || index < 0 {
			return nil, fmt.Errorf("invalid session index %q", rawIndex)
		}
		suffix := strconv.Itoa(index)
		rawID := r.FormValue("item_id_" + suffix)
		id, err := strconv.ParseInt(rawID, 10, 64)
		if err != nil || id <= 0 {
			return nil, fmt.Errorf("invalid item id for session row %s", suffix)
		}
		status := r.FormValue("result_" + suffix)
		if status == "" {
			return nil, fmt.Errorf("missing result for session row %s", suffix)
		}
		if status != "correct" && status != "missed" {
			return nil, fmt.Errorf("invalid result for session row %s", suffix)
		}
		results = append(results, ReviewResult{
			ItemID:  id,
			Prompt:  r.FormValue("prompt_" + suffix),
			Answer:  r.FormValue("answer_" + suffix),
			Correct: status == "correct",
		})
	}
	return results, nil
}

func (a *App) renderSession(w http.ResponseWriter, data SessionPageData) {
	itemsJSON, err := json.Marshal(data.Items)
	if err != nil {
		serverError(w, err)
		return
	}
	data.Lines = chunkSessionLines(data.Items, sessionLineMaxRunes)
	data.ItemsJSON = template.JS(itemsJSON)
	a.render(w, "session", data)
}

func (a *App) render(w http.ResponseWriter, name string, data any) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := a.templates.ExecuteTemplate(w, name, data); err != nil {
		log.Printf("render %s: %v", name, err)
	}
}

func formatDBTime(t time.Time) string {
	return t.UTC().Truncate(time.Second).Format(time.RFC3339)
}

func parseDBTime(value string) (time.Time, error) {
	return time.Parse(time.RFC3339, value)
}

func boolInt(value bool) int {
	if value {
		return 1
	}
	return 0
}

func parseRequiredID(value string, label string) (int64, error) {
	id, err := strconv.ParseInt(value, 10, 64)
	if err != nil || id <= 0 {
		return 0, fmt.Errorf("invalid %s", label)
	}
	return id, nil
}

func parseOptionalInt64(value string) int64 {
	if value == "" {
		return 0
	}
	id, _ := strconv.ParseInt(value, 10, 64)
	return id
}

func positiveIntForm(value string, defaultValue int) (int, error) {
	if strings.TrimSpace(value) == "" {
		return defaultValue, nil
	}
	parsed, err := strconv.Atoi(value)
	if err != nil || parsed <= 0 {
		return 0, errors.New("practice count must be an integer greater than zero")
	}
	return parsed, nil
}

func importFormFromQuery(query url.Values) ImportFormData {
	return ImportFormData{
		DeckID:    parseOptionalInt64(query.Get("deck_id")),
		DeckName:  query.Get("deck_name"),
		GroupName: query.Get("group_name"),
		Content:   query.Get("content"),
	}
}

func redirectWithNotice(w http.ResponseWriter, r *http.Request, path string, notice string) {
	u, err := url.Parse(path)
	if err != nil || !strings.HasPrefix(path, "/") {
		u = &url.URL{Path: "/"}
	}
	query := u.Query()
	query.Set("notice", notice)
	u.RawQuery = query.Encode()
	http.Redirect(w, r, u.String(), http.StatusSeeOther)
}

func methodNotAllowed(w http.ResponseWriter) {
	http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
}

func serverError(w http.ResponseWriter, err error) {
	log.Printf("server error: %v", err)
	http.Error(w, "internal server error", http.StatusInternalServerError)
}

const pageTemplates = `
{{define "layoutTop"}}
<!doctype html>
<html>
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>Stoin SRS</title>
	<style>
		:root {
			color-scheme: dark;
			--bg: #101214;
			--panel: #171a1f;
			--panel2: #20242b;
			--text: #e7e0d3;
			--muted: #a9a193;
			--line: #343943;
			--accent: #d6a13d;
			--good: #30472f;
			--bad: #533036;
			--focus: #6d8cc7;
		}
		html, body { margin: 0; padding: 0; background: var(--bg); color: var(--text); font: 16px/1.45 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
		main { max-width: 900px; margin: 0 auto; padding: 22px; }
		a { color: var(--accent); }
		h1, h2, h3 { line-height: 1.15; }
		.panel { background: var(--panel); border: 1px solid var(--line); border-radius: 6px; padding: 16px; margin: 16px 0; }
		.group { background: var(--panel); border: 1px solid var(--line); border-radius: 6px; margin: 18px 0; }
		.group header { display: flex; gap: 12px; align-items: center; justify-content: space-between; padding: 12px 14px; border-bottom: 1px solid var(--line); background: var(--panel2); }
		.word-row { display: grid; grid-template-columns: auto 1fr auto; gap: 10px; align-items: center; padding: 8px 14px; border-top: 1px solid rgba(255,255,255,0.04); }
		.word-row:first-of-type { border-top: 0; }
		.word-meta { color: var(--muted); font-size: 0.9em; }
		label { display: inline-flex; align-items: center; gap: 8px; }
		input, select, textarea, button { background: #0e1013; color: var(--text); border: 1px solid var(--line); border-radius: 4px; padding: 8px; font: inherit; }
		textarea { width: 100%; min-height: 180px; box-sizing: border-box; }
		button { cursor: pointer; background: #222832; }
		button.primary { background: #4b3b1f; border-color: #8a6a2a; }
		button:disabled { opacity: 0.45; cursor: default; }
		.actions { display: flex; flex-wrap: wrap; gap: 10px; align-items: center; margin: 14px 0; }
		.notice { border-left: 4px solid var(--accent); padding: 8px 12px; background: #1c1a13; }
		.errors { border-left: 4px solid #c76d6d; padding: 8px 12px; background: #231416; }
		.errors ul { margin: 0; padding-left: 20px; }
		.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
		@media (max-width: 700px) { .grid { grid-template-columns: 1fr; } }
		.session-list { display: grid; gap: 14px; margin: 22px 0; }
		.session-line { border-bottom: 1px solid rgba(255,255,255,0.07); display: grid; gap: 4px; padding: 4px 0 12px; }
		.session-line.current { border-bottom-color: rgba(109,140,199,0.65); }
		.session-target { color: var(--muted); font-size: 1.2em; line-height: 1.7; overflow-wrap: anywhere; }
		.session-token { border-radius: 4px; padding: 0.05em 0.03em; }
		.session-token.past { color: var(--text); }
		.session-token.future { color: var(--muted); }
		.session-token.current { color: #f1d18f; font-weight: 700; }
		.session-token.current.wrong { color: #e8b8b8; }
		.session-token.missed { background: var(--bad); color: var(--text); }
		.session-input-row { align-items: center; display: flex; gap: 10px; min-height: 2.6rem; }
		.session-echo { align-items: center; color: var(--text); display: flex; flex: 1 1 auto; flex-wrap: wrap; font-size: 1.2em; gap: 0 0.35rem; line-height: 1.7; min-height: 2.6rem; min-width: 0; overflow-wrap: anywhere; }
		.typed-word { color: var(--text); }
		.typed-missed { align-items: center; display: inline-flex; height: 1.2em; justify-content: center; width: 1.2em; }
		.typed-missed::before { border: 2px solid #d08a8a; content: ""; display: block; height: 0.55em; transform: rotate(45deg); width: 0.55em; }
		.typing-echo { color: var(--text); min-height: 1.6rem; min-width: 0.6rem; white-space: pre-wrap; }
		.typing-echo.input-error { color: #ffd2d2; }
		.typing-cursor { border-right: 2px solid var(--text); display: inline-block; height: 1.2em; margin-left: 0.08rem; vertical-align: -0.18em; }
		.input-spacer { min-height: 2.6rem; }
		.session-line-input { border: 0; height: 1px; left: -10000px; min-height: 1px; opacity: 0; padding: 0; pointer-events: none; position: fixed; top: 0; width: 1px; }
		.session-skip { flex: 0 0 auto; white-space: nowrap; }
		@media (max-width: 700px) { .session-input-row { align-items: stretch; display: grid; } .session-skip { justify-self: start; } }
		.small { font-size: 0.9em; color: var(--muted); }
	</style>
</head>
<body>
<main>
{{end}}

{{define "layoutBottom"}}
</main>
</body>
</html>
{{end}}

{{define "importForm"}}
{{if .Errors}}
<div class="errors">
	<strong>Import failed</strong>
	<ul>
	{{range .Errors}}<li>line {{.Line}}: {{.Message}}</li>{{end}}
	</ul>
</div>
{{end}}
<form method="post" action="/import" class="panel">
	<input type="hidden" name="return" value="{{.Return}}">
	{{if .DeckID}}<input type="hidden" name="deck_id" value="{{.DeckID}}">{{end}}
	{{if not .DeckID}}
	<div class="grid">
		<label>Existing deck
			<select name="deck_id">
				<option value="">choose...</option>
				{{range .Decks}}<option value="{{.ID}}" {{if eq $.Form.DeckID .ID}}selected{{end}}>{{.Name}}</option>{{end}}
			</select>
		</label>
		<label>New deck name
			<input name="deck_name" value="{{.Form.DeckName}}">
		</label>
	</div>
	{{end}}
	<label>Plain-list group name
		<input name="group_name" value="{{.Form.GroupName}}" placeholder="leave blank for grouped format">
	</label>
	<textarea name="content" spellcheck="false">{{.Form.Content}}</textarea>
	<div class="actions"><button class="primary" type="submit">Import</button></div>
</form>
{{end}}

{{define "index"}}
{{template "layoutTop" .}}
<h1>Stoin SRS</h1>
{{if .Notice}}<p class="notice">{{.Notice}}</p>{{end}}
<div class="panel">
	<h2>Decks</h2>
	{{if .Decks}}
	<ul>
		{{range .Decks}}<li><a href="/deck?id={{.ID}}">{{.Name}}</a></li>{{end}}
	</ul>
	{{else}}
	<p>No decks yet.</p>
	{{end}}
	<form method="post" action="/session/review-all-due">
		<button type="submit">Review all due across all decks</button>
		<span class="small">{{.DueCount}} due, up to {{.DueLimit}} per session</span>
	</form>
</div>
<h2>Import</h2>
{{template "importForm" dict "Errors" .Errors "Form" .Form "Decks" .Decks "DeckID" 0 "Return" "/"}}
{{template "layoutBottom" .}}
{{end}}

{{define "deck"}}
{{template "layoutTop" .}}
<p><a href="/">All decks</a></p>
{{if .Notice}}<p class="notice">{{.Notice}}</p>{{end}}
<h1>{{.Deck.Name}}</h1>
<p class="small">{{.TotalItems}} words, {{.DueCount}} due</p>
<form method="post" action="/session/start" id="selection-form">
	<input type="hidden" name="deck_id" value="{{.Deck.ID}}">
	<div class="actions">
		<button class="primary" type="submit" name="mode" value="review">Review selected</button>
		<label>Practice count <input type="number" name="practice_count" min="1" value="1"></label>
		<button type="submit" name="mode" value="practice">Practice selected</button>
		<button type="submit" name="mode" value="practice_all">Practice all</button>
	</div>
	{{if .Groups}}
	{{range .Groups}}
	<section class="group">
		<header>
			<label><input type="checkbox" class="group-check" data-group="{{.ID}}"> {{.Name}}</label>
			<span class="small">{{len .Items}} words</span>
		</header>
		{{if .Items}}
		{{range .Items}}
		<div class="word-row">
			<input type="checkbox" name="item_id" value="{{.ID}}" data-group="{{.GroupID}}">
			<span>{{.Text}}</span>
			<span class="word-meta">{{if gt .IntroRemaining 0}}learning {{.IntroRemaining}}{{else}}due {{.DueAt.Format "2006-01-02"}}{{end}}</span>
		</div>
		{{end}}
		{{else}}
		<div class="word-row"><span></span><span class="small">No words in this group.</span><span></span></div>
		{{end}}
	</section>
	{{end}}
	{{else}}
	<p>No groups yet.</p>
	{{end}}
</form>
<h2>Import Into {{.Deck.Name}}</h2>
{{template "importForm" dict "Errors" .Errors "Form" .Form "Decks" nil "DeckID" .Deck.ID "Return" (printf "/deck?id=%d" .Deck.ID)}}
<script>
document.querySelectorAll('.group-check').forEach(function(box) {
	box.addEventListener('change', function() {
		document.querySelectorAll('input[name="item_id"][data-group="'+box.dataset.group+'"]').forEach(function(item) {
			item.checked = box.checked;
		});
	});
});
</script>
{{template "layoutBottom" .}}
{{end}}

{{define "session"}}
{{template "layoutTop" .}}
<p><a href="{{.ReturnURL}}">Back</a></p>
<h1>{{if .IsReview}}Review{{else}}Practice{{end}}</h1>
<form method="post" action="/session/submit" id="session-form">
	<input type="hidden" name="mode" value="{{.Mode}}">
	<input type="hidden" name="return" value="{{.ReturnURL}}">
	<div class="session-list" id="word-list">
	{{range $line := .Lines}}
		<div class="session-line" data-line="{{$line.Index}}" data-start="{{$line.StartIndex}}" data-end="{{$line.EndIndex}}">
			<div class="session-target">
			{{range $i, $lineItem := $line.Items}}{{if $i}} {{end}}<span class="session-token future" data-index="{{$lineItem.Index}}">{{$lineItem.Item.Text}}</span>{{end}}
			</div>
			<div class="session-input-row">
				<div class="session-echo" id="line_echo_{{$line.Index}}"></div>
				<button type="button" class="session-skip" data-line="{{$line.Index}}" disabled>Skip</button>
			</div>
			<textarea class="session-line-input" id="typing_line_{{$line.Index}}" spellcheck="false" autocomplete="off" autocapitalize="off" autocorrect="off" disabled></textarea>
			{{range $lineItem := $line.Items}}
			<input type="hidden" name="session_index" value="{{$lineItem.Index}}">
			<input type="hidden" name="item_id_{{$lineItem.Index}}" value="{{$lineItem.Item.ID}}">
			<input type="hidden" name="prompt_{{$lineItem.Index}}" value="{{$lineItem.Item.Text}}">
			<input type="hidden" name="answer_{{$lineItem.Index}}" id="answer_{{$lineItem.Index}}">
			<input type="hidden" name="result_{{$lineItem.Index}}" id="result_{{$lineItem.Index}}">
			{{end}}
		</div>
	{{end}}
	</div>
	<div class="actions">
		<button class="primary" type="submit" id="submit" disabled>Submit</button>
	</div>
</form>
<script>
const items = {{.ItemsJSON}};
let index = 0;
let currentInput = '';
const results = Array(items.length).fill('');
const submit = document.getElementById('submit');
const lines = Array.from(document.querySelectorAll('.session-line'));
const lineInputs = Array.from(document.querySelectorAll('.session-line-input'));
const skipButtons = Array.from(document.querySelectorAll('.session-skip'));
const tokens = Array.from(document.querySelectorAll('.session-token'));
const lineByItem = [];
lines.forEach((line, lineIndex) => {
	const start = Number(line.dataset.start);
	const end = Number(line.dataset.end);
	for (let i = start; i <= end; i++) lineByItem[i] = lineIndex;
	line.addEventListener('click', function() {
		focusCurrentInput(false);
	});
});
function norm(s) { return s.trim(); }
function currentLineIndex() {
	return index < items.length ? lineByItem[index] : -1;
}
function inputPrefixIsOk() {
	if (index >= items.length) return true;
	const typed = norm(currentInput);
	return typed === '' || items[index].text.startsWith(typed);
}
function makeSpan(className, text) {
	const span = document.createElement('span');
	span.className = className;
	if (text !== undefined) span.textContent = text;
	return span;
}
function renderTypingEcho() {
	const visibleInput = currentInput.trimStart();
	const echo = makeSpan(currentInput && !inputPrefixIsOk() ? 'typing-echo input-error' : 'typing-echo', visibleInput);
	echo.appendChild(makeSpan('typing-cursor'));
	return echo;
}
function renderLineEcho(line, lineIndex) {
	const echo = line.querySelector('.session-echo');
	const start = Number(line.dataset.start);
	const end = Number(line.dataset.end);
	const currentLine = currentLineIndex();
	let wrote = false;
	echo.replaceChildren();
	for (let i = start; i <= end && i < index; i++) {
		if (results[i] === 'missed') {
			const marker = makeSpan('typed-missed');
			marker.setAttribute('aria-label', 'skipped');
			echo.appendChild(marker);
		} else {
			echo.appendChild(makeSpan('typed-word', items[i].text));
		}
		wrote = true;
	}
	if (lineIndex === currentLine && index < items.length) {
		echo.appendChild(renderTypingEcho());
		wrote = true;
	}
	if (!wrote) echo.appendChild(makeSpan('input-spacer'));
}
function updateTokenStates() {
	tokens.forEach((token) => {
		const itemIndex = Number(token.dataset.index);
		token.className = 'session-token';
		if (itemIndex < index) {
			token.classList.add(results[itemIndex] === 'missed' ? 'missed' : 'past');
		} else if (itemIndex === index) {
			token.classList.add('current');
			if (currentInput && !inputPrefixIsOk()) token.classList.add('wrong');
		} else {
			token.classList.add('future');
		}
	});
}
function syncControls() {
	const currentLine = currentLineIndex();
	lines.forEach((line, lineIndex) => {
		const current = lineIndex === currentLine;
		line.classList.toggle('current', current);
		lineInputs[lineIndex].disabled = !current;
		skipButtons[lineIndex].disabled = !current;
		if (current) {
			if (lineInputs[lineIndex].value !== currentInput) lineInputs[lineIndex].value = currentInput;
		} else {
			lineInputs[lineIndex].value = '';
		}
	});
	if (index >= items.length) {
		submit.disabled = false;
	}
}
function focusCurrentInput(scroll) {
	if (index >= items.length) return;
	const currentLine = currentLineIndex();
	const input = lineInputs[currentLine];
	if (!input || input.disabled) return;
	if (scroll) lines[currentLine].scrollIntoView({block: 'center'});
	input.focus({preventScroll: true});
	input.setSelectionRange(input.value.length, input.value.length);
}
function renderSession(options) {
	const opts = options || {};
	updateTokenStates();
	lines.forEach(renderLineEcho);
	syncControls();
	if (opts.focus !== false) focusCurrentInput(opts.scroll !== false);
}
function completeCurrent(correct) {
	if (index >= items.length) return;
	const answer = currentInput;
	document.getElementById('answer_' + index).value = answer;
	document.getElementById('result_' + index).value = correct ? 'correct' : 'missed';
	results[index] = correct ? 'correct' : 'missed';
	const oldLine = currentLineIndex();
	if (oldLine >= 0) lineInputs[oldLine].value = '';
	currentInput = '';
	index++;
	renderSession();
}
lineInputs.forEach((input, lineIndex) => {
	input.addEventListener('input', function() {
		if (lineIndex !== currentLineIndex()) return;
		currentInput = input.value;
		if (index < items.length && norm(currentInput) === items[index].text) {
			completeCurrent(true);
		} else {
			renderSession({focus: false, scroll: false});
		}
	});
});
skipButtons.forEach((button, lineIndex) => {
	button.addEventListener('click', function() {
		if (lineIndex === currentLineIndex()) completeCurrent(false);
	});
});
renderSession();
</script>
{{template "layoutBottom" .}}
{{end}}
`

func templateDict(values ...any) (map[string]any, error) {
	if len(values)%2 != 0 {
		return nil, errors.New("dict requires key/value pairs")
	}
	out := map[string]any{}
	for i := 0; i < len(values); i += 2 {
		key, ok := values[i].(string)
		if !ok {
			return nil, errors.New("dict keys must be strings")
		}
		out[key] = values[i+1]
	}
	return out, nil
}
