package main

import (
	"context"
	"encoding/json"
	"errors"
	"os"
	"sort"
	"strings"
	"sync"
	"time"
)

const maxHintOutlines = 4

type DictionaryHints struct {
	configPath string
	mu         sync.Mutex
	loaded     bool
	sources    []hintSource
	byText     map[string][]string
}

type hintSource struct {
	path    string
	modTime time.Time
	size    int64
}

type hintConfig struct {
	Dictionaries []json.RawMessage `json:"dictionaries"`
}

type hintDictionaryObject struct {
	Path    string `json:"path"`
	Enabled *bool  `json:"enabled"`
}

func NewDictionaryHints(configPath string) *DictionaryHints {
	return &DictionaryHints{configPath: configPath}
}

func (h *DictionaryHints) Lookup(ctx context.Context, text string) ([]string, error) {
	if strings.TrimSpace(text) == "" {
		return nil, nil
	}
	if h == nil {
		return nil, nil
	}
	h.mu.Lock()
	defer h.mu.Unlock()

	if err := h.reloadIfChanged(ctx); err != nil {
		return nil, err
	}
	outlines := h.byText[text]
	if len(outlines) > maxHintOutlines {
		outlines = outlines[:maxHintOutlines]
	}
	return append([]string(nil), outlines...), nil
}

func (h *DictionaryHints) reloadIfChanged(ctx context.Context) error {
	if !h.loaded {
		return h.reload(ctx)
	}
	changed, err := h.sourcesChanged()
	if err != nil {
		return err
	}
	if changed {
		return h.reload(ctx)
	}
	return nil
}

func (h *DictionaryHints) sourcesChanged() (bool, error) {
	if h.configPath != "" {
		configTracked := false
		for _, source := range h.sources {
			if source.path == h.configPath {
				configTracked = true
				break
			}
		}
		if !configTracked {
			if _, err := os.Stat(h.configPath); err == nil {
				return true, nil
			}
		}
	}
	for _, source := range h.sources {
		info, err := os.Stat(source.path)
		if err != nil {
			return true, nil
		}
		if !info.ModTime().Equal(source.modTime) || info.Size() != source.size {
			return true, nil
		}
	}
	return false, nil
}

func (h *DictionaryHints) reload(ctx context.Context) error {
	dictionaries, sources, err := hintDictionariesFromConfig(h.configPath)
	if err != nil {
		return err
	}
	if len(dictionaries) == 0 {
		dictionaries = []string{"lapwing-base.json"}
	}

	byOutline := map[string]string{}
	for _, path := range dictionaries {
		select {
		case <-ctx.Done():
			return ctx.Err()
		default:
		}
		info, err := os.Stat(path)
		if err != nil {
			return err
		}
		sources = append(sources, hintSource{
			path:    path,
			modTime: info.ModTime(),
			size:    info.Size(),
		})
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		var entries map[string]string
		if err := json.Unmarshal(data, &entries); err != nil {
			return err
		}
		for outline, translation := range entries {
			if strings.TrimSpace(outline) == "" || translation == "" {
				continue
			}
			byOutline[outline] = translation
		}
	}

	byText := map[string][]string{}
	for outline, translation := range byOutline {
		if !translationCanBeHinted(translation) {
			continue
		}
		byText[translation] = append(byText[translation], outline)
	}
	for text := range byText {
		sort.Slice(byText[text], func(i, j int) bool {
			left := byText[text][i]
			right := byText[text][j]
			leftStrokes := strings.Count(left, "/")
			rightStrokes := strings.Count(right, "/")
			if leftStrokes != rightStrokes {
				return leftStrokes < rightStrokes
			}
			if len(left) != len(right) {
				return len(left) < len(right)
			}
			return left < right
		})
	}

	h.sources = sources
	h.byText = byText
	h.loaded = true
	return nil
}

func hintDictionariesFromConfig(path string) ([]string, []hintSource, error) {
	info, err := os.Stat(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil, nil, nil
		}
		return nil, nil, err
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, nil, err
	}
	var config hintConfig
	if err := json.Unmarshal(data, &config); err != nil {
		return nil, nil, err
	}
	sources := []hintSource{{
		path:    path,
		modTime: info.ModTime(),
		size:    info.Size(),
	}}
	dictionaries := make([]string, 0, len(config.Dictionaries))
	for _, raw := range config.Dictionaries {
		var path string
		if err := json.Unmarshal(raw, &path); err == nil {
			if strings.TrimSpace(path) != "" {
				dictionaries = append(dictionaries, path)
			}
			continue
		}
		var object hintDictionaryObject
		if err := json.Unmarshal(raw, &object); err != nil {
			return nil, nil, err
		}
		if object.Enabled != nil && !*object.Enabled {
			continue
		}
		if strings.TrimSpace(object.Path) != "" {
			dictionaries = append(dictionaries, object.Path)
		}
	}
	return dictionaries, sources, nil
}

func translationCanBeHinted(translation string) bool {
	if strings.TrimSpace(translation) == "" {
		return false
	}
	return !strings.ContainsAny(translation, "{}=")
}
