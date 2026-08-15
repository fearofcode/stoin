package main

import (
	"context"
	"database/sql"
	"embed"
	"flag"
	"html/template"
	"io/fs"
	"log"
	"net/http"
	"time"

	_ "modernc.org/sqlite"
)

const (
	defaultDBPath        = "stoin-srs-web.sqlite3"
	defaultAddr          = "127.0.0.1:8080"
	defaultConfigPath    = "stoin-config.json"
	defaultPhrasingPath  = "phrasing.json"
	defaultHintIndexPath = ".stoin-runtime-hints.json"
	defaultDailyNewLimit = 10
	introRepetitions     = 5
	recoveryPracticeDays = 5
	reviewAllDueLimit    = 100
	sessionLineMaxRunes  = 52
	maxRequestBodyBytes  = 4 << 20
)

var scheduleDays = []int{1, 3, 7, 14, 30, 60, 120, 240}

//go:embed templates/*.html static/*.css static/*.js
var assetFS embed.FS

type App struct {
	db            *sql.DB
	templates     *template.Template
	hints         *DictionaryHints
	phrasingPath  string
	dailyNewLimit int
	now           func() time.Time
}

func main() {
	dbPath := flag.String("db", defaultDBPath, "SQLite database path")
	addr := flag.String("addr", defaultAddr, "HTTP listen address")
	configPath := flag.String("config", defaultConfigPath, "Stoin config path for fallback dictionary hints")
	hintIndexPath := flag.String("hint-index", defaultHintIndexPath, "Running Stoin process hint index path")
	phrasingPath := flag.String("phrasing", defaultPhrasingPath, "Phrasing JSON path")
	dailyNewLimit := flag.Int("daily-new-limit", defaultDailyNewLimit, "Combined daily new-card limit for Mandatories, Briefs, and Phrases")
	flag.Parse()
	if *dailyNewLimit < 0 {
		log.Fatal("daily-new-limit must be zero or greater")
	}

	app, err := NewAppWithOptions(*dbPath, *phrasingPath, *configPath, *hintIndexPath)
	if err != nil {
		log.Fatal(err)
	}
	app.dailyNewLimit = *dailyNewLimit
	defer app.db.Close()

	mux := http.NewServeMux()
	app.routes(mux)

	log.Printf("stoin-srs-web listening on http://%s", *addr)
	log.Fatal(http.ListenAndServe(*addr, mux))
}

func NewApp(dbPath string) (*App, error) {
	return NewAppWithOptions(dbPath, defaultPhrasingPath, defaultConfigPath, "")
}

func NewAppWithPhrasing(dbPath string, phrasingPath string) (*App, error) {
	return NewAppWithOptions(dbPath, phrasingPath, defaultConfigPath, "")
}

func NewAppWithOptions(dbPath string, phrasingPath string, configPath string, hintIndexPath string) (*App, error) {
	db, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)

	templates, err := loadTemplates()
	if err != nil {
		db.Close()
		return nil, err
	}

	app := &App{
		db:            db,
		templates:     templates,
		hints:         NewDictionaryHints(configPath, hintIndexPath),
		phrasingPath:  phrasingPath,
		dailyNewLimit: defaultDailyNewLimit,
		now:           time.Now,
	}
	if err := app.initSchema(context.Background()); err != nil {
		db.Close()
		return nil, err
	}
	return app, nil
}

func loadTemplates() (*template.Template, error) {
	return template.New("pages").
		Funcs(template.FuncMap{"dict": templateDict}).
		ParseFS(assetFS, "templates/*.html")
}

func withoutCaching(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
		w.Header().Set("CDN-Cache-Control", "no-store")
		w.Header().Set("Pragma", "no-cache")
		w.Header().Set("Expires", "0")

		// ServeContent honors these headers with 304, 412, or partial responses.
		// Local development assets should always be read and returned in full.
		for _, header := range []string{
			"If-Match",
			"If-None-Match",
			"If-Modified-Since",
			"If-Unmodified-Since",
			"If-Range",
			"Range",
		} {
			r.Header.Del(header)
		}

		next.ServeHTTP(w, r)
	})
}

func (a *App) routes(mux *http.ServeMux) {
	staticFS, err := fs.Sub(assetFS, "static")
	if err != nil {
		panic(err)
	}

	mux.Handle("/static/", withoutCaching(http.StripPrefix("/static/", http.FileServer(http.FS(staticFS)))))
	mux.HandleFunc("/", a.handleIndex)
	mux.HandleFunc("/deck", a.handleDeck)
	mux.HandleFunc("/deck/edit", a.handleDeckEdit)
	mux.HandleFunc("/backup", a.handleBackup)
	mux.Handle("/hint", withoutCaching(http.HandlerFunc(a.handleHint)))
	mux.HandleFunc("/import", a.handleImport)
	mux.HandleFunc("/item/delete", a.handleItemDelete)
	mux.HandleFunc("/item/edit", a.handleItemEdit)
	mux.Handle("/phrasing", withoutCaching(http.HandlerFunc(a.handlePhrasingTrainer)))
	mux.Handle("/phrasing-data.json", withoutCaching(http.HandlerFunc(a.handlePhrasingData)))
	mux.HandleFunc("/session/start", a.handleSessionStart)
	mux.HandleFunc("/session/review-all-due", a.handleReviewAllDue)
	mux.HandleFunc("/session/submit", a.handleSessionSubmit)
}
