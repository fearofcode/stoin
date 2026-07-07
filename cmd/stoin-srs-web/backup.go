package main

import (
	"context"
	"database/sql"
	"net/http"
	"strings"
	"time"
)

func (a *App) handleBackup(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		methodNotAllowed(w)
		return
	}
	dump, err := a.sqlBackup(r.Context())
	if err != nil {
		serverError(w, err)
		return
	}
	filename := "stoin-srs-backup-" + time.Now().UTC().Format("20060102T150405Z") + ".sql"
	w.Header().Set("Content-Type", "application/sql; charset=utf-8")
	w.Header().Set("Content-Disposition", `attachment; filename="`+filename+`"`)
	_, _ = w.Write([]byte(dump))
}

func (a *App) sqlBackup(ctx context.Context) (string, error) {
	tx, err := a.db.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if err != nil {
		return "", err
	}
	defer tx.Rollback()

	var out strings.Builder
	out.WriteString("-- Stoin SRS SQL backup\n")
	out.WriteString("-- Generated at ")
	out.WriteString(time.Now().UTC().Format(time.RFC3339))
	out.WriteString("\n\n")
	out.WriteString("PRAGMA foreign_keys=OFF;\n")
	out.WriteString("BEGIN TRANSACTION;\n\n")

	tables, err := writeBackupSchema(ctx, tx, &out)
	if err != nil {
		return "", err
	}
	for _, table := range tables {
		if err := writeBackupTableRows(ctx, tx, table, &out); err != nil {
			return "", err
		}
	}

	out.WriteString("COMMIT;\n")
	out.WriteString("PRAGMA foreign_keys=ON;\n")
	if err := tx.Commit(); err != nil {
		return "", err
	}
	return out.String(), nil
}

func writeBackupSchema(ctx context.Context, tx *sql.Tx, out *strings.Builder) ([]string, error) {
	rows, err := tx.QueryContext(ctx, `
SELECT type, name, sql
FROM sqlite_schema
WHERE sql IS NOT NULL
  AND name NOT LIKE 'sqlite_%'
ORDER BY
  CASE type WHEN 'table' THEN 0 WHEN 'index' THEN 1 WHEN 'trigger' THEN 2 WHEN 'view' THEN 3 ELSE 4 END,
  name`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var tables []string
	for rows.Next() {
		var objectType string
		var name string
		var sqlText string
		if err := rows.Scan(&objectType, &name, &sqlText); err != nil {
			return nil, err
		}
		if objectType == "table" {
			tables = append(tables, name)
		}
		out.WriteString(sqlText)
		out.WriteString(";\n\n")
	}
	return tables, rows.Err()
}

func writeBackupTableRows(ctx context.Context, tx *sql.Tx, table string, out *strings.Builder) error {
	columns, err := backupTableColumns(ctx, tx, table)
	if err != nil {
		return err
	}
	if len(columns) == 0 {
		return nil
	}

	quotedColumns := make([]string, 0, len(columns))
	valueExpressions := make([]string, 0, len(columns))
	hasID := false
	for _, column := range columns {
		quoted := quoteSQLIdentifier(column)
		quotedColumns = append(quotedColumns, quoted)
		valueExpressions = append(valueExpressions, "quote("+quoted+")")
		if column == "id" {
			hasID = true
		}
	}

	query := "SELECT " + strings.Join(valueExpressions, ", ") + " FROM " + quoteSQLIdentifier(table)
	if hasID {
		query += " ORDER BY " + quoteSQLIdentifier("id")
	}
	rows, err := tx.QueryContext(ctx, query)
	if err != nil {
		return err
	}
	defer rows.Close()

	values := make([]sql.NullString, len(columns))
	dest := make([]any, len(columns))
	for i := range values {
		dest[i] = &values[i]
	}
	wroteHeader := false
	for rows.Next() {
		if err := rows.Scan(dest...); err != nil {
			return err
		}
		if !wroteHeader {
			out.WriteString("-- Data for ")
			out.WriteString(quoteSQLIdentifier(table))
			out.WriteString("\n")
			wroteHeader = true
		}
		literals := make([]string, len(values))
		for i, value := range values {
			if value.Valid {
				literals[i] = value.String
			} else {
				literals[i] = "NULL"
			}
		}
		out.WriteString("INSERT INTO ")
		out.WriteString(quoteSQLIdentifier(table))
		out.WriteString("(")
		out.WriteString(strings.Join(quotedColumns, ", "))
		out.WriteString(") VALUES(")
		out.WriteString(strings.Join(literals, ", "))
		out.WriteString(");\n")
	}
	if err := rows.Err(); err != nil {
		return err
	}
	if wroteHeader {
		out.WriteString("\n")
	}
	return nil
}

func backupTableColumns(ctx context.Context, tx *sql.Tx, table string) ([]string, error) {
	rows, err := tx.QueryContext(ctx, "PRAGMA table_info("+quoteSQLIdentifier(table)+")")
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var columns []string
	for rows.Next() {
		var cid int
		var name string
		var typeName string
		var notNull int
		var defaultValue sql.NullString
		var primaryKey int
		if err := rows.Scan(&cid, &name, &typeName, &notNull, &defaultValue, &primaryKey); err != nil {
			return nil, err
		}
		columns = append(columns, name)
	}
	return columns, rows.Err()
}

func quoteSQLIdentifier(name string) string {
	return `"` + strings.ReplaceAll(name, `"`, `""`) + `"`
}
