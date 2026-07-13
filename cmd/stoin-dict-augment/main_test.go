package main

import "testing"

func TestFoldVowelCodaStrokeRequiresMatchingVowels(t *testing.T) {
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
			name:     "different vowel bank",
			outline:  "TK/TPHAOEU/AEBL",
			boundary: 1,
			ok:       false,
		},
		{
			name:     "subset is not enough",
			outline:  "STAOE/AEGZ",
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
	generated, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(sources, claims)
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

	additional, ambiguousCount, joinCount, boundaryCount := selectSafeCandidates(sources, claims)
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
