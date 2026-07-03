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
	a.render(w, "deck", data)
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
	nextItems, err := a.dueItems(r.Context(), deckID, reviewAllDueLimit)
	if err != nil {
		serverError(w, err)
		return
	}
	if len(nextItems) > 0 {
		a.renderSession(w, SessionPageData{
			Mode:      "review",
			DeckID:    deckID,
			ReturnURL: returnURL,
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
