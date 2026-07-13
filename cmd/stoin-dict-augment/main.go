package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type stenoKey uint8

const (
	keyNumber stenoKey = iota
	keyLeftS
	keyLeftT
	keyLeftK
	keyLeftP
	keyLeftW
	keyLeftH
	keyLeftR
	keyA
	keyO
	keyStar
	keyE
	keyU
	keyRightF
	keyRightR
	keyRightP
	keyRightB
	keyRightL
	keyRightG
	keyRightT
	keyRightS
	keyRightD
	keyRightZ
)

func bit(key stenoKey) uint32 {
	return uint32(1) << key
}

const (
	regionLeft = iota
	regionVowels
	regionRight
)

type sourceEntry struct {
	outline []uint32
	value   string
}

type candidateClaim struct {
	outline  []uint32
	value    string
	conflict bool
}

type trieNode struct {
	children map[uint32]*trieNode
}

type outlineTrie struct {
	root *trieNode
}

func newOutlineTrie() *outlineTrie {
	return &outlineTrie{root: &trieNode{children: make(map[uint32]*trieNode)}}
}

func (t *outlineTrie) insert(outline []uint32) {
	node := t.root
	for _, stroke := range outline {
		child := node.children[stroke]
		if child == nil {
			child = &trieNode{children: make(map[uint32]*trieNode)}
			node.children[stroke] = child
		}
		node = child
	}
}

func (t *outlineTrie) hasPrefix(outline []uint32) bool {
	node := t.root
	for _, stroke := range outline {
		node = node.children[stroke]
		if node == nil {
			return false
		}
	}
	return true
}

func main() {
	var outputPath string
	flag.StringVar(&outputPath, "output", "", "path for the generated augmentation dictionary")
	flag.StringVar(&outputPath, "o", "", "path for the generated augmentation dictionary (shorthand)")
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s -output <augmentations.json> <source.json> [source.json ...]\n", filepath.Base(os.Args[0]))
		flag.PrintDefaults()
	}
	flag.Parse()

	if outputPath == "" || flag.NArg() == 0 {
		flag.Usage()
		os.Exit(2)
	}

	if err := run(outputPath, flag.Args(), os.Stdout, os.Stderr); err != nil {
		fmt.Fprintf(os.Stderr, "stoin-dict-augment: %v\n", err)
		os.Exit(1)
	}
}

func run(outputPath string, inputPaths []string, stdout, stderr io.Writer) error {
	if err := ensureDistinctOutput(outputPath, inputPaths); err != nil {
		return err
	}

	sources, invalidCount, err := loadSources(inputPaths)
	if err != nil {
		return err
	}
	if invalidCount != 0 {
		fmt.Fprintf(stderr, "Skipped %d source entries whose outlines Stoin cannot parse.\n", invalidCount)
	}

	claims := generateCandidateClaims(sources)
	augmentations, ambiguousCount, boundaryCount := selectSafeCandidates(sources, claims)
	if err := writeDictionary(outputPath, augmentations); err != nil {
		return err
	}

	fmt.Fprintf(
		stdout,
		"Read %d canonical source entries; wrote %d augmentations to %s (%d ambiguous and %d boundary-conflicting candidates skipped).\n",
		len(sources),
		len(augmentations),
		outputPath,
		ambiguousCount,
		boundaryCount,
	)
	return nil
}

func ensureDistinctOutput(outputPath string, inputPaths []string) error {
	outputAbsolute, err := filepath.Abs(outputPath)
	if err != nil {
		return fmt.Errorf("resolve output path: %w", err)
	}
	for _, inputPath := range inputPaths {
		inputAbsolute, err := filepath.Abs(inputPath)
		if err != nil {
			return fmt.Errorf("resolve input path %q: %w", inputPath, err)
		}
		if inputAbsolute == outputAbsolute {
			return fmt.Errorf("output path must differ from input path %q", inputPath)
		}
	}
	return nil
}

func loadSources(paths []string) (map[string]sourceEntry, int, error) {
	sources := make(map[string]sourceEntry)
	invalidCount := 0

	for _, path := range paths {
		contents, err := os.ReadFile(path)
		if err != nil {
			return nil, 0, fmt.Errorf("read %q: %w", path, err)
		}

		var dictionary map[string]string
		if err := json.Unmarshal(contents, &dictionary); err != nil {
			return nil, 0, fmt.Errorf("parse %q as a string-to-string JSON object: %w", path, err)
		}
		if dictionary == nil {
			return nil, 0, fmt.Errorf("parse %q: dictionary must be a JSON object", path)
		}

		// JSON objects are unordered. Sorting makes canonical alias collisions
		// deterministic within each file; later input files still take precedence.
		keys := make([]string, 0, len(dictionary))
		for outline := range dictionary {
			keys = append(keys, outline)
		}
		sort.Strings(keys)

		for _, rawOutline := range keys {
			outline, ok := parseOutline(rawOutline)
			if !ok {
				invalidCount++
				continue
			}
			canonical := formatOutline(outline)
			sources[canonical] = sourceEntry{outline: outline, value: dictionary[rawOutline]}
		}
	}

	return sources, invalidCount, nil
}

func generateCandidateClaims(sources map[string]sourceEntry) map[string]*candidateClaim {
	claims := make(map[string]*candidateClaim)
	keys := sortedKeys(sources)

	for _, sourceKey := range keys {
		entry := sources[sourceKey]
		states := map[string][]uint32{sourceKey: entry.outline}
		queue := [][]uint32{entry.outline}
		addCandidate := func(candidate []uint32) {
			candidateKey := formatOutline(candidate)
			if _, seen := states[candidateKey]; seen {
				return
			}
			states[candidateKey] = candidate
			queue = append(queue, candidate)

			if _, exists := sources[candidateKey]; exists {
				return
			}
			claim := claims[candidateKey]
			if claim == nil {
				claims[candidateKey] = &candidateClaim{
					outline: append([]uint32(nil), candidate...),
					value:   entry.value,
				}
			} else if claim.value != entry.value {
				claim.conflict = true
			}
		}

		for len(queue) != 0 {
			outline := queue[0]
			queue = queue[1:]

			for boundary := 0; boundary+1 < len(outline); boundary++ {
				merged, ok := mergeSuffixStroke(outline[boundary], outline[boundary+1])
				if !ok {
					continue
				}

				candidate := make([]uint32, 0, len(outline)-1)
				candidate = append(candidate, outline[:boundary]...)
				candidate = append(candidate, merged)
				candidate = append(candidate, outline[boundary+2:]...)
				addCandidate(candidate)
			}

			for strokeIndex := 1; strokeIndex+1 < len(outline); strokeIndex++ {
				if outline[strokeIndex] != aouStroke() {
					continue
				}
				candidate := make([]uint32, 0, len(outline)-1)
				candidate = append(candidate, outline[:strokeIndex]...)
				candidate = append(candidate, outline[strokeIndex+1:]...)
				addCandidate(candidate)
			}
		}
	}

	return claims
}

func aouStroke() uint32 {
	return bit(keyA) | bit(keyO) | bit(keyU)
}

func mergeSuffixStroke(left, suffix uint32) (uint32, bool) {
	direct := bit(keyRightR) |
		bit(keyRightL) |
		bit(keyRightT) |
		bit(keyRightS) |
		bit(keyRightD) |
		bit(keyRightZ)
	allowed := direct | bit(keyRightG)
	if suffix == 0 || suffix & ^allowed != 0 {
		return 0, false
	}

	nonG := suffix &^ bit(keyRightG)
	if left&nonG != 0 {
		return 0, false
	}
	merged := left | nonG

	if suffix&bit(keyRightG) != 0 {
		if merged&bit(keyRightG) == 0 {
			merged |= bit(keyRightG)
		} else {
			ingReplacement := bit(keyRightD) | bit(keyRightZ)
			if merged&ingReplacement != 0 {
				return 0, false
			}
			merged |= ingReplacement
		}
	}

	return merged, true
}

func selectSafeCandidates(sources map[string]sourceEntry, claims map[string]*candidateClaim) (map[string]string, int, int) {
	additional := make(map[string]string)
	outlineByKey := make(map[string][]uint32)
	ambiguousCount := 0

	for key, claim := range claims {
		if claim.conflict {
			ambiguousCount++
			continue
		}
		additional[key] = claim.value
		outlineByKey[key] = claim.outline
	}

	prefixes := newOutlineTrie()
	for _, entry := range sources {
		prefixes.insert(entry.outline)
	}
	ignored := ignoredBoundaryOutlines()
	boundaryCount := 0

	// Checking against the complete candidate set is intentionally conservative.
	// It also removes the order dependence in lapwing_augmentor's insertion pass.
	boundaryConflicts := make([]string, 0)
	for _, key := range sortedKeys(additional) {
		if !validWordBoundaries(outlineByKey[key], sources, additional, prefixes, ignored) {
			boundaryConflicts = append(boundaryConflicts, key)
		}
	}
	for _, key := range boundaryConflicts {
		delete(additional, key)
	}
	boundaryCount = len(boundaryConflicts)

	return additional, ambiguousCount, boundaryCount
}

func validWordBoundaries(
	outline []uint32,
	sources map[string]sourceEntry,
	additional map[string]string,
	prefixes *outlineTrie,
	ignored map[string]struct{},
) bool {
	if len(outline) < 2 {
		return true
	}

	for split := 1; split < len(outline); split++ {
		prefixOutline := outline[:split]
		suffixOutline := outline[split:]
		prefix := formatOutline(prefixOutline)
		suffix := formatOutline(suffixOutline)

		prefixValue, prefixExists := dictionaryValue(prefix, sources, additional)
		suffixValue, suffixExists := dictionaryValue(suffix, sources, additional)
		if !suffixExists {
			suffixExists = prefixes.hasPrefix(suffixOutline)
		}
		if !prefixExists || !suffixExists {
			continue
		}
		if _, ok := ignored[prefix]; ok {
			continue
		}
		if _, ok := ignored[suffix]; ok {
			continue
		}
		if strings.HasSuffix(prefixValue, "^}") || strings.HasPrefix(suffixValue, "{^") {
			continue
		}
		return false
	}

	return true
}

func dictionaryValue(key string, sources map[string]sourceEntry, additional map[string]string) (string, bool) {
	if entry, ok := sources[key]; ok {
		return entry.value, true
	}
	value, ok := additional[key]
	return value, ok
}

func ignoredBoundaryOutlines() map[string]struct{} {
	// These are the same deliberate word-boundary exceptions used by
	// lapwing_augmentor, canonicalized through Stoin's stroke parser.
	raw := []string{
		"SK", "KP*", "TA", "KP", "K-P", "-FP", "A*", "PH", "PW", "P*", "-BG", "S-G",
	}
	ignored := make(map[string]struct{}, len(raw))
	for _, outline := range raw {
		if parsed, ok := parseOutline(outline); ok {
			ignored[formatOutline(parsed)] = struct{}{}
		}
	}
	return ignored
}

func parseOutline(raw string) ([]uint32, bool) {
	parts := strings.Split(raw, "/")
	if len(parts) == 0 {
		return nil, false
	}
	outline := make([]uint32, len(parts))
	for index, part := range parts {
		stroke, ok := parseStroke(part)
		if !ok {
			return nil, false
		}
		outline[index] = stroke
	}
	return outline, true
}

func parseStroke(raw string) (uint32, bool) {
	var stroke uint32
	region := regionLeft
	sawAny := false
	sawNumberDigit := false

	for _, char := range raw {
		if char == '/' {
			return 0, false
		}
		if char == '-' {
			region = regionRight
			continue
		}

		var keyBit uint32
		isNumberDigit := false
		switch region {
		case regionLeft:
			keyBit = leftBit(char)
			if keyBit == 0 {
				keyBit = leftNumberBit(char)
				isNumberDigit = keyBit != 0
			}
			if keyBit == 0 {
				keyBit = vowelBit(char)
				if keyBit != 0 {
					region = regionVowels
				}
			}
			if keyBit == 0 {
				keyBit = vowelNumberBit(char)
				if keyBit != 0 {
					region = regionVowels
					isNumberDigit = true
				}
			}
			if keyBit == 0 {
				keyBit = rightBit(char)
				if keyBit != 0 {
					region = regionRight
				}
			}
			if keyBit == 0 {
				keyBit = rightNumberBit(char)
				if keyBit != 0 {
					region = regionRight
					isNumberDigit = true
				}
			}
		case regionVowels:
			keyBit = vowelBit(char)
			if keyBit == 0 {
				keyBit = vowelNumberBit(char)
				isNumberDigit = keyBit != 0
			}
			if keyBit == 0 {
				keyBit = rightBit(char)
				if keyBit != 0 {
					region = regionRight
				}
			}
			if keyBit == 0 {
				keyBit = rightNumberBit(char)
				if keyBit != 0 {
					region = regionRight
					isNumberDigit = true
				}
			}
		case regionRight:
			keyBit = rightBit(char)
			if keyBit == 0 {
				keyBit = rightNumberBit(char)
				isNumberDigit = keyBit != 0
			}
			if keyBit == 0 {
				// Stoin accepts Plover's internal hyphen before the vowel bank,
				// for example -ER and TKA-EURB.
				keyBit = vowelBit(char)
				if keyBit != 0 {
					region = regionVowels
				}
			}
			if keyBit == 0 {
				keyBit = vowelNumberBit(char)
				if keyBit != 0 {
					region = regionVowels
					isNumberDigit = true
				}
			}
		}

		if keyBit == 0 || stroke&keyBit != 0 {
			return 0, false
		}
		stroke |= keyBit
		sawAny = true
		if isNumberDigit {
			sawNumberDigit = true
		}
	}

	if !sawAny || sawNumberDigit && stroke&bit(keyNumber) == 0 {
		return 0, false
	}
	return stroke, true
}

func leftBit(char rune) uint32 {
	switch char {
	case '#':
		return bit(keyNumber)
	case 'S':
		return bit(keyLeftS)
	case 'T':
		return bit(keyLeftT)
	case 'K':
		return bit(keyLeftK)
	case 'P':
		return bit(keyLeftP)
	case 'W':
		return bit(keyLeftW)
	case 'H':
		return bit(keyLeftH)
	case 'R':
		return bit(keyLeftR)
	default:
		return 0
	}
}

func leftNumberBit(char rune) uint32 {
	switch char {
	case '1':
		return bit(keyLeftS)
	case '2':
		return bit(keyLeftT)
	case '3':
		return bit(keyLeftP)
	case '4':
		return bit(keyLeftH)
	default:
		return 0
	}
}

func vowelBit(char rune) uint32 {
	switch char {
	case 'A':
		return bit(keyA)
	case 'O':
		return bit(keyO)
	case '*':
		return bit(keyStar)
	case 'E':
		return bit(keyE)
	case 'U':
		return bit(keyU)
	default:
		return 0
	}
}

func vowelNumberBit(char rune) uint32 {
	switch char {
	case '5':
		return bit(keyA)
	case '0':
		return bit(keyO)
	default:
		return 0
	}
}

func rightBit(char rune) uint32 {
	switch char {
	case 'F':
		return bit(keyRightF)
	case 'R':
		return bit(keyRightR)
	case 'P':
		return bit(keyRightP)
	case 'B':
		return bit(keyRightB)
	case 'L':
		return bit(keyRightL)
	case 'G':
		return bit(keyRightG)
	case 'T':
		return bit(keyRightT)
	case 'S':
		return bit(keyRightS)
	case 'D':
		return bit(keyRightD)
	case 'Z':
		return bit(keyRightZ)
	default:
		return 0
	}
}

func rightNumberBit(char rune) uint32 {
	switch char {
	case '6':
		return bit(keyRightF)
	case '7':
		return bit(keyRightP)
	case '8':
		return bit(keyRightL)
	case '9':
		return bit(keyRightT)
	default:
		return 0
	}
}

func formatOutline(outline []uint32) string {
	strokes := make([]string, len(outline))
	for index, stroke := range outline {
		strokes[index] = formatStroke(stroke)
	}
	return strings.Join(strokes, "/")
}

func formatStroke(stroke uint32) string {
	leftAndVowels := []struct {
		key   stenoKey
		label byte
	}{
		{keyNumber, '#'},
		{keyLeftS, 'S'},
		{keyLeftT, 'T'},
		{keyLeftK, 'K'},
		{keyLeftP, 'P'},
		{keyLeftW, 'W'},
		{keyLeftH, 'H'},
		{keyLeftR, 'R'},
		{keyA, 'A'},
		{keyO, 'O'},
		{keyStar, '*'},
		{keyE, 'E'},
		{keyU, 'U'},
	}
	right := []struct {
		key   stenoKey
		label byte
	}{
		{keyRightF, 'F'},
		{keyRightR, 'R'},
		{keyRightP, 'P'},
		{keyRightB, 'B'},
		{keyRightL, 'L'},
		{keyRightG, 'G'},
		{keyRightT, 'T'},
		{keyRightS, 'S'},
		{keyRightD, 'D'},
		{keyRightZ, 'Z'},
	}

	var leftBuilder strings.Builder
	for _, item := range leftAndVowels {
		if stroke&bit(item.key) != 0 {
			leftBuilder.WriteByte(item.label)
		}
	}
	var rightBuilder strings.Builder
	for _, item := range right {
		if stroke&bit(item.key) != 0 {
			rightBuilder.WriteByte(item.label)
		}
	}

	left := leftBuilder.String()
	rightLabels := rightBuilder.String()
	if rightLabels == "" {
		return left
	}
	implicit := left + rightLabels
	if parsed, ok := parseStroke(implicit); ok && parsed == stroke {
		return implicit
	}
	return left + "-" + rightLabels
}

func writeDictionary(path string, dictionary map[string]string) error {
	directory := filepath.Dir(path)
	temporary, err := os.CreateTemp(directory, ".stoin-dict-augment-*.json")
	if err != nil {
		return fmt.Errorf("create temporary output in %q: %w", directory, err)
	}
	temporaryPath := temporary.Name()
	keep := false
	defer func() {
		if !keep {
			_ = os.Remove(temporaryPath)
		}
	}()

	encoder := json.NewEncoder(temporary)
	encoder.SetEscapeHTML(false)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(dictionary); err != nil {
		_ = temporary.Close()
		return fmt.Errorf("encode output: %w", err)
	}
	if err := temporary.Chmod(0o644); err != nil {
		_ = temporary.Close()
		return fmt.Errorf("set output permissions: %w", err)
	}
	if err := temporary.Close(); err != nil {
		return fmt.Errorf("close temporary output: %w", err)
	}
	if err := os.Rename(temporaryPath, path); err != nil {
		return fmt.Errorf("replace output %q: %w", path, err)
	}
	keep = true
	return nil
}

func sortedKeys[V any](values map[string]V) []string {
	keys := make([]string, 0, len(values))
	for key := range values {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	return keys
}
