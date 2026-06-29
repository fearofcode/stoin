package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"strings"
)

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
