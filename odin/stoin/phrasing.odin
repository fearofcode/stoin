package stoin

import "core:encoding/json"
import "core:os"

Fv_Agreement :: enum {
	First_Singular,
	Third_Singular,
	Plural,
}

Fv_Modal :: enum {
	None,
	Can,
	Should,
	Will,
}

Fv_Structure :: enum {
	Simple,
	Progressive,
	Perfect,
	Perfect_Progressive,
}

Fv_Verb_Kind :: enum {
	Other,
	Be,
	Have,
	Do,
}

Phrase_Form :: struct {
	bits: u64,
	text: string,
}

Iv_Stem :: struct {
	bits:  u64,
	forms: [dynamic]Phrase_Form,
}

Phrase_Tail :: struct {
	id:   string,
	bits: u64,
	text: string,
}

Nv_Prefix :: struct {
	bits:     u64,
	text:     string,
	tail_ids: [dynamic]string,
}

Fv_Starter :: struct {
	bits:             u64,
	agreement:        Fv_Agreement,
	text:             string,
	be_contraction:   string,
	have_contraction: string,
	will_contraction: string,
}

Fv_Operator :: struct {
	bits:     u64,
	modal:    Fv_Modal,
	negative: bool,
}

Fv_Structure_Row :: struct {
	bits:      u64,
	structure: Fv_Structure,
}

Fv_Verb :: struct {
	id:                 string,
	kind:               Fv_Verb_Kind,
	base:               string,
	third:              string,
	past:               string,
	present_participle: string,
	past_participle:    string,
}

Fv_Ender :: struct {
	bits:       u64,
	verb_id:    string,
	verb_index: int,
	suffix:     string,
	past:       bool,
}

Phrasing :: struct {
	iv_stems:         [dynamic]Iv_Stem,
	iv_tails:         [dynamic]Phrase_Tail,
	nv_prefixes:      [dynamic]Nv_Prefix,
	nv_tails:         [dynamic]Phrase_Tail,
	fv_starters:      [dynamic]Fv_Starter,
	fv_operators:     [dynamic]Fv_Operator,
	fv_structures:    [dynamic]Fv_Structure_Row,
	fv_verbs:         [dynamic]Fv_Verb,
	fv_enders:        [dynamic]Fv_Ender,
	contraction_bits: u64,
}

Raw_Phrase_Form :: struct {
	stroke: string,
	text:   string,
}

Raw_Iv_Stem :: struct {
	stroke: string,
	forms:  [dynamic]Raw_Phrase_Form,
}

Raw_Phrase_Tail :: struct {
	id:     string,
	stroke: string,
	text:   string,
}

Raw_Nv_Prefix :: struct {
	stroke: string,
	text:   string,
	tails:  [dynamic]string,
}

Raw_Initial_Verbs :: struct {
	tails: [dynamic]Raw_Phrase_Tail,
	stems: [dynamic]Raw_Iv_Stem,
}

Raw_Nonverbs :: struct {
	tails:    [dynamic]Raw_Phrase_Tail,
	prefixes: [dynamic]Raw_Nv_Prefix,
}

Raw_Fv_Starter :: struct {
	stroke:           string,
	text:             string,
	agreement:        string,
	be_contraction:   string,
	have_contraction: string,
	will_contraction: string,
}

Raw_Fv_Operator :: struct {
	stroke:   string,
	modal:    string,
	negative: bool,
}

Raw_Fv_Structure :: struct {
	stroke: string,
	kind:   string,
}

Raw_Fv_Verb :: struct {
	id:                 string,
	base:               string,
	third:              string,
	past:               string,
	present_participle: string,
	past_participle:    string,
}

Raw_Fv_Ender :: struct {
	stroke: string,
	verb:   string,
	suffix: string,
	past:   bool,
}

Raw_Final_Verbs :: struct {
	contraction_stroke: string,
	starters:           [dynamic]Raw_Fv_Starter,
	operators:          [dynamic]Raw_Fv_Operator,
	structures:         [dynamic]Raw_Fv_Structure,
	verbs:              [dynamic]Raw_Fv_Verb,
	enders:             [dynamic]Raw_Fv_Ender,
}

Raw_Phrasing :: struct {
	initial_verbs: Raw_Initial_Verbs,
	nonverbs:      Raw_Nonverbs,
	final_verbs:   Raw_Final_Verbs,
}

json_string_delete :: proc(text: string) {
	if len(text) > 0 {
		delete(text)
	}
}

raw_phrasing_destroy :: proc(raw: ^Raw_Phrasing) {
	for tail in raw.initial_verbs.tails {
		json_string_delete(tail.id)
		json_string_delete(tail.stroke)
		json_string_delete(tail.text)
	}
	delete(raw.initial_verbs.tails)
	for stem in raw.initial_verbs.stems {
		json_string_delete(stem.stroke)
		for form in stem.forms {
			json_string_delete(form.stroke)
			json_string_delete(form.text)
		}
		delete(stem.forms)
	}
	delete(raw.initial_verbs.stems)

	for tail in raw.nonverbs.tails {
		json_string_delete(tail.id)
		json_string_delete(tail.stroke)
		json_string_delete(tail.text)
	}
	delete(raw.nonverbs.tails)
	for prefix in raw.nonverbs.prefixes {
		json_string_delete(prefix.stroke)
		json_string_delete(prefix.text)
		for tail_id in prefix.tails {
			json_string_delete(tail_id)
		}
		delete(prefix.tails)
	}
	delete(raw.nonverbs.prefixes)

	json_string_delete(raw.final_verbs.contraction_stroke)
	for starter in raw.final_verbs.starters {
		json_string_delete(starter.stroke)
		json_string_delete(starter.text)
		json_string_delete(starter.agreement)
		json_string_delete(starter.be_contraction)
		json_string_delete(starter.have_contraction)
		json_string_delete(starter.will_contraction)
	}
	delete(raw.final_verbs.starters)
	for operator in raw.final_verbs.operators {
		json_string_delete(operator.stroke)
		json_string_delete(operator.modal)
	}
	delete(raw.final_verbs.operators)
	for structure in raw.final_verbs.structures {
		json_string_delete(structure.stroke)
		json_string_delete(structure.kind)
	}
	delete(raw.final_verbs.structures)
	for verb in raw.final_verbs.verbs {
		json_string_delete(verb.id)
		json_string_delete(verb.base)
		json_string_delete(verb.third)
		json_string_delete(verb.past)
		json_string_delete(verb.present_participle)
		json_string_delete(verb.past_participle)
	}
	delete(raw.final_verbs.verbs)
	for ender in raw.final_verbs.enders {
		json_string_delete(ender.stroke)
		json_string_delete(ender.verb)
		json_string_delete(ender.suffix)
	}
	delete(raw.final_verbs.enders)
	raw^ = {}
}

phrasing_parse_stroke :: proc(stroke: string) -> (u64, bool) {
	if len(stroke) == 0 {
		return 0, true
	}
	return stroke_string_to_bits(stroke)
}

phrasing_clone :: proc(text: string) -> (string, bool) {
	return clone_string_ok(text)
}

phrasing_bits_are_unique_forms :: proc(forms: []Phrase_Form, bits: u64) -> bool {
	for form in forms {
		if form.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_tails :: proc(tails: []Phrase_Tail, bits: u64) -> bool {
	for tail in tails {
		if tail.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_iv_stems :: proc(stems: []Iv_Stem, bits: u64) -> bool {
	for stem in stems {
		if stem.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_nv_prefixes :: proc(prefixes: []Nv_Prefix, bits: u64) -> bool {
	for prefix in prefixes {
		if prefix.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_fv_starters :: proc(starters: []Fv_Starter, bits: u64) -> bool {
	for starter in starters {
		if starter.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_fv_operators :: proc(operators: []Fv_Operator, bits: u64) -> bool {
	for operator in operators {
		if operator.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_fv_structures :: proc(structures: []Fv_Structure_Row, bits: u64) -> bool {
	for structure in structures {
		if structure.bits == bits {
			return false
		}
	}
	return true
}

phrasing_bits_are_unique_fv_enders :: proc(enders: []Fv_Ender, bits: u64) -> bool {
	for ender in enders {
		if ender.bits == bits {
			return false
		}
	}
	return true
}

phrasing_find_tail :: proc(tails: []Phrase_Tail, id: string) -> int {
	for tail, i in tails {
		if tail.id == id {
			return i
		}
	}
	return -1
}

phrasing_find_verb :: proc(verbs: []Fv_Verb, id: string) -> int {
	for verb, i in verbs {
		if verb.id == id {
			return i
		}
	}
	return -1
}

phrasing_parse_agreement :: proc(text: string) -> (Fv_Agreement, bool) {
	switch text {
	case "first_singular":
		return .First_Singular, true
	case "third_singular":
		return .Third_Singular, true
	case "plural":
		return .Plural, true
	}
	return .Plural, false
}

phrasing_parse_modal :: proc(text: string) -> (Fv_Modal, bool) {
	switch text {
	case "none":
		return .None, true
	case "can":
		return .Can, true
	case "should":
		return .Should, true
	case "will":
		return .Will, true
	}
	return .None, false
}

phrasing_parse_structure :: proc(text: string) -> (Fv_Structure, bool) {
	switch text {
	case "simple":
		return .Simple, true
	case "progressive":
		return .Progressive, true
	case "perfect":
		return .Perfect, true
	case "perfect_progressive":
		return .Perfect_Progressive, true
	}
	return .Simple, false
}

phrasing_verb_kind :: proc(id: string) -> Fv_Verb_Kind {
	switch id {
	case "be":
		return .Be
	case "have":
		return .Have
	case "do":
		return .Do
	}
	return .Other
}

phrasing_destroy :: proc(phrasing: ^Phrasing) {
	for stem in phrasing.iv_stems {
		for form in stem.forms {
			owned_string_delete(form.text)
		}
		delete(stem.forms)
	}
	delete(phrasing.iv_stems)
	for tail in phrasing.iv_tails {
		owned_string_delete(tail.id)
		owned_string_delete(tail.text)
	}
	delete(phrasing.iv_tails)
	for prefix in phrasing.nv_prefixes {
		owned_string_delete(prefix.text)
		for tail_id in prefix.tail_ids {
			owned_string_delete(tail_id)
		}
		delete(prefix.tail_ids)
	}
	delete(phrasing.nv_prefixes)
	for tail in phrasing.nv_tails {
		owned_string_delete(tail.id)
		owned_string_delete(tail.text)
	}
	delete(phrasing.nv_tails)
	for starter in phrasing.fv_starters {
		owned_string_delete(starter.text)
		owned_string_delete(starter.be_contraction)
		owned_string_delete(starter.have_contraction)
		owned_string_delete(starter.will_contraction)
	}
	delete(phrasing.fv_starters)
	delete(phrasing.fv_operators)
	delete(phrasing.fv_structures)
	for verb in phrasing.fv_verbs {
		owned_string_delete(verb.id)
		owned_string_delete(verb.base)
		owned_string_delete(verb.third)
		owned_string_delete(verb.past)
		owned_string_delete(verb.present_participle)
		owned_string_delete(verb.past_participle)
	}
	delete(phrasing.fv_verbs)
	for ender in phrasing.fv_enders {
		owned_string_delete(ender.verb_id)
		owned_string_delete(ender.suffix)
	}
	delete(phrasing.fv_enders)
	phrasing^ = {}
}

phrasing_parse_tail_array :: proc(out: ^[dynamic]Phrase_Tail, raw_tails: []Raw_Phrase_Tail) -> bool {
	out^ = make([dynamic]Phrase_Tail)
	for raw in raw_tails {
		bits, bits_ok := phrasing_parse_stroke(raw.stroke)
		if !bits_ok || !phrasing_bits_are_unique_tails(out^[:], bits) {
			return false
		}
		id, id_ok := phrasing_clone(raw.id)
		text, text_ok := phrasing_clone(raw.text)
		if !id_ok || !text_ok {
			owned_string_delete(id)
			owned_string_delete(text)
			return false
		}
		append(out, Phrase_Tail{id = id, bits = bits, text = text})
	}
	return true
}

phrasing_parse_initial_verbs :: proc(phrasing: ^Phrasing, raw: ^Raw_Initial_Verbs) -> bool {
	if !phrasing_parse_tail_array(&phrasing.iv_tails, raw.tails[:]) {
		return false
	}
	phrasing.iv_stems = make([dynamic]Iv_Stem)
	for raw_stem in raw.stems {
		bits, bits_ok := phrasing_parse_stroke(raw_stem.stroke)
		if !bits_ok || !phrasing_bits_are_unique_iv_stems(phrasing.iv_stems[:], bits) {
			return false
		}
		stem := Iv_Stem{bits = bits, forms = make([dynamic]Phrase_Form)}
		for raw_form in raw_stem.forms {
			form_bits, form_bits_ok := phrasing_parse_stroke(raw_form.stroke)
			if !form_bits_ok || !phrasing_bits_are_unique_forms(stem.forms[:], form_bits) {
				delete(stem.forms)
				return false
			}
			text, text_ok := phrasing_clone(raw_form.text)
			if !text_ok {
				delete(stem.forms)
				return false
			}
			append(&stem.forms, Phrase_Form{bits = form_bits, text = text})
		}
		append(&phrasing.iv_stems, stem)
	}
	return true
}

phrasing_parse_nonverbs :: proc(phrasing: ^Phrasing, raw: ^Raw_Nonverbs) -> bool {
	if !phrasing_parse_tail_array(&phrasing.nv_tails, raw.tails[:]) {
		return false
	}
	phrasing.nv_prefixes = make([dynamic]Nv_Prefix)
	for raw_prefix in raw.prefixes {
		bits, bits_ok := phrasing_parse_stroke(raw_prefix.stroke)
		if !bits_ok || !phrasing_bits_are_unique_nv_prefixes(phrasing.nv_prefixes[:], bits) {
			return false
		}
		text, text_ok := phrasing_clone(raw_prefix.text)
		if !text_ok {
			return false
		}
		prefix := Nv_Prefix{bits = bits, text = text, tail_ids = make([dynamic]string)}
		for raw_tail_id in raw_prefix.tails {
			if phrasing_find_tail(phrasing.nv_tails[:], raw_tail_id) < 0 {
				owned_string_delete(prefix.text)
				delete(prefix.tail_ids)
				return false
			}
			tail_id, tail_id_ok := phrasing_clone(raw_tail_id)
			if !tail_id_ok {
				owned_string_delete(prefix.text)
				delete(prefix.tail_ids)
				return false
			}
			append(&prefix.tail_ids, tail_id)
		}
		append(&phrasing.nv_prefixes, prefix)
	}
	return true
}

phrasing_parse_final_verbs :: proc(phrasing: ^Phrasing, raw: ^Raw_Final_Verbs) -> bool {
	contraction_bits, contraction_ok := phrasing_parse_stroke(raw.contraction_stroke)
	if !contraction_ok {
		return false
	}
	phrasing.contraction_bits = contraction_bits

	phrasing.fv_starters = make([dynamic]Fv_Starter)
	for raw_starter in raw.starters {
		bits, bits_ok := phrasing_parse_stroke(raw_starter.stroke)
		agreement, agreement_ok := phrasing_parse_agreement(raw_starter.agreement)
		if !bits_ok || !agreement_ok || !phrasing_bits_are_unique_fv_starters(phrasing.fv_starters[:], bits) {
			return false
		}
		text, text_ok := phrasing_clone(raw_starter.text)
		be, be_ok := phrasing_clone(raw_starter.be_contraction)
		have, have_ok := phrasing_clone(raw_starter.have_contraction)
		will, will_ok := phrasing_clone(raw_starter.will_contraction)
		if !text_ok || !be_ok || !have_ok || !will_ok {
			owned_string_delete(text)
			owned_string_delete(be)
			owned_string_delete(have)
			owned_string_delete(will)
			return false
		}
		append(&phrasing.fv_starters, Fv_Starter{
			bits = bits,
			agreement = agreement,
			text = text,
			be_contraction = be,
			have_contraction = have,
			will_contraction = will,
		})
	}

	phrasing.fv_operators = make([dynamic]Fv_Operator)
	for raw_operator in raw.operators {
		bits, bits_ok := phrasing_parse_stroke(raw_operator.stroke)
		modal, modal_ok := phrasing_parse_modal(raw_operator.modal)
		if !bits_ok || !modal_ok || !phrasing_bits_are_unique_fv_operators(phrasing.fv_operators[:], bits) {
			return false
		}
		append(&phrasing.fv_operators, Fv_Operator{bits = bits, modal = modal, negative = raw_operator.negative})
	}

	phrasing.fv_structures = make([dynamic]Fv_Structure_Row)
	for raw_structure in raw.structures {
		bits, bits_ok := phrasing_parse_stroke(raw_structure.stroke)
		structure, structure_ok := phrasing_parse_structure(raw_structure.kind)
		if !bits_ok || !structure_ok || !phrasing_bits_are_unique_fv_structures(phrasing.fv_structures[:], bits) {
			return false
		}
		append(&phrasing.fv_structures, Fv_Structure_Row{bits = bits, structure = structure})
	}

	phrasing.fv_verbs = make([dynamic]Fv_Verb)
	for raw_verb in raw.verbs {
		id, id_ok := phrasing_clone(raw_verb.id)
		base, base_ok := phrasing_clone(raw_verb.base)
		third, third_ok := phrasing_clone(raw_verb.third)
		past, past_ok := phrasing_clone(raw_verb.past)
		present_participle, present_ok := phrasing_clone(raw_verb.present_participle)
		past_participle, participle_ok := phrasing_clone(raw_verb.past_participle)
		if !id_ok || !base_ok || !third_ok || !past_ok || !present_ok || !participle_ok {
			owned_string_delete(id)
			owned_string_delete(base)
			owned_string_delete(third)
			owned_string_delete(past)
			owned_string_delete(present_participle)
			owned_string_delete(past_participle)
			return false
		}
		append(&phrasing.fv_verbs, Fv_Verb{
			id = id,
			kind = phrasing_verb_kind(id),
			base = base,
			third = third,
			past = past,
			present_participle = present_participle,
			past_participle = past_participle,
		})
	}

	phrasing.fv_enders = make([dynamic]Fv_Ender)
	for raw_ender in raw.enders {
		bits, bits_ok := phrasing_parse_stroke(raw_ender.stroke)
		if !bits_ok || !phrasing_bits_are_unique_fv_enders(phrasing.fv_enders[:], bits) {
			return false
		}
		verb_index := -1
		verb_id := ""
		if len(raw_ender.verb) > 0 {
			verb_index = phrasing_find_verb(phrasing.fv_verbs[:], raw_ender.verb)
			if verb_index < 0 {
				return false
			}
			verb_id_ok: bool
			verb_id, verb_id_ok = phrasing_clone(raw_ender.verb)
			if !verb_id_ok {
				return false
			}
		}
		suffix, suffix_ok := phrasing_clone(raw_ender.suffix)
		if !suffix_ok {
			owned_string_delete(verb_id)
			return false
		}
		append(&phrasing.fv_enders, Fv_Ender{
			bits = bits,
			verb_id = verb_id,
			verb_index = verb_index,
			suffix = suffix,
			past = raw_ender.past,
		})
	}
	return true
}

phrasing_load :: proc(path: string) -> (phrasing: Phrasing, ok: bool) {
	data, read_err := os.read_entire_file(path, context.allocator)
	if read_err != nil {
		return {}, false
	}
	defer delete(data)

	raw: Raw_Phrasing
	unmarshal_err := json.unmarshal(data, &raw)
	if unmarshal_err != nil {
		raw_phrasing_destroy(&raw)
		return {}, false
	}
	defer raw_phrasing_destroy(&raw)

	ok = phrasing_parse_initial_verbs(&phrasing, &raw.initial_verbs) &&
		phrasing_parse_nonverbs(&phrasing, &raw.nonverbs) &&
		phrasing_parse_final_verbs(&phrasing, &raw.final_verbs)
	if !ok {
		phrasing_destroy(&phrasing)
		return {}, false
	}
	return phrasing, true
}
