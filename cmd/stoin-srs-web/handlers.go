package main

import (
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

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
	data.EditDeck = r.URL.Query().Get("edit_deck") == "1"
	data.EditItemID = parseOptionalInt64(r.URL.Query().Get("edit_item_id"))
	data.DeckError = r.URL.Query().Get("deck_error")
	data.ItemError = r.URL.Query().Get("item_error")
	a.render(w, "deck", data)
}

func (a *App) handleDeckEdit(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}
	if err := r.ParseForm(); err != nil {
		http.Error(w, "could not read form", http.StatusBadRequest)
		return
	}
	deckID := parseOptionalInt64(r.FormValue("deck_id"))
	if deckID <= 0 {
		http.Error(w, "invalid deck edit", http.StatusBadRequest)
		return
	}
	name := strings.TrimSpace(r.FormValue("name"))
	if name == "" {
		redirectWithDeckError(w, r, deckID, "Deck name cannot be empty.")
		return
	}
	paused := r.FormValue("paused") != ""
	if err := a.updateDeck(r.Context(), deckID, name, paused); err != nil {
		if errors.Is(err, ErrDuplicateDeck) {
			redirectWithDeckError(w, r, deckID, "A deck with that name already exists.")
			return
		}
		if errors.Is(err, sql.ErrNoRows) {
			http.NotFound(w, r)
			return
		}
		serverError(w, err)
		return
	}
	redirectWithNotice(w, r, deckPath(deckID), "Deck updated.")
}

func (a *App) handleItemEdit(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}
	if err := r.ParseForm(); err != nil {
		http.Error(w, "could not read form", http.StatusBadRequest)
		return
	}
	deckID := parseOptionalInt64(r.FormValue("deck_id"))
	itemID := parseOptionalInt64(r.FormValue("item_id"))
	if deckID <= 0 || itemID <= 0 {
		http.Error(w, "invalid item edit", http.StatusBadRequest)
		return
	}

	text := strings.TrimSpace(r.FormValue("text"))
	if text == "" {
		redirectWithItemError(w, r, deckID, itemID, "Word cannot be empty.")
		return
	}
	if err := a.updateItemText(r.Context(), deckID, itemID, text); err != nil {
		if errors.Is(err, ErrDuplicateItem) {
			redirectWithItemError(w, r, deckID, itemID, "That word is already in this deck.")
			return
		}
		if errors.Is(err, sql.ErrNoRows) {
			http.NotFound(w, r)
			return
		}
		serverError(w, err)
		return
	}
	redirectWithNotice(w, r, deckPath(deckID), "Updated word.")
}

func (a *App) handleItemDelete(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		methodNotAllowed(w)
		return
	}
	if err := r.ParseForm(); err != nil {
		http.Error(w, "could not read form", http.StatusBadRequest)
		return
	}
	deckID := parseOptionalInt64(r.FormValue("deck_id"))
	itemID := parseOptionalInt64(r.FormValue("item_id"))
	if deckID <= 0 || itemID <= 0 {
		http.Error(w, "invalid item delete", http.StatusBadRequest)
		return
	}
	if err := a.deleteItem(r.Context(), deckID, itemID); err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			http.NotFound(w, r)
			return
		}
		serverError(w, err)
		return
	}
	redirectWithNotice(w, r, deckPath(deckID), "Deleted word.")
}

func (a *App) handlePhrasingTrainer(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		methodNotAllowed(w)
		return
	}
	a.render(w, "phrasingTrainer", nil)
}

func (a *App) handlePhrasingData(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		methodNotAllowed(w)
		return
	}
	data, err := os.ReadFile(a.phrasingPath)
	if err != nil {
		http.Error(w, "could not read phrasing data", http.StatusInternalServerError)
		return
	}
	if !json.Valid(data) {
		http.Error(w, "phrasing data is not valid JSON", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_, _ = w.Write(data)
}

func (a *App) handleHint(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		methodNotAllowed(w)
		return
	}
	itemID, err := parseRequiredID(r.URL.Query().Get("item_id"), "item id")
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	text, err := a.itemTextByID(r.Context(), itemID)
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			http.NotFound(w, r)
			return
		}
		serverError(w, err)
		return
	}
	outlines, err := a.hints.Lookup(r.Context(), text)
	if err != nil {
		serverError(w, err)
		return
	}
	response := struct {
		Found    bool     `json:"found"`
		Text     string   `json:"text"`
		Outlines []string `json:"outlines"`
	}{
		Found:    len(outlines) > 0,
		Text:     text,
		Outlines: outlines,
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_ = json.NewEncoder(w).Encode(response)
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
	if rawMode != "practice" && rawMode != "review" && rawMode != "practice_all" && rawMode != "review_all" {
		http.Error(w, "mode must be practice, practice_all, review, or review_all", http.StatusBadRequest)
		return
	}
	mode := rawMode
	allItems := rawMode == "practice_all" || rawMode == "review_all"
	if allItems {
		mode = strings.TrimSuffix(rawMode, "_all")
	}
	order, err := parseSessionOrder(r.FormValue("session_order"))
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	deckID := parseOptionalInt64(r.FormValue("deck_id"))
	returnURL := "/"
	if deckID > 0 {
		returnURL = "/deck?id=" + strconv.FormatInt(deckID, 10)
	}
	if mode == "review" && deckID > 0 {
		deck, err := a.deckByID(r.Context(), deckID)
		if err != nil {
			if errors.Is(err, sql.ErrNoRows) {
				http.NotFound(w, r)
				return
			}
			serverError(w, err)
			return
		}
		if deck.Paused {
			redirectWithNotice(w, r, returnURL, "This deck is paused. Resume it before reviewing.")
			return
		}
	}

	count := 1
	if mode == "practice" {
		count, err = positiveIntForm(r.FormValue("practice_count"), 1)
		if err != nil {
			redirectWithNotice(w, r, returnURL, err.Error())
			return
		}
	}

	var items []SessionItem
	if allItems {
		if deckID <= 0 {
			http.Error(w, strings.ReplaceAll(rawMode, "_", " ")+" requires a deck", http.StatusBadRequest)
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
		if mode == "review" {
			items, err = a.reviewItemsByID(r.Context(), ids)
		} else {
			items, err = a.itemsByID(r.Context(), ids)
		}
	}
	if err != nil {
		serverError(w, err)
		return
	}
	if len(items) == 0 {
		redirectWithNotice(w, r, returnURL, "No words in this deck.")
		return
	}
	items = orderSessionItems(items, order, nil)
	items = repeatSessionItems(items, count)
	a.renderSession(w, SessionPageData{
		Mode:       mode,
		DeckID:     deckID,
		ReturnURL:  returnURL,
		Order:      order,
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

	items, err := a.dueItems(r.Context(), 0, reviewAllDueLimit)
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
	deckID := parseOptionalInt64(r.FormValue("deck_id"))
	returnURL := r.FormValue("return")
	if returnURL == "" {
		returnURL = "/"
	}
	if mode != "practice" && mode != "review" {
		http.Error(w, "mode must be practice or review", http.StatusBadRequest)
		return
	}

	results, err := parseSubmittedResults(r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	if mode == "practice" {
		missed := deduplicatedMissedResults(results)
		if len(missed) > 0 {
			if err := a.applyReviewBatch(r.Context(), missed); err != nil {
				serverError(w, err)
				return
			}
			redirectWithNotice(w, r, returnURL, "Practice complete; missed words reset.")
			return
		}
		redirectWithNotice(w, r, returnURL, "Practice complete.")
		return
	}
	reviewIDs := make([]int64, len(results))
	for i, result := range results {
		reviewIDs[i] = result.ItemID
	}
	activeItems, err := a.reviewItemsByID(r.Context(), reviewIDs)
	if err != nil {
		serverError(w, err)
		return
	}
	if len(activeItems) != len(reviewIDs) {
		http.Error(w, "review contains words from a paused or missing deck", http.StatusBadRequest)
		return
	}

	order, err := parseSessionOrder(r.FormValue("session_order"))
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	if err := a.applyScheduledReviewBatch(r.Context(), results); err != nil {
		if errors.Is(err, ErrReviewItemUnavailable) {
			http.Error(w, "review contains words from a paused or missing deck", http.StatusBadRequest)
			return
		}
		serverError(w, err)
		return
	}
	nextItems, err := a.dueItems(r.Context(), deckID, reviewAllDueLimit)
	if err != nil {
		serverError(w, err)
		return
	}
	if len(nextItems) > 0 {
		nextItems = orderSessionItems(nextItems, order, nil)
		a.renderSession(w, SessionPageData{
			Mode:      "review",
			DeckID:    deckID,
			ReturnURL: returnURL,
			Order:     order,
			Items:     nextItems,
			IsReview:  true,
		})
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
