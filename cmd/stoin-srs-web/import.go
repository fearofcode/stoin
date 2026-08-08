package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"strings"
)

func parseImportText(content string, groupName string) ([]ImportGroup, []ParseIssue) {
	groups, issues, _ := parseImportTextWithDuplicatePolicy(content, groupName, false)
	return groups, issues
}

func parseDeduplicatedImportText(content string, groupName string) ([]ImportGroup, []ParseIssue, int) {
	return parseImportTextWithDuplicatePolicy(content, groupName, true)
}

func parseImportTextWithDuplicatePolicy(content string, groupName string, dropDuplicates bool) ([]ImportGroup, []ParseIssue, int) {
	if strings.TrimSpace(groupName) != "" {
		return parseSimpleImportWithDuplicatePolicy(content, strings.TrimSpace(groupName), dropDuplicates)
	}
	return parseGroupedImportWithDuplicatePolicy(content, dropDuplicates)
}

func parseSimpleImport(content string, groupName string) ([]ImportGroup, []ParseIssue) {
	groups, issues, _ := parseSimpleImportWithDuplicatePolicy(content, groupName, false)
	return groups, issues
}

func parseSimpleImportWithDuplicatePolicy(content string, groupName string, dropDuplicates bool) ([]ImportGroup, []ParseIssue, int) {
	var words []string
	seen := map[string]int{}
	var issues []ParseIssue
	duplicatesRemoved := 0
	for lineNumber, line := range strings.Split(content, "\n") {
		word := strings.TrimSpace(strings.TrimSuffix(line, "\r"))
		if word == "" {
			continue
		}
		if firstLine, ok := seen[word]; ok {
			if dropDuplicates {
				duplicatesRemoved++
			} else {
				issues = append(issues, ParseIssue{
					Line:    lineNumber + 1,
					Message: fmt.Sprintf("duplicate word in group %q (first seen on line %d)", groupName, firstLine),
				})
			}
			continue
		}
		seen[word] = lineNumber + 1
		words = append(words, word)
	}
	if len(words) == 0 {
		issues = append(issues, ParseIssue{Line: 1, Message: "import text contains no words"})
	}
	if len(issues) > 0 {
		return nil, issues, duplicatesRemoved
	}
	return []ImportGroup{{Name: groupName, Line: 1, Words: words}}, nil, duplicatesRemoved
}

func parseGroupedImport(content string) ([]ImportGroup, []ParseIssue) {
	groups, issues, _ := parseGroupedImportWithDuplicatePolicy(content, false)
	return groups, issues
}

func parseGroupedImportWithDuplicatePolicy(content string, dropDuplicates bool) ([]ImportGroup, []ParseIssue, int) {
	var groups []ImportGroup
	groupLines := map[string]int{}
	allWords := map[string]int{}
	var issues []ParseIssue
	duplicatesRemoved := 0

	current := ImportGroup{}
	currentSeen := map[string]int{}
	currentValid := false
	currentHadWords := false
	previousBlank := true

	finishCurrent := func() {
		if current.Name == "" {
			return
		}
		if currentValid {
			if len(current.Words) == 0 {
				if !dropDuplicates || !currentHadWords {
					issues = append(issues, ParseIssue{
						Line:    current.Line,
						Message: fmt.Sprintf("group %q contains no words", current.Name),
					})
				}
			} else {
				groups = append(groups, current)
			}
		}
		current = ImportGroup{}
		currentSeen = map[string]int{}
		currentValid = false
		currentHadWords = false
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
			currentHadWords = true
			seen := currentSeen
			if dropDuplicates {
				seen = allWords
			}
			if firstLine, ok := seen[stripped]; ok {
				if dropDuplicates {
					duplicatesRemoved++
				} else {
					issues = append(issues, ParseIssue{
						Line:    lineNumber,
						Message: fmt.Sprintf("duplicate word in group %q (first seen on line %d)", current.Name, firstLine),
					})
				}
			} else {
				seen[stripped] = lineNumber
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
		return nil, issues, duplicatesRemoved
	}
	return groups, nil, duplicatesRemoved
}

func importHash(content string, groupName string) string {
	hash := sha256.Sum256([]byte(groupName + "\x00" + content))
	return hex.EncodeToString(hash[:])
}
