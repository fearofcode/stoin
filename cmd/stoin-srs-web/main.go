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

	_ "modernc.org/sqlite"
)

const (
	defaultDBPath       = "stoin-srs-web.sqlite3"
	defaultAddr         = "127.0.0.1:8080"
	introRepetitions    = 5
	reviewAllDueLimit   = 100
	sessionLineMaxRunes = 52
	maxRequestBodyBytes = 4 << 20
)

var scheduleDays = []int{1, 3, 7, 14, 30, 60, 120, 240}

//go:embed templates/*.html static/*.css static/*.js
var assetFS embed.FS

type App struct {
	db        *sql.DB
	templates *template.Template
}

func main() {
	dbPath := flag.String("db", defaultDBPath, "SQLite database path")
	addr := flag.String("addr", defaultAddr, "HTTP listen address")
	flag.Parse()

	app, err := NewApp(*dbPath)
	if err != nil {
		log.Fatal(err)
	}
	defer app.db.Close()

	mux := http.NewServeMux()
	app.routes(mux)

	log.Printf("stoin-srs-web listening on http://%s", *addr)
	log.Fatal(http.ListenAndServe(*addr, mux))
}

func NewApp(dbPath string) (*App, error) {
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
		db:        db,
		templates: templates,
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

func (a *App) routes(mux *http.ServeMux) {
	staticFS, err := fs.Sub(assetFS, "static")
	if err != nil {
		panic(err)
	}

	mux.Handle("/static/", http.StripPrefix("/static/", http.FileServer(http.FS(staticFS))))
	mux.HandleFunc("/", a.handleIndex)
	mux.HandleFunc("/deck", a.handleDeck)
	mux.HandleFunc("/import", a.handleImport)
	mux.HandleFunc("/phrasing", a.handlePhrasingTrainer)
	mux.HandleFunc("/session/start", a.handleSessionStart)
	mux.HandleFunc("/session/review-all-due", a.handleReviewAllDue)
	mux.HandleFunc("/session/submit", a.handleSessionSubmit)
}
