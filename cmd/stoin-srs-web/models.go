package main

import (
	"html/template"
	"time"
)

type Deck struct {
	ID        int64
	Name      string
	CreatedAt string
	Paused    bool
}

type Group struct {
	ID             int64
	DeckID         int64
	Name           string
	CreatedAt      string
	LearningCount  int
	IntroRemaining int
	Items          []Item
}

type Item struct {
	ID             int64
	GroupID        int64
	DeckName       string
	GroupName      string
	Text           string
	IntroRemaining int
	ScheduleStage  int
	IntervalDays   float64
	DueAt          time.Time
	ReviewCount    int
	CorrectCount   int
	IncorrectCount int
}

type ImportGroup struct {
	Name  string
	Line  int
	Words []string
}

type ParseIssue struct {
	Line    int
	Message string
}

type ImportStats struct {
	Groups          int
	ItemsRead       int
	Added           int
	Existing        int
	DuplicateImport bool
}

type IndexPageData struct {
	Decks          []Deck
	ActiveDecks    []Deck
	PausedDecks    []Deck
	Errors         []ParseIssue
	Notice         string
	Form           ImportFormData
	DueLimit       int
	DueCount       int
	LearningCount  int
	IntroRemaining int
}

type ImportFormData struct {
	DeckID    int64
	DeckName  string
	GroupName string
	Content   string
}

type DeckPageData struct {
	Deck           Deck
	EditDeck       bool
	Groups         []Group
	Errors         []ParseIssue
	Notice         string
	EditItemID     int64
	ItemError      string
	Form           ImportFormData
	TotalItems     int
	DueCount       int
	LearningCount  int
	IntroRemaining int
}

type SessionItem struct {
	ID        int64  `json:"id"`
	Text      string `json:"text"`
	DeckName  string `json:"deckName"`
	GroupName string `json:"groupName"`
}

type SessionLineItem struct {
	Index int
	Item  SessionItem
}

type SessionLine struct {
	Index      int
	StartIndex int
	EndIndex   int
	Items      []SessionLineItem
}

type SessionPageData struct {
	Mode       string
	DeckID     int64
	ReturnURL  string
	Order      string
	Items      []SessionItem
	Lines      []SessionLine
	ItemsJSON  template.JS
	IsReview   bool
	IsPractice bool
}

type LearningStats struct {
	Count          int
	IntroRemaining int
}

type ReviewResult struct {
	ItemID  int64
	Prompt  string
	Answer  string
	Correct bool
}
