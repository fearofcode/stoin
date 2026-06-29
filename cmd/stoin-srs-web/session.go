package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"html/template"
	"net/http"
	"strconv"
	"unicode/utf8"
)

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
