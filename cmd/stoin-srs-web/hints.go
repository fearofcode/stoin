package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"sort"
	"strings"
	"sync"
	"time"
)

const maxHintOutlines = 4

const (
	hintSourceDictionary  = "dictionary"
	hintSourceInitialVerb = "initial_verb"
	hintSourceFinalVerb   = "final_verb"
	hintSourceNonVerb     = "non_verb"
)

type Hint struct {
	Outline string `json:"outline"`
	Source  string `json:"source"`
}

type DictionaryHints struct {
	configPath    string
	hintIndexPath string
	mu            sync.Mutex
	loaded        bool
	usingRuntime  bool
	sources       []hintSource
	byText        map[string][]Hint
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

type runtimeHintIndex struct {
	Version int             `json:"version"`
	Hints   map[string]Hint `json:"hints"`
}

func NewDictionaryHints(configPath string, hintIndexPath ...string) *DictionaryHints {
	runtimePath := ""
	if len(hintIndexPath) > 0 {
		runtimePath = hintIndexPath[0]
	}
	return &DictionaryHints{configPath: configPath, hintIndexPath: runtimePath}
}

func (h *DictionaryHints) Lookup(ctx context.Context, text string) ([]Hint, error) {
	if strings.TrimSpace(text) == "" || h == nil {
		return nil, nil
	}
	h.mu.Lock()
	defer h.mu.Unlock()

	if err := h.reloadIfChanged(ctx); err != nil {
		return nil, err
	}
	hints := h.byText[text]
	if len(hints) > maxHintOutlines {
		hints = hints[:maxHintOutlines]
	}
	return append([]Hint(nil), hints...), nil
}

func (h *DictionaryHints) reloadIfChanged(ctx context.Context) error {
	if h.hintIndexPath != "" {
		info, err := os.Stat(h.hintIndexPath)
		if err == nil {
			if !h.loaded || !h.usingRuntime || sourceChanged(h.sources, h.hintIndexPath, info) {
				return h.reloadRuntime(ctx, info)
			}
			return nil
		}
		if !errors.Is(err, os.ErrNotExist) {
			return err
		}
		if h.usingRuntime {
			h.loaded = false
			h.sources = nil
			h.byText = nil
		}
	}

	if !h.loaded {
		return h.reloadConfig(ctx)
	}
	changed, err := h.sourcesChanged()
	if err != nil {
		return err
	}
	if changed {
		return h.reloadConfig(ctx)
	}
	return nil
}

func sourceChanged(sources []hintSource, path string, info os.FileInfo) bool {
	if len(sources) != 1 || sources[0].path != path {
		return true
	}
	return !info.ModTime().Equal(sources[0].modTime) || info.Size() != sources[0].size
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

func (h *DictionaryHints) reloadRuntime(ctx context.Context, info os.FileInfo) error {
	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
	}

	data, err := os.ReadFile(h.hintIndexPath)
	if err != nil {
		return err
	}
	var index runtimeHintIndex
	if err := json.Unmarshal(data, &index); err != nil {
		return err
	}
	if index.Version != 1 {
		return fmt.Errorf("unsupported Stoin hint index version %d", index.Version)
	}

	byText := make(map[string][]Hint, len(index.Hints))
	for text, hint := range index.Hints {
		if strings.TrimSpace(text) == "" || strings.TrimSpace(hint.Outline) == "" {
			continue
		}
		if !validHintSource(hint.Source) {
			return fmt.Errorf("Stoin hint for %q has unknown source %q", text, hint.Source)
		}
		byText[text] = []Hint{hint}
	}

	h.sources = []hintSource{{path: h.hintIndexPath, modTime: info.ModTime(), size: info.Size()}}
	h.byText = byText
	h.loaded = true
	h.usingRuntime = true
	return nil
}

func validHintSource(source string) bool {
	switch source {
	case hintSourceDictionary, hintSourceInitialVerb, hintSourceFinalVerb, hintSourceNonVerb:
		return true
	default:
		return false
	}
}

func (h *DictionaryHints) reloadConfig(ctx context.Context) error {
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

	byText := map[string][]Hint{}
	for outline, translation := range byOutline {
		if !translationCanBeHinted(translation) {
			continue
		}
		byText[translation] = append(byText[translation], Hint{
			Outline: outline,
			Source:  hintSourceDictionary,
		})
	}
	for text := range byText {
		sort.Slice(byText[text], func(i, j int) bool {
			left := byText[text][i].Outline
			right := byText[text][j].Outline
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
	h.usingRuntime = false
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
