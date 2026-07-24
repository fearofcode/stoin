package main

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestLoadWordFrequencyDataDownloadsAndCaches(t *testing.T) {
	cachePath := filepath.Join(t.TempDir(), "count_1w.txt")
	t.Setenv(wordFrequencyCacheEnvironment, cachePath)

	requestCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(response http.ResponseWriter, request *http.Request) {
		requestCount++
		response.Header().Set("Content-Type", "text/plain")
		_, _ = response.Write([]byte("common\t20\nrare\t1\n"))
	}))
	defer server.Close()

	previousURL := wordFrequencyURL
	previousClient := wordFrequencyHTTPClient
	wordFrequencyURL = server.URL
	wordFrequencyHTTPClient = server.Client()
	t.Cleanup(func() {
		wordFrequencyURL = previousURL
		wordFrequencyHTTPClient = previousClient
	})

	var stderr bytes.Buffer
	data, err := loadWordFrequencyData(&stderr)
	if err != nil {
		t.Fatalf("first loadWordFrequencyData: %v", err)
	}
	if data != "common\t20\nrare\t1\n" {
		t.Fatalf("downloaded frequency data = %q", data)
	}
	if requestCount != 1 {
		t.Fatalf("download requests = %d, want 1", requestCount)
	}
	if !strings.Contains(stderr.String(), server.URL) || !strings.Contains(stderr.String(), cachePath) {
		t.Fatalf("download notice = %q, want URL and cache path", stderr.String())
	}

	data, err = loadWordFrequencyData(&stderr)
	if err != nil {
		t.Fatalf("cached loadWordFrequencyData: %v", err)
	}
	if data != "common\t20\nrare\t1\n" {
		t.Fatalf("cached frequency data = %q", data)
	}
	if requestCount != 1 {
		t.Fatalf("download requests after cached load = %d, want 1", requestCount)
	}
}

func TestLoadWordFrequencyDataReportsDownloadFailure(t *testing.T) {
	cachePath := filepath.Join(t.TempDir(), "count_1w.txt")
	t.Setenv(wordFrequencyCacheEnvironment, cachePath)

	server := httptest.NewServer(http.HandlerFunc(func(response http.ResponseWriter, request *http.Request) {
		http.Error(response, "unavailable", http.StatusServiceUnavailable)
	}))
	defer server.Close()

	previousURL := wordFrequencyURL
	previousClient := wordFrequencyHTTPClient
	wordFrequencyURL = server.URL
	wordFrequencyHTTPClient = server.Client()
	t.Cleanup(func() {
		wordFrequencyURL = previousURL
		wordFrequencyHTTPClient = previousClient
	})

	_, err := loadWordFrequencyData(io.Discard)
	if err == nil || !strings.Contains(err.Error(), "503 Service Unavailable") {
		t.Fatalf("loadWordFrequencyData error = %v, want HTTP status", err)
	}
	if _, statErr := os.Stat(cachePath); !os.IsNotExist(statErr) {
		t.Fatalf("failed download cache stat error = %v, want not-exist", statErr)
	}
}

func TestGZPluralAddsSOnlyToSimpleNFinalTranslations(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "OEGZ", value: "ocean"},
		{outline: "RAEUZ", value: "raise"},
		{outline: "STAEUPBLGZ", value: "stages"},
		{outline: "STEUGZ", value: "{^stition}"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	claims := generateCandidateClaims(sources, nil, nil)
	if claim := claims["OEGSZ"]; claim == nil || claim.conflict || claim.value != "oceans" {
		t.Fatalf("OEGSZ claim = %#v, want oceans", claim)
	}
	for _, outline := range []string{"RAEUSZ", "STAEUPBLGSZ", "STEUGSZ"} {
		if claim := claims[outline]; claim != nil {
			t.Fatalf("unexpected %s claim = %#v", outline, claim)
		}
	}
}

func TestGZPluralDoesNotReplaceExistingOutline(t *testing.T) {
	outline, ok := parseOutline("OEGZ")
	if !ok {
		t.Fatal("parseOutline(\"OEGZ\") failed")
	}
	pluralOutline, ok := parseOutline("OEGSZ")
	if !ok {
		t.Fatal("parseOutline(\"OEGSZ\") failed")
	}
	sources := map[string]sourceEntry{
		"OEGZ":  {outline: outline, value: "ocean"},
		"OEGSZ": {outline: pluralOutline, value: "occupied"},
	}

	if claim := generateCandidateClaims(sources, nil, nil)["OEGSZ"]; claim != nil {
		t.Fatalf("occupied OEGSZ claim = %#v, want none", claim)
	}
}

func TestTwoWayConflictUsesFrequencyAndStarredAlternative(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "POEGZ", value: "potion"},
		{outline: "POESZ/-G", value: "possessing"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	claims := generateCandidateClaims(sources, nil, nil)
	claim := claims["POEGSZ"]
	if claim == nil || !claim.conflict || len(claim.alternatives) != 1 {
		t.Fatalf("POEGSZ claim = %#v, want a two-way conflict", claim)
	}
	frequencies := map[string]uint64{
		"possessing": 2,
		"potions":    1,
	}
	additional, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(
		sources,
		claims,
		nil,
		frequencies,
	)
	if ambiguousCount != 0 || joinCount != 0 || boundaryCount != 0 {
		t.Fatalf(
			"resolved conflict counts = (%d, %d, %d), want (0, 0, 0)",
			ambiguousCount,
			joinCount,
			boundaryCount,
		)
	}
	if got := additional["POEGSZ"]; got != "possessing" {
		t.Fatalf("POEGSZ = %q, want possessing", got)
	}
	if got := additional["PO*EGSZ"]; got != "potions" {
		t.Fatalf("PO*EGSZ = %q, want potions", got)
	}
	if got := additional["POEGSZ/R-R"]; got != "potions" {
		t.Fatalf("POEGSZ/R-R = %q, want potions", got)
	}
}

func TestTwoWayConflictFallsBackToLongerTranslation(t *testing.T) {
	outline, ok := parseOutline("KAT")
	if !ok {
		t.Fatal("parseOutline(\"KAT\") failed")
	}
	claims := map[string]*candidateClaim{
		"KAT": {
			outline:      outline,
			value:        "the",
			alternatives: []string{"definitely-not-in-count-1w"},
			conflict:     true,
		},
	}

	additional, ambiguousCount, _, _ := selectSafeCandidates(nil, claims, nil, nil)
	if ambiguousCount != 0 {
		t.Fatalf("ambiguous count = %d, want 0", ambiguousCount)
	}
	if got := additional["KAT"]; got != "definitely-not-in-count-1w" {
		t.Fatalf("KAT = %q, want longer unlisted translation", got)
	}
	if got := additional["KA*T"]; got != "the" {
		t.Fatalf("KA*T = %q, want the", got)
	}
	if got := additional["KAT/R-R"]; got != "the" {
		t.Fatalf("KAT/R-R = %q, want the", got)
	}
}

func TestTwoWayConflictPrefersMatchingReferenceValue(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "PU/HRAEUD", value: "pallad"},
		{outline: "PHRAEU/-D", value: "played"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	claims := generateCandidateClaims(sources, nil, nil)
	claim := claims["PHRAEUD"]
	if claim == nil || !claim.conflict || len(claim.alternatives) != 1 {
		t.Fatalf("PHRAEUD claim = %#v, want a two-way conflict", claim)
	}
	preferenceKey, preference := entry("PHRAEUD", "played")
	preferences := map[string]sourceEntry{preferenceKey: preference}

	additional, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(
		sources,
		claims,
		preferences,
		nil,
	)
	if ambiguousCount != 0 || joinCount != 0 || boundaryCount != 0 {
		t.Fatalf(
			"preferred conflict counts = (%d, %d, %d), want (0, 0, 0)",
			ambiguousCount,
			joinCount,
			boundaryCount,
		)
	}
	if got := additional["PHRAEUD"]; got != "played" {
		t.Fatalf("PHRAEUD = %q, want played", got)
	}
	if got := additional["PHRA*EUD"]; got != "pallad" {
		t.Fatalf("PHRA*EUD = %q, want pallad", got)
	}
	if got := additional["PHRAEUD/R-R"]; got != "pallad" {
		t.Fatalf("PHRAEUD/R-R = %q, want pallad", got)
	}
}

func TestRunLoadsPreferenceDictionary(t *testing.T) {
	directory := t.TempDir()
	sourcePath := filepath.Join(directory, "source.json")
	preferencePath := filepath.Join(directory, "preference.json")
	outputPath := filepath.Join(directory, "augmentations.json")
	if err := os.WriteFile(
		sourcePath,
		[]byte(`{"PHRAEU":"play","PU/HRAEUD":"pallad"}`),
		0o644,
	); err != nil {
		t.Fatalf("write source dictionary: %v", err)
	}
	if err := os.WriteFile(preferencePath, []byte(`{"PHRAEUD":"played"}`), 0o644); err != nil {
		t.Fatalf("write preference dictionary: %v", err)
	}

	var stdout bytes.Buffer
	var stderr bytes.Buffer
	if err := run(
		outputPath,
		[]string{sourcePath},
		nil,
		nil,
		[]string{preferencePath},
		&stdout,
		&stderr,
	); err != nil {
		t.Fatalf("run: %v", err)
	}
	contents, err := os.ReadFile(outputPath)
	if err != nil {
		t.Fatalf("read output dictionary: %v", err)
	}
	var augmentations map[string]string
	if err := json.Unmarshal(contents, &augmentations); err != nil {
		t.Fatalf("parse output dictionary: %v", err)
	}
	if got := augmentations["PHRAEUD"]; got != "played" {
		t.Fatalf("PHRAEUD = %q, want played", got)
	}
	if got := augmentations["PHRA*EUD"]; got != "pallad" {
		t.Fatalf("PHRA*EUD = %q, want pallad", got)
	}
	if got := augmentations["PHRAEUD/R-R"]; got != "pallad" {
		t.Fatalf("PHRAEUD/R-R = %q, want pallad", got)
	}
}

func TestDerivationSeedsRequireMissingTranslationAndSharedPrimaryPrefix(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "SKWRABG", value: "jack"},
		{outline: "SKWRABGZ", value: "jacks"},
		{outline: "KAT", value: "cat"},
		{outline: "KA/TO", value: "cater"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	derivations := make(map[string]sourceEntry)
	for _, derivation := range []struct {
		outline string
		value   string
	}{
		{outline: "SKWRABG/-G", value: "jacking"},
		{outline: "SKWRABG/-Z", value: "jacks"},
		{outline: "KAT/-G", value: "dogging"},
		{outline: "KA/TO/-G", value: "catering"},
	} {
		key, parsed := entry(derivation.outline, derivation.value)
		derivations[key] = parsed
	}

	seeds, stats := selectDerivationSeeds(sources, derivations, nil)
	if stats.eligibleSeeds != 1 || stats.translationPresent != 1 {
		t.Fatalf("derivation stats = %#v, want one eligible and one present translation", stats)
	}
	if len(seeds) != 1 || seeds["SKWRABG/G"].value != "jacking" {
		t.Fatalf("derivation seeds = %#v, want only SKWRABG/-G jacking", seeds)
	}

	claims := make(map[string]*candidateClaim)
	addDerivedSuffixClaims(sources, seeds, claims)
	if claim := claims["SKWRABGDZ"]; claim == nil || claim.value != "jacking" {
		t.Fatalf("SKWRABGDZ claim = %#v, want jacking", claim)
	}
	if _, imported := claims["SKWRABG/G"]; imported {
		t.Fatal("derivation source outline was imported instead of only seeding folds")
	}
}

func TestRunLoadsDerivationDictionary(t *testing.T) {
	directory := t.TempDir()
	sourcePath := filepath.Join(directory, "source.json")
	derivationPath := filepath.Join(directory, "derivation.json")
	outputPath := filepath.Join(directory, "augmentations.json")
	if err := os.WriteFile(
		sourcePath,
		[]byte(`{"KAG":"cat","KAG/DZ":"primary","SKWRABG":"jack"}`),
		0o644,
	); err != nil {
		t.Fatalf("write source dictionary: %v", err)
	}
	if err := os.WriteFile(
		derivationPath,
		[]byte(`{"KAG/-G":"catting","SKWRABG/-G":"jacking"}`),
		0o644,
	); err != nil {
		t.Fatalf("write derivation dictionary: %v", err)
	}

	var stdout bytes.Buffer
	var stderr bytes.Buffer
	if err := run(
		outputPath,
		[]string{sourcePath},
		nil,
		[]string{derivationPath},
		nil,
		&stdout,
		&stderr,
	); err != nil {
		t.Fatalf("run: %v", err)
	}
	contents, err := os.ReadFile(outputPath)
	if err != nil {
		t.Fatalf("read output dictionary: %v", err)
	}
	var augmentations map[string]string
	if err := json.Unmarshal(contents, &augmentations); err != nil {
		t.Fatalf("parse output dictionary: %v", err)
	}
	if got := augmentations["SKWRABGDZ"]; got != "jacking" {
		t.Fatalf("SKWRABGDZ = %q, want jacking", got)
	}
	if got := augmentations["KAGDZ"]; got != "primary" {
		t.Fatalf("KAGDZ = %q, want accepted primary augmentation", got)
	}
	if _, imported := augmentations["SKWRABG/G"]; imported {
		t.Fatal("derivation source outline was written to the output")
	}
}

func TestTwoWayConflictRepeatsRROutlineUntilAvailable(t *testing.T) {
	outline, ok := parseOutline("KAT")
	if !ok {
		t.Fatal("parseOutline(\"KAT\") failed")
	}
	occupiedRR, ok := parseOutline("KAT/R-R")
	if !ok {
		t.Fatal("parseOutline(\"KAT/R-R\") failed")
	}
	sources := map[string]sourceEntry{
		"KAT/R-R": {outline: occupiedRR, value: "occupied"},
	}
	claims := map[string]*candidateClaim{
		"KAT": {
			outline:      outline,
			value:        "first",
			alternatives: []string{"longer second"},
			conflict:     true,
		},
	}

	additional, ambiguousCount, _, boundaryCount := selectSafeCandidates(sources, claims, nil, nil)
	if ambiguousCount != 0 || boundaryCount != 0 {
		t.Fatalf(
			"repeated R-R conflict counts = (%d, %d), want (0, 0)",
			ambiguousCount,
			boundaryCount,
		)
	}
	if _, generated := additional["KAT/R-R"]; generated {
		t.Fatal("occupied KAT/R-R was replaced")
	}
	if got := additional["KAT/R-R/R-R"]; got != "first" {
		t.Fatalf("KAT/R-R/R-R = %q, want first", got)
	}
}

func TestTwoWayConflictStaysAmbiguousWhenStarredOutlineIsOccupied(t *testing.T) {
	outline, ok := parseOutline("KAT")
	if !ok {
		t.Fatal("parseOutline(\"KAT\") failed")
	}
	starred, ok := parseOutline("KA*T")
	if !ok {
		t.Fatal("parseOutline(\"KA*T\") failed")
	}
	sources := map[string]sourceEntry{
		"KA*T": {outline: starred, value: "occupied"},
	}
	claims := map[string]*candidateClaim{
		"KAT": {
			outline:      outline,
			value:        "first",
			alternatives: []string{"second"},
			conflict:     true,
		},
	}

	additional, ambiguousCount, _, _ := selectSafeCandidates(sources, claims, nil, nil)
	if ambiguousCount != 1 {
		t.Fatalf("ambiguous count = %d, want 1", ambiguousCount)
	}
	if _, generated := additional["KAT"]; generated {
		t.Fatal("unstarred conflict was generated despite occupied starred outline")
	}
}

func TestLeadingCollapseRequiresCompleteTwoStrokeOutline(t *testing.T) {
	tests := []struct {
		name    string
		outline string
		want    string
		ok      bool
	}{
		{
			name:    "torrential",
			outline: "TOR/EPBLGS",
			want:    "TREPBLGS",
			ok:      true,
		},
		{
			name:    "alternate torrential split",
			outline: "TU/REPBLGS",
			want:    "TREPBLGS",
			ok:      true,
		},
		{
			name:    "partial jai alai collapse",
			outline: "HEU/U/HRAOEU",
			ok:      false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			outline, parsed := parseOutline(test.outline)
			if !parsed {
				t.Fatalf("parseOutline(%q) failed", test.outline)
			}
			got, ok := collapseLeadingConsonantVowelStroke(outline)
			if ok != test.ok {
				t.Fatalf("collapseLeadingConsonantVowelStroke(%q) ok = %v, want %v", test.outline, ok, test.ok)
			}
			if !ok {
				return
			}
			if gotOutline := formatOutline(got); gotOutline != test.want {
				t.Fatalf("collapseLeadingConsonantVowelStroke(%q) = %q, want %q", test.outline, gotOutline, test.want)
			}
		})
	}
}

func TestLeadingCollapseDoesNotShadowUnrelatedFinalDBase(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "PHRAEUPB", value: "plain"},
		{outline: "PU/HRAEUD/KWR-PB", value: "Palladian"},
		{outline: "PO/HRAEUR/-S", value: "Polaris"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	claims := generateCandidateClaims(sources, nil, nil)
	if _, generated := claims["PHRAEUPBD"]; generated {
		t.Fatal("Palladian leading collapse shadowed plain + final -D")
	}
	if claim := claims["PU/HRAEUPBD"]; claim == nil || claim.value != "Palladian" {
		t.Fatalf("intermediate Palladian fold = %#v, want PU/HRAEUPBD", claim)
	}
	if claim := claims["PHRAEURS"]; claim == nil || claim.value != "Polaris" {
		t.Fatalf("ordinary Polaris collapse = %#v, want Polaris", claim)
	}

	planedOutline, ok := parseOutline("PHRAEUPBD")
	if !ok {
		t.Fatal("parseOutline(\"PHRAEUPBD\") failed")
	}
	if shadowsUnrelatedFinalDBase(planedOutline, "planed", sources) {
		t.Fatal("related planed translation was treated as shadowing the plain stem")
	}
}

func TestTrailingDropDoesNotShadowJoinedComposition(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "HU/HRAOUS", value: "{halluc^}"},
		{outline: "TPHAEUT", value: "{^inate}"},
		{outline: "HU/HRAOUS/TPHAEUT/-FB", value: "hallucinative"},
		{outline: "PAT/RU/KHRAOEUPB/-S", value: "patriclinous"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	claims := generateCandidateClaims(sources, nil, nil)
	if _, generated := claims["HU/HRAOUS/TPHAEUT"]; generated {
		t.Fatal("trailing drop shadowed {halluc^} + {^inate}")
	}
	if claim := claims["PAT/RU/KHRAOEUPB"]; claim == nil || claim.value != "patriclinous" {
		t.Fatalf("ordinary trailing drop = %#v, want patriclinous", claim)
	}

	prefixOnlySources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "KA/TO", value: "{cat^}"},
		{outline: "PWU", value: "boo"},
	} {
		key, parsed := entry(source.outline, source.value)
		prefixOnlySources[key] = parsed
	}
	candidate, ok := parseOutline("KA/TO/PWU")
	if !ok {
		t.Fatal("parseOutline for prefix-only join fixture failed")
	}
	if shadowsJoinedFinalStroke(candidate, prefixOnlySources) {
		t.Fatal("prefix-only join should not trigger the final-stroke suffix guard")
	}
}

func TestFoldVowelCodaStrokeRequiresMatchingVowelsOrPluralPSes(t *testing.T) {
	tests := []struct {
		name     string
		outline  string
		boundary int
		want     string
		ok       bool
	}{
		{
			name:     "matching vowel bank",
			outline:  "AEUR/AEUGZ",
			boundary: 0,
			want:     "AEURGZ",
			ok:       true,
		},
		{
			name:     "mismatch would collapse deniably",
			outline:  "TK/TPHAOEU/AEBL",
			boundary: 1,
			ok:       false,
		},
		{
			name:     "plural pses preserves ellipses",
			outline:  "HREUPS/AEZ",
			boundary: 0,
			want:     "HREUPSZ",
			ok:       true,
		},
		{
			name:     "other new vowels are rejected",
			outline:  "ABG/SES/AEBL",
			boundary: 1,
			ok:       false,
		},
		{
			name:     "plural exception rejects divorcees collapse",
			outline:  "SRORS/AEZ",
			boundary: 0,
			ok:       false,
		},
		{
			name:     "plural exception requires exact ps coda",
			outline:  "HREUPLS/AEZ",
			boundary: 0,
			ok:       false,
		},
		{
			name:     "star changes preceding vowel bank",
			outline:  "AE*UR/AEUGZ",
			boundary: 0,
			ok:       false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			outline, parsed := parseOutline(test.outline)
			if !parsed {
				t.Fatalf("parseOutline(%q) failed", test.outline)
			}
			got, ok := foldVowelCodaStroke(outline, test.boundary)
			if ok != test.ok {
				t.Fatalf("foldVowelCodaStroke(%q, %d) ok = %v, want %v", test.outline, test.boundary, ok, test.ok)
			}
			if !ok {
				return
			}
			if gotOutline := formatOutline(got); gotOutline != test.want {
				t.Fatalf("foldVowelCodaStroke(%q, %d) = %q, want %q", test.outline, test.boundary, gotOutline, test.want)
			}
		})
	}
}

func TestOmitSecondConsonantVowelStroke(t *testing.T) {
	tests := []struct {
		name    string
		outline string
		want    string
		ok      bool
	}{
		{
			name:    "cosmopolitan",
			outline: "KAUZ/PHO/PAUL/T-PB",
			want:    "KAUZ/PAUL/T-PB",
			ok:      true,
		},
		{
			name:    "cosmetology",
			outline: "KAUZ/PHU/TAULG",
			want:    "KAUZ/TAULG",
			ok:      true,
		},
		{
			name:    "requires three strokes",
			outline: "KAUZ/PHU",
			ok:      false,
		},
		{
			name:    "requires consonants",
			outline: "KAUZ/AOU/TAULG",
			ok:      false,
		},
		{
			name:    "requires vowels",
			outline: "KAUZ/PH/TAULG",
			ok:      false,
		},
		{
			name:    "rejects right hand coda",
			outline: "KAUZ/PHUB/TAULG",
			ok:      false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			outline, parsed := parseOutline(test.outline)
			if !parsed {
				t.Fatalf("parseOutline(%q) failed", test.outline)
			}
			got, ok := omitSecondConsonantVowelStroke(outline)
			if ok != test.ok {
				t.Fatalf("omitSecondConsonantVowelStroke(%q) ok = %v, want %v", test.outline, ok, test.ok)
			}
			if !ok {
				return
			}
			if gotOutline := formatOutline(got); gotOutline != test.want {
				t.Fatalf("omitSecondConsonantVowelStroke(%q) = %q, want %q", test.outline, gotOutline, test.want)
			}
		})
	}
}

func TestOmitInteriorKWRVowelStroke(t *testing.T) {
	tests := []struct {
		name        string
		outline     string
		strokeIndex int
		want        string
		ok          bool
	}{
		{
			name:        "diminutive",
			outline:     "TKEU/PHEUPB/KWRU/T-FB",
			strokeIndex: 2,
			want:        "TKEU/PHEUPB/TFB",
			ok:          true,
		},
		{
			name:        "second stroke",
			outline:     "STKOES/KWRO/TKPWR-F",
			strokeIndex: 1,
			want:        "STKOES/TKPWRF",
			ok:          true,
		},
		{
			name:        "rejects first stroke",
			outline:     "KWRU/PHEUPB/T-FB",
			strokeIndex: 0,
			ok:          false,
		},
		{
			name:        "rejects final stroke",
			outline:     "TKEU/PHEUPB/KWRU",
			strokeIndex: 2,
			ok:          false,
		},
		{
			name:        "requires vowels",
			outline:     "TKEU/KWR/T-FB",
			strokeIndex: 1,
			ok:          false,
		},
		{
			name:        "requires exact linker",
			outline:     "TKEU/KWU/T-FB",
			strokeIndex: 1,
			ok:          false,
		},
		{
			name:        "rejects right hand coda",
			outline:     "TKEU/KWRUB/T-FB",
			strokeIndex: 1,
			ok:          false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			outline, parsed := parseOutline(test.outline)
			if !parsed {
				t.Fatalf("parseOutline(%q) failed", test.outline)
			}
			got, ok := omitInteriorKWRVowelStroke(outline, test.strokeIndex)
			if ok != test.ok {
				t.Fatalf("omitInteriorKWRVowelStroke(%q, %d) ok = %v, want %v", test.outline, test.strokeIndex, ok, test.ok)
			}
			if !ok {
				return
			}
			if gotOutline := formatOutline(got); gotOutline != test.want {
				t.Fatalf("omitInteriorKWRVowelStroke(%q, %d) = %q, want %q", test.outline, test.strokeIndex, gotOutline, test.want)
			}
		})
	}
}

func TestSupplementalEntriesUseSafetyChecks(t *testing.T) {
	entry := func(rawOutline, value string) (string, sourceEntry) {
		outline, ok := parseOutline(rawOutline)
		if !ok {
			t.Fatalf("parseOutline(%q) failed", rawOutline)
		}
		key := formatOutline(outline)
		return key, sourceEntry{outline: outline, value: value}
	}

	sources := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "KA", value: "ca"},
		{outline: "TO", value: "to"},
		{outline: "STAB", value: "stab"},
	} {
		key, parsed := entry(source.outline, source.value)
		sources[key] = parsed
	}

	supplemental := make(map[string]sourceEntry)
	for _, source := range []struct {
		outline string
		value   string
	}{
		{outline: "AUBLGS", value: "auxiliary"},
		{outline: "AEURGZ", value: "airings"},
		{outline: "STAB", value: "different"},
		{outline: "KA/TO", value: "cat"},
		{outline: "KA/P-P", value: "join marker"},
		{outline: "HREURLT", value: "literal"},
		{outline: "HREURLT/R-R", value: "literal"},
	} {
		key, parsed := entry(source.outline, source.value)
		supplemental[key] = parsed
	}

	aublgsKey, aublgsEntry := entry("AUBLGS", "not auxiliary")
	aeurgzKey, aeurgzEntry := entry("AEURGZ", "aeration")
	claims := map[string]*candidateClaim{
		aublgsKey: {
			outline:  aublgsEntry.outline,
			value:    aublgsEntry.value,
			conflict: true,
		},
		aeurgzKey: {
			outline: aeurgzEntry.outline,
			value:   aeurgzEntry.value,
		},
	}
	generated, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(sources, claims, nil, nil)
	if ambiguousCount != 1 || joinCount != 0 || boundaryCount != 0 {
		t.Fatalf("generated safety counts = (%d, %d, %d), want (1, 0, 0)", ambiguousCount, joinCount, boundaryCount)
	}
	retainAcceptedClaims(claims, generated)

	stats, supplementalKeys := addSupplementalClaims(sources, supplemental, claims, rrMarkedTranslations(supplemental))
	if stats.exactPrimaryOverlaps != 1 {
		t.Fatalf("exact primary overlaps = %d, want 1", stats.exactPrimaryOverlaps)
	}
	if stats.exactGeneratedOverlaps != 1 {
		t.Fatalf("exact generated overlaps = %d, want 1", stats.exactGeneratedOverlaps)
	}
	if stats.rrExcluded != 2 {
		t.Fatalf("R-R exclusions = %d, want 2", stats.rrExcluded)
	}
	if len(supplementalKeys) != 3 {
		t.Fatalf("supplemental candidates = %d, want 3", len(supplementalKeys))
	}

	additional, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(sources, claims, nil, nil)
	if ambiguousCount != 0 {
		t.Fatalf("ambiguous candidates = %d, want 0", ambiguousCount)
	}
	if joinCount != 1 {
		t.Fatalf("trailing P-P candidates = %d, want 1", joinCount)
	}
	if boundaryCount != 1 {
		t.Fatalf("boundary conflicts = %d, want 1", boundaryCount)
	}
	if len(additional) != 2 || additional["AUBLGS"] != "auxiliary" || additional["AEURGZ"] != "aeration" {
		t.Fatalf("safe combined entries = %#v, want supplemental AUBLGS and generated AEURGZ", additional)
	}
}
