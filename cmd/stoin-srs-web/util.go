package main

import (
	"errors"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"
)

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
