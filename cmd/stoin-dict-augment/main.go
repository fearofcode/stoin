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

type stringListFlag []string

func (values *stringListFlag) String() string {
	return strings.Join(*values, ",")
}

func (values *stringListFlag) Set(value string) error {
	*values = append(*values, value)
	return nil
}

type supplementalStats struct {
	exactPrimaryOverlaps   int
	exactGeneratedOverlaps int
	rrExcluded             int
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
	var additionalPaths stringListFlag
	flag.StringVar(&outputPath, "output", "", "path for the generated augmentation dictionary")
	flag.StringVar(&outputPath, "o", "", "path for the generated augmentation dictionary (shorthand)")
	flag.Var(&additionalPaths, "additional", "supplemental dictionary whose non-conflicting entries are copied (repeatable)")
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s -output <augmentations.json> [-additional <supplemental.json>] <source.json> [source.json ...]\n", filepath.Base(os.Args[0]))
		flag.PrintDefaults()
	}
	flag.Parse()

	if outputPath == "" || flag.NArg() == 0 {
		flag.Usage()
		os.Exit(2)
	}

	if err := run(outputPath, flag.Args(), additionalPaths, os.Stdout, os.Stderr); err != nil {
		fmt.Fprintf(os.Stderr, "stoin-dict-augment: %v\n", err)
		os.Exit(1)
	}
}

func run(outputPath string, inputPaths, additionalPaths []string, stdout, stderr io.Writer) error {
	allInputPaths := append(append([]string(nil), inputPaths...), additionalPaths...)
	if err := ensureDistinctOutput(outputPath, allInputPaths); err != nil {
		return err
	}

	sources, invalidCount, err := loadSources(inputPaths)
	if err != nil {
		return err
	}
	if invalidCount != 0 {
		fmt.Fprintf(stderr, "Skipped %d source entries whose outlines Stoin cannot parse.\n", invalidCount)
	}

	excludedTranslations := rrMarkedTranslations(sources)
	claims := generateCandidateClaims(sources, excludedTranslations)
	augmentations, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(sources, claims)

	var supplemental map[string]sourceEntry
	var supplementalKeys map[string]struct{}
	var importStats supplementalStats
	if len(additionalPaths) != 0 {
		supplemental, invalidCount, err = loadSources(additionalPaths)
		if err != nil {
			return err
		}
		if invalidCount != 0 {
			fmt.Fprintf(stderr, "Skipped %d supplemental entries whose outlines Stoin cannot parse.\n", invalidCount)
		}

		supplementalExcluded := rrMarkedTranslations(supplemental)
		for translation := range excludedTranslations {
			supplementalExcluded[translation] = struct{}{}
		}
		retainAcceptedClaims(claims, augmentations)
		importStats, supplementalKeys = addSupplementalClaims(sources, supplemental, claims, supplementalExcluded)

		var additionalAmbiguous, additionalJoin, additionalBoundary int
		augmentations, additionalAmbiguous, additionalJoin, additionalBoundary = selectSafeCandidates(sources, claims)
		ambiguousCount += additionalAmbiguous
		joinCount += additionalJoin
		boundaryCount += additionalBoundary
	}

	if err := writeDictionary(outputPath, augmentations); err != nil {
		return err
	}

	fmt.Fprintf(
		stdout,
		"Read %d canonical source entries; excluded %d R-R-marked translations; wrote %d additions to %s (%d ambiguous, %d trailing-P-P, and %d boundary-conflicting candidates skipped).\n",
		len(sources),
		len(excludedTranslations),
		len(augmentations),
		outputPath,
		ambiguousCount,
		joinCount,
		boundaryCount,
	)
	if len(additionalPaths) != 0 {
		importedCount := 0
		for key := range supplementalKeys {
			if _, imported := augmentations[key]; imported {
				importedCount++
			}
		}
		fmt.Fprintf(
			stdout,
			"Read %d canonical supplemental entries; imported %d after skipping %d exact primary overlaps, %d existing augmentation overlaps, and %d entries with R-R-marked translations.\n",
			len(supplemental),
			importedCount,
			importStats.exactPrimaryOverlaps,
			importStats.exactGeneratedOverlaps,
			importStats.rrExcluded,
		)
	}
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

func rrMarkedTranslations(sources map[string]sourceEntry) map[string]struct{} {
	rrStroke := bit(keyLeftR) | bit(keyRightR)
	excluded := make(map[string]struct{})
	for _, entry := range sources {
		for strokeIndex := 1; strokeIndex < len(entry.outline); strokeIndex++ {
			if entry.outline[strokeIndex] == rrStroke {
				excluded[entry.value] = struct{}{}
				break
			}
		}
	}
	return excluded
}

func endsWithJoinStroke(outline []uint32) bool {
	joinStroke := bit(keyLeftP) | bit(keyRightP)
	return len(outline) != 0 && outline[len(outline)-1] == joinStroke
}

func addClaim(claims map[string]*candidateClaim, key string, outline []uint32, value string) {
	claim := claims[key]
	if claim == nil {
		claims[key] = &candidateClaim{
			outline: append([]uint32(nil), outline...),
			value:   value,
		}
	} else if claim.value != value {
		claim.conflict = true
	}
}

func retainAcceptedClaims(claims map[string]*candidateClaim, accepted map[string]string) {
	for key := range claims {
		if _, ok := accepted[key]; !ok {
			delete(claims, key)
		}
	}
}

func addSupplementalClaims(
	sources, supplemental map[string]sourceEntry,
	claims map[string]*candidateClaim,
	excludedTranslations map[string]struct{},
) (supplementalStats, map[string]struct{}) {
	stats := supplementalStats{}
	keys := make(map[string]struct{})

	for _, key := range sortedKeys(supplemental) {
		entry := supplemental[key]
		if _, overlaps := sources[key]; overlaps {
			stats.exactPrimaryOverlaps++
			continue
		}
		if _, overlaps := claims[key]; overlaps {
			stats.exactGeneratedOverlaps++
			continue
		}
		if _, excluded := excludedTranslations[entry.value]; excluded {
			stats.rrExcluded++
			continue
		}
		keys[key] = struct{}{}
		addClaim(claims, key, entry.outline, entry.value)
	}

	return stats, keys
}

func generateCandidateClaims(sources map[string]sourceEntry, excludedTranslations map[string]struct{}) map[string]*candidateClaim {
	claims := make(map[string]*candidateClaim)
	keys := sortedKeys(sources)

	for _, sourceKey := range keys {
		entry := sources[sourceKey]
		if _, excluded := excludedTranslations[entry.value]; excluded || endsWithJoinStroke(entry.outline) {
			continue
		}
		states := map[string][]uint32{sourceKey: entry.outline}
		queue := [][]uint32{entry.outline}
		addCandidate := func(candidate []uint32) {
			candidateKey := formatOutline(candidate)
			if _, seen := states[candidateKey]; seen {
				return
			}
			states[candidateKey] = candidate

			trailingJoin := endsWithJoinStroke(candidate)
			if source, exists := sources[candidateKey]; exists {
				if source.value == entry.value && !trailingJoin {
					queue = append(queue, candidate)
				}
				return
			}
			if !trailingJoin {
				queue = append(queue, candidate)
			}
			addClaim(claims, candidateKey, candidate, entry.value)
		}

		for len(queue) != 0 {
			outline := queue[0]
			queue = queue[1:]

			for boundary := 0; boundary+1 < len(outline); boundary++ {
				left := outline[boundary]
				suffix, ok := foldableSuffixBits(outline[boundary+1])
				if !ok {
					continue
				}
				merged, ok := mergeSuffixStroke(left, suffix, false)
				if !ok {
					continue
				}

				candidate := outlineWithMergedBoundary(outline, boundary, merged)
				addCandidate(candidate)

				// Prefer an ordinary G fold when the key is free. If that exact
				// outline is already assigned to a different source translation,
				// try the DZ -ing chord as a collision fallback too.
				if suffix&bit(keyRightG) == 0 || left&bit(keyRightG) != 0 {
					continue
				}
				directKey := formatOutline(candidate)
				directSource, occupied := sources[directKey]
				if !occupied || directSource.value == entry.value {
					continue
				}
				fallback, ok := mergeSuffixStroke(left, suffix, true)
				if ok {
					addCandidate(outlineWithMergedBoundary(outline, boundary, fallback))
				}
			}

			for boundary := 0; boundary+1 < len(outline); boundary++ {
				candidate, ok := foldVowelCodaStroke(outline, boundary)
				if ok {
					addCandidate(candidate)
				}
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

			for strokeIndex := 1; strokeIndex+1 < len(outline); strokeIndex++ {
				candidate, ok := omitInteriorKWRVowelStroke(outline, strokeIndex)
				if ok {
					addCandidate(candidate)
				}
			}

			for strokeIndex := 1; strokeIndex+1 < len(outline); strokeIndex++ {
				candidate, ok := redistributeVowellessLBridge(outline, strokeIndex)
				if ok {
					addCandidate(candidate)
				}
			}

			for strokeIndex := 1; strokeIndex+1 < len(outline); strokeIndex++ {
				candidate, ok := foldInteriorConsonantVowelStroke(outline, strokeIndex)
				if ok {
					addCandidate(candidate)
				}
			}

			candidate, ok := omitSecondConsonantVowelStroke(outline)
			if ok {
				addCandidate(candidate)
			}

			candidate, ok = collapseLeadingConsonantVowelStroke(outline)
			if ok {
				addCandidate(candidate)
			}

			candidate, ok = omitLeadingVowelStroke(outline)
			if ok {
				addCandidate(candidate)
			}

			if len(outline) > 3 {
				candidate := append([]uint32(nil), outline[:len(outline)-1]...)
				addCandidate(candidate)
			}
		}
	}

	return claims
}

func aouStroke() uint32 {
	return bit(keyA) | bit(keyO) | bit(keyU)
}

func kwrLinkerStroke() uint32 {
	return bit(keyLeftK) | bit(keyLeftW) | bit(keyLeftR)
}

func vowelMask() uint32 {
	return bit(keyA) | bit(keyO) | bit(keyE) | bit(keyU)
}

func omitLeadingVowelStroke(outline []uint32) ([]uint32, bool) {
	if len(outline) < 2 || outline[0] == 0 || outline[0]&^vowelMask() != 0 {
		return nil, false
	}
	return append([]uint32(nil), outline[1:]...), true
}

func omitInteriorKWRVowelStroke(outline []uint32, strokeIndex int) ([]uint32, bool) {
	if len(outline) < 3 || strokeIndex <= 0 || strokeIndex+1 >= len(outline) {
		return nil, false
	}

	stroke := outline[strokeIndex]
	vowels := stroke & vowelMask()
	if vowels == 0 || stroke != kwrLinkerStroke()|vowels {
		return nil, false
	}

	candidate := make([]uint32, 0, len(outline)-1)
	candidate = append(candidate, outline[:strokeIndex]...)
	candidate = append(candidate, outline[strokeIndex+1:]...)
	return candidate, true
}

func leftHandMask() uint32 {
	return bit(keyLeftS) |
		bit(keyLeftT) |
		bit(keyLeftK) |
		bit(keyLeftP) |
		bit(keyLeftW) |
		bit(keyLeftH) |
		bit(keyLeftR)
}

func foldInteriorConsonantVowelStroke(outline []uint32, middleIndex int) ([]uint32, bool) {
	if len(outline) < 3 || middleIndex <= 0 || middleIndex+1 >= len(outline) {
		return nil, false
	}

	middle := outline[middleIndex]
	consonants := middle & leftHandMask()
	vowels := middle & vowelMask()
	if consonants == 0 || vowels == 0 || middle != consonants|vowels {
		return nil, false
	}

	foldedConsonants, ok := rightHandConsonantFold(consonants)
	if !ok || outline[middleIndex-1]&foldedConsonants != 0 {
		return nil, false
	}

	candidate := make([]uint32, 0, len(outline)-1)
	candidate = append(candidate, outline[:middleIndex-1]...)
	candidate = append(candidate, outline[middleIndex-1]|foldedConsonants)
	candidate = append(candidate, outline[middleIndex+1:]...)
	return candidate, true
}

func omitSecondConsonantVowelStroke(outline []uint32) ([]uint32, bool) {
	if len(outline) < 3 {
		return nil, false
	}

	second := outline[1]
	consonants := second & leftHandMask()
	vowels := second & vowelMask()
	if consonants == 0 || vowels == 0 || second != consonants|vowels {
		return nil, false
	}

	candidate := make([]uint32, 0, len(outline)-1)
	candidate = append(candidate, outline[0])
	candidate = append(candidate, outline[2:]...)
	return candidate, true
}

func rightHandConsonantFold(consonants uint32) (uint32, bool) {
	// These are the complete-consonant moves used by Lapwing's alternate
	// syllable splitting. Requiring an exact chord avoids partial phonetic
	// rewrites while still covering ordinary single-key folds such as T -> -T.
	folds := []struct {
		left  uint32
		right uint32
	}{
		{bit(keyLeftS), bit(keyRightS)},
		{bit(keyLeftT), bit(keyRightT)},
		{bit(keyLeftP), bit(keyRightP)},
		{bit(keyLeftR), bit(keyRightR)},
		{bit(keyLeftP) | bit(keyLeftW), bit(keyRightB)},
		{bit(keyLeftT) | bit(keyLeftK), bit(keyRightD)},
		{bit(keyLeftT) | bit(keyLeftP), bit(keyRightF)},
		{bit(keyLeftT) | bit(keyLeftK) | bit(keyLeftP) | bit(keyLeftW), bit(keyRightP) | bit(keyRightB) | bit(keyRightL) | bit(keyRightG)},
		{bit(keyLeftS) | bit(keyLeftK) | bit(keyLeftW) | bit(keyLeftR), bit(keyRightP) | bit(keyRightB) | bit(keyRightL) | bit(keyRightG)},
		{bit(keyLeftK), bit(keyRightB) | bit(keyRightG)},
		{bit(keyLeftH) | bit(keyLeftR), bit(keyRightL)},
		{bit(keyLeftP) | bit(keyLeftH), bit(keyRightP) | bit(keyRightL)},
		{bit(keyLeftT) | bit(keyLeftP) | bit(keyLeftH), bit(keyRightP) | bit(keyRightB)},
		{bit(keyLeftS) | bit(keyLeftR), bit(keyRightF)},
		{bit(keyLeftT) | bit(keyLeftH), bit(keyStar) | bit(keyRightT)},
		{bit(keyLeftK) | bit(keyLeftH), bit(keyRightF) | bit(keyRightP)},
		{bit(keyLeftS) | bit(keyLeftH), bit(keyRightR) | bit(keyRightB)},
		{bit(keyLeftS) | bit(keyLeftT) | bit(keyLeftK) | bit(keyLeftP) | bit(keyLeftW), bit(keyRightZ)},
	}
	for _, fold := range folds {
		if consonants == fold.left {
			return fold.right, true
		}
	}
	return 0, false
}

func collapseLeadingConsonantVowelStroke(outline []uint32) ([]uint32, bool) {
	if len(outline) < 2 || outline[1]&vowelMask() == 0 {
		return nil, false
	}

	leading := outline[0]
	leadingLeft := leading & leftHandMask()
	leadingVowels := leading & vowelMask()
	leadingRight := leading & rightHandMask()
	if leadingVowels == 0 || leading != leadingLeft|leadingVowels|leadingRight {
		return nil, false
	}

	foldedRight, ok := leftHandConsonantFold(leadingRight)
	if !ok || !leftKeysPrecede(leadingLeft, foldedRight) {
		return nil, false
	}
	movedLeft := leadingLeft | foldedRight
	if movedLeft == 0 {
		return nil, false
	}
	nextLeft := outline[1] & leftHandMask()
	if !leftKeysPrecede(movedLeft, nextLeft) {
		return nil, false
	}

	candidate := make([]uint32, 0, len(outline)-1)
	candidate = append(candidate, outline[1]|movedLeft)
	candidate = append(candidate, outline[2:]...)
	return candidate, true
}

func leftHandConsonantFold(consonants uint32) (uint32, bool) {
	// Prefer the same unambiguous complete-chord moves as Lapwing when a
	// right-hand consonant coda becomes the beginning of the following stroke.
	folds := []struct {
		right uint32
		left  uint32
	}{
		{0, 0},
		{bit(keyRightS), bit(keyLeftS)},
		{bit(keyRightT), bit(keyLeftT)},
		{bit(keyRightP), bit(keyLeftP)},
		{bit(keyRightR), bit(keyLeftR)},
		{bit(keyRightB), bit(keyLeftP) | bit(keyLeftW)},
		{bit(keyRightD), bit(keyLeftT) | bit(keyLeftK)},
		{bit(keyRightF), bit(keyLeftT) | bit(keyLeftP)},
		{bit(keyRightP) | bit(keyRightB) | bit(keyRightL) | bit(keyRightG), bit(keyLeftS) | bit(keyLeftK) | bit(keyLeftW) | bit(keyLeftR)},
		{bit(keyRightB) | bit(keyRightG), bit(keyLeftK)},
		{bit(keyRightL), bit(keyLeftH) | bit(keyLeftR)},
		{bit(keyRightP) | bit(keyRightL), bit(keyLeftP) | bit(keyLeftH)},
		{bit(keyRightP) | bit(keyRightB), bit(keyLeftT) | bit(keyLeftP) | bit(keyLeftH)},
		{bit(keyRightF) | bit(keyRightP), bit(keyLeftK) | bit(keyLeftH)},
		{bit(keyRightR) | bit(keyRightB), bit(keyLeftS) | bit(keyLeftH)},
		{bit(keyRightZ), bit(keyLeftS) | bit(keyLeftT) | bit(keyLeftK) | bit(keyLeftP) | bit(keyLeftW)},
	}
	for _, fold := range folds {
		if consonants == fold.right {
			return fold.left, true
		}
	}
	return 0, false
}

func leftKeysPrecede(prefix, suffix uint32) bool {
	for prefixKey := keyLeftS; prefixKey <= keyLeftR; prefixKey++ {
		if prefix&bit(prefixKey) == 0 {
			continue
		}
		for suffixKey := keyLeftS; suffixKey <= prefixKey; suffixKey++ {
			if suffix&bit(suffixKey) != 0 {
				return false
			}
		}
	}
	return true
}

func redistributeVowellessLBridge(outline []uint32, middleIndex int) ([]uint32, bool) {
	if len(outline) < 3 || middleIndex <= 0 || middleIndex+1 >= len(outline) {
		return nil, false
	}

	previous := outline[middleIndex-1]
	middle := outline[middleIndex]
	next := outline[middleIndex+1]
	if previous&vowelMask() == 0 || next&vowelMask() == 0 || middle&vowelMask() != 0 {
		return nil, false
	}

	leftToRight := []struct {
		left  stenoKey
		right stenoKey
	}{
		{keyLeftS, keyRightS},
		{keyLeftT, keyRightT},
		{keyLeftP, keyRightP},
		{keyLeftR, keyRightR},
	}
	movableLeft := uint32(0)
	for _, pair := range leftToRight {
		movableLeft |= bit(pair.left)
	}
	allowedMiddle := movableLeft | bit(keyRightL)
	if middle&bit(keyRightL) == 0 || middle&^allowedMiddle != 0 {
		return nil, false
	}
	middleLeft := middle & movableLeft
	if middleLeft == 0 {
		return nil, false
	}

	redistributedPrevious := previous
	for _, pair := range leftToRight {
		if middleLeft&bit(pair.left) == 0 {
			continue
		}
		if redistributedPrevious&bit(pair.right) != 0 {
			return nil, false
		}
		redistributedPrevious |= bit(pair.right)
	}

	leftL := bit(keyLeftH) | bit(keyLeftR)
	if next&leftL != 0 {
		return nil, false
	}
	redistributedNext := next | leftL

	candidate := make([]uint32, 0, len(outline)-1)
	candidate = append(candidate, outline[:middleIndex-1]...)
	candidate = append(candidate, redistributedPrevious, redistributedNext)
	candidate = append(candidate, outline[middleIndex+2:]...)
	return candidate, true
}

func outlineWithMergedBoundary(outline []uint32, boundary int, merged uint32) []uint32 {
	candidate := make([]uint32, 0, len(outline)-1)
	candidate = append(candidate, outline[:boundary]...)
	candidate = append(candidate, merged)
	candidate = append(candidate, outline[boundary+2:]...)
	return candidate
}

func foldableSuffixBits(stroke uint32) (uint32, bool) {
	plainSuffixes := bit(keyRightR) |
		bit(keyRightL) |
		bit(keyRightG) |
		bit(keyRightT) |
		bit(keyRightS) |
		bit(keyRightD) |
		bit(keyRightZ)
	if stroke != 0 && stroke & ^plainSuffixes == 0 {
		return stroke, true
	}

	kwrLinker := kwrLinkerStroke()
	if stroke&kwrLinker != kwrLinker {
		return 0, false
	}
	kwrSuffix := stroke &^ kwrLinker
	if kwrSuffix == 0 || kwrSuffix & ^rightHandMask() != 0 {
		return 0, false
	}
	return kwrSuffix, true
}

func rightHandMask() uint32 {
	return bit(keyRightF) |
		bit(keyRightR) |
		bit(keyRightP) |
		bit(keyRightB) |
		bit(keyRightL) |
		bit(keyRightG) |
		bit(keyRightT) |
		bit(keyRightS) |
		bit(keyRightD) |
		bit(keyRightZ)
}

func foldVowelCodaStroke(outline []uint32, boundary int) ([]uint32, bool) {
	if boundary < 0 || boundary+1 >= len(outline) {
		return nil, false
	}

	following := outline[boundary+1]
	linker := following & leftHandMask()
	vowels := following & vowelMask()
	coda := following & rightHandMask()
	kwrLinker := kwrLinkerStroke()
	if linker != 0 && linker != kwrLinker {
		return nil, false
	}
	if vowels == 0 || coda == 0 || following != linker|vowels|coda {
		return nil, false
	}
	preceding := outline[boundary]
	precedingVowels := preceding & vowelMask()
	if precedingVowels == 0 || preceding&bit(keyStar) != 0 {
		return nil, false
	}
	pluralPSes := linker == 0 &&
		vowels == bit(keyA)|bit(keyE) &&
		coda == bit(keyRightZ) &&
		preceding&rightHandMask() == bit(keyRightP)|bit(keyRightS)
	if precedingVowels != vowels && !pluralPSes {
		return nil, false
	}
	if outline[boundary]&coda != 0 {
		return nil, false
	}

	return outlineWithMergedBoundary(outline, boundary, outline[boundary]|coda), true
}

func mergeSuffixStroke(left, suffix uint32, forceDZForG bool) (uint32, bool) {
	if suffix == 0 || suffix & ^rightHandMask() != 0 {
		return 0, false
	}

	nonG := suffix &^ bit(keyRightG)
	if left&nonG != 0 {
		return 0, false
	}
	merged := left | nonG

	if suffix&bit(keyRightG) != 0 {
		if merged&bit(keyRightG) == 0 && !forceDZForG {
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

func selectSafeCandidates(sources map[string]sourceEntry, claims map[string]*candidateClaim) (map[string]string, int, int, int) {
	additional := make(map[string]string)
	outlineByKey := make(map[string][]uint32)
	ambiguousCount := 0
	joinCount := 0

	for key, claim := range claims {
		if claim.conflict {
			ambiguousCount++
			continue
		}
		if endsWithJoinStroke(claim.outline) {
			joinCount++
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

	return additional, ambiguousCount, joinCount, boundaryCount
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
