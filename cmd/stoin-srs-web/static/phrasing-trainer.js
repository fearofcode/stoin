let phraseData = null;

const phraseCountInput = document.getElementById('phrase-count');
const phraseOrderSelect = document.getElementById('phrase-order');
const phraseSourceSelect = document.getElementById('phrase-source');
const phraseBankSource = document.getElementById('phrase-bank-source');
const phrasePastedSource = document.getElementById('phrase-pasted-source');
const phrasePastedList = document.getElementById('phrase-pasted-list');
const phrasePastedStatus = document.getElementById('phrase-pasted-status');
const phraseFocusList = document.getElementById('phrase-focus-list');
const phraseFilterInput = document.getElementById('phrase-filter');
const phraseShowOutlines = document.getElementById('phrase-show-outlines');
const phraseSelectAll = document.getElementById('phrase-select-all');
const phraseSelectNone = document.getElementById('phrase-select-none');
const phraseHintsSelect = document.getElementById('phrase-hints');
const phraseRestart = document.getElementById('phrase-restart');
const phraseReroll = document.getElementById('phrase-reroll');
const phraseSetSummary = document.getElementById('phrase-set-summary');
const phraseProgress = document.getElementById('phrase-progress');
const phraseBankName = document.getElementById('phrase-bank-name');
const phrasePrompt = document.getElementById('phrase-prompt');
const phraseHint = document.getElementById('phrase-hint');
const phraseAnswer = document.getElementById('phrase-answer');
const phraseStatus = document.getElementById('phrase-status');
let phraseQueue = [];
let phraseIndex = 0;
let phraseMistake = false;
let pastedPromptLookup = null;
let pastedPhraseLineCount = 0;
let pastedUnmatchedPhrases = [];
const phraseCheckedByKey = new Map();
const phraseStorageKey = 'stoin.phrasingTrainer.v15';

const familyLabels = {
	initial_verbs: 'Initial verbs',
	final_verbs: 'Final verbs',
	nonverbs: 'Nonverbs',
};

const stenoIndexes = {
	num: 0,
	leftS: 1,
	leftT: 2,
	leftK: 3,
	leftP: 4,
	leftW: 5,
	leftH: 6,
	leftR: 7,
	a: 8,
	o: 9,
	star: 10,
	e: 11,
	u: 12,
	rightF: 13,
	rightR: 14,
	rightP: 15,
	rightB: 16,
	rightL: 17,
	rightG: 18,
	rightT: 19,
	rightS: 20,
	rightD: 21,
	rightZ: 22,
};

function stenoBit(index) {
	return 1 << index;
}

function bitForChar(map, char) {
	return Object.prototype.hasOwnProperty.call(map, char) ? stenoBit(map[char]) : 0;
}

const leftMap = {
	'#': stenoIndexes.num,
	S: stenoIndexes.leftS,
	T: stenoIndexes.leftT,
	K: stenoIndexes.leftK,
	P: stenoIndexes.leftP,
	W: stenoIndexes.leftW,
	H: stenoIndexes.leftH,
	R: stenoIndexes.leftR,
};
const leftNumberMap = {
	'1': stenoIndexes.leftS,
	'2': stenoIndexes.leftT,
	'3': stenoIndexes.leftP,
	'4': stenoIndexes.leftH,
};
const vowelMap = {
	A: stenoIndexes.a,
	O: stenoIndexes.o,
	'*': stenoIndexes.star,
	E: stenoIndexes.e,
	U: stenoIndexes.u,
};
const vowelNumberMap = {
	'5': stenoIndexes.a,
	'0': stenoIndexes.o,
};
const rightMap = {
	F: stenoIndexes.rightF,
	R: stenoIndexes.rightR,
	P: stenoIndexes.rightP,
	B: stenoIndexes.rightB,
	L: stenoIndexes.rightL,
	G: stenoIndexes.rightG,
	T: stenoIndexes.rightT,
	S: stenoIndexes.rightS,
	D: stenoIndexes.rightD,
	Z: stenoIndexes.rightZ,
};
const rightNumberMap = {
	'6': stenoIndexes.rightF,
	'7': stenoIndexes.rightP,
	'8': stenoIndexes.rightL,
	'9': stenoIndexes.rightT,
};
const leftAndVowelLabels = [
	[stenoIndexes.num, '#'],
	[stenoIndexes.leftS, 'S'],
	[stenoIndexes.leftT, 'T'],
	[stenoIndexes.leftK, 'K'],
	[stenoIndexes.leftP, 'P'],
	[stenoIndexes.leftW, 'W'],
	[stenoIndexes.leftH, 'H'],
	[stenoIndexes.leftR, 'R'],
	[stenoIndexes.a, 'A'],
	[stenoIndexes.o, 'O'],
	[stenoIndexes.star, '*'],
	[stenoIndexes.e, 'E'],
	[stenoIndexes.u, 'U'],
];
const rightLabels = [
	[stenoIndexes.rightF, 'F'],
	[stenoIndexes.rightR, 'R'],
	[stenoIndexes.rightP, 'P'],
	[stenoIndexes.rightB, 'B'],
	[stenoIndexes.rightL, 'L'],
	[stenoIndexes.rightG, 'G'],
	[stenoIndexes.rightT, 'T'],
	[stenoIndexes.rightS, 'S'],
	[stenoIndexes.rightD, 'D'],
	[stenoIndexes.rightZ, 'Z'],
];

function addStenoBit(bits, bit, stroke) {
	if (!bit || (bits & bit) !== 0) {
		throw new Error('invalid stroke fragment ' + JSON.stringify(stroke));
	}
	return bits | bit;
}

function parseStrokeBits(stroke) {
	if (stroke === '') return 0;
	let bits = 0;
	let region = 'left';
	let sawAny = false;
	let sawNumberDigit = false;
	for (let i = 0; i < stroke.length; i++) {
		const char = stroke[i];
		let bit = 0;
		if (char === '/') {
			throw new Error('multi-stroke outlines are not phrase-generator fragments');
		}
		if (char === '-') {
			region = 'right';
			continue;
		}
		if (region === 'left') {
			bit = bitForChar(leftMap, char);
			if (!bit) {
				bit = bitForChar(leftNumberMap, char);
				if (bit) sawNumberDigit = true;
			}
			if (!bit) {
				bit = bitForChar(vowelMap, char);
				if (bit) region = 'vowel';
			}
			if (!bit) {
				bit = bitForChar(vowelNumberMap, char);
				if (bit) {
					region = 'vowel';
					sawNumberDigit = true;
				}
			}
			if (!bit) {
				bit = bitForChar(rightMap, char);
				if (bit) region = 'right';
			}
			if (!bit) {
				bit = bitForChar(rightNumberMap, char);
				if (bit) {
					region = 'right';
					sawNumberDigit = true;
				}
			}
		} else if (region === 'vowel') {
			bit = bitForChar(vowelMap, char);
			if (!bit) {
				bit = bitForChar(vowelNumberMap, char);
				if (bit) sawNumberDigit = true;
			}
			if (!bit) {
				bit = bitForChar(rightMap, char);
				if (bit) region = 'right';
			}
			if (!bit) {
				bit = bitForChar(rightNumberMap, char);
				if (bit) {
					region = 'right';
					sawNumberDigit = true;
				}
			}
		} else {
			bit = bitForChar(rightMap, char);
			if (!bit) {
				bit = bitForChar(rightNumberMap, char);
				if (bit) sawNumberDigit = true;
			}
		}
		bits = addStenoBit(bits, bit, stroke);
		sawAny = true;
	}
	if (!sawAny || (sawNumberDigit && (bits & stenoBit(stenoIndexes.num)) === 0)) {
		throw new Error('invalid stroke fragment ' + JSON.stringify(stroke));
	}
	return bits;
}

function labelsForBits(labelRows, bits) {
	let out = '';
	labelRows.forEach(function(row) {
		if ((bits & stenoBit(row[0])) !== 0) out += row[1];
	});
	return out;
}

function strokeBitsToString(bits) {
	if (!bits) return '';
	let out = labelsForBits(leftAndVowelLabels, bits);
	const right = labelsForBits(rightLabels, bits);
	if (!right) return out;
	const implicit = out + right;
	try {
		if (parseStrokeBits(implicit) === bits) return implicit;
	} catch (error) {
		// Fall through to explicit right-hand notation.
	}
	return out + '-' + right;
}

function strokeBitsToExplicitRightString(bits) {
	if (!bits) return '';
	const out = labelsForBits(leftAndVowelLabels, bits);
	const right = labelsForBits(rightLabels, bits);
	return right ? out + '-' + right : out;
}

function combineStrokeParts(parts) {
	let bits = 0;
	let explicitRight = false;
	parts.forEach(function(part) {
		if (!part) return;
		if (part.indexOf('-') !== -1) explicitRight = true;
		const partBits = parseStrokeBits(part);
		if ((bits & partBits) !== 0) {
			throw new Error('overlapping phrase-generator fragments near ' + JSON.stringify(part));
		}
		bits |= partBits;
	});
	return explicitRight ? strokeBitsToExplicitRightString(bits) : strokeBitsToString(bits);
}

function displayStroke(stroke) {
	return stroke === '' ? 'empty' : stroke;
}

function appendWord(words, word) {
	if (word) words.push(word);
	return true;
}

function phraseFromWords(words) {
	return words.filter(Boolean).join(' ');
}

function uniqueStrings(items) {
	const seen = new Set();
	const out = [];
	items.forEach(function(item) {
		if (item && !seen.has(item)) {
			seen.add(item);
			out.push(item);
		}
	});
	return out;
}

function familyKey(family) {
	return 'family\n' + family;
}

function bankKey(family, bank, value) {
	return 'bank\n' + family + '\n' + bank + '\n' + value;
}

function defaultFamilyChecked(family) {
	return family === 'initial_verbs';
}

function optionChecked(key, defaultChecked) {
	return phraseCheckedByKey.has(key) ? phraseCheckedByKey.get(key) : defaultChecked;
}

function familyEnabled(family) {
	return optionChecked(familyKey(family), defaultFamilyChecked(family));
}

function bankOptionChecked(family, bank, value) {
	return optionChecked(bankKey(family, bank, value), defaultFamilyChecked(family));
}

function availableFamilies() {
	const families = [];
	if (phraseData && phraseData.initial_verbs) families.push('initial_verbs');
	if (phraseData && phraseData.final_verbs) families.push('final_verbs');
	if (phraseData && phraseData.nonverbs) families.push('nonverbs');
	return families;
}

function sectionOption(key, title, detail, searchParts, defaultChecked) {
	return {
		key: key,
		title: title,
		detail: detail || '',
		searchText: searchParts.filter(Boolean).join(' '),
		defaultChecked: defaultChecked,
	};
}

function familySection() {
	return {
		title: 'Families',
		options: availableFamilies().map(function(family) {
			return sectionOption(
				familyKey(family),
				familyLabels[family],
				'',
				[familyLabels[family], family],
				defaultFamilyChecked(family)
			);
		}),
	};
}

function initialFormOptions() {
	const byStroke = new Map();
	(phraseData.initial_verbs.stems || []).forEach(function(stem) {
		(stem.forms || []).forEach(function(form) {
			const entry = byStroke.get(form.stroke) || { stroke: form.stroke, texts: [] };
			entry.texts.push(form.text);
			byStroke.set(form.stroke, entry);
		});
	});
	return Array.from(byStroke.values()).map(function(entry) {
		entry.texts = uniqueStrings(entry.texts);
		entry.label = initialFormLabel(entry.stroke);
		return entry;
	});
}

function initialFormLabel(stroke) {
	switch (stroke) {
	case '': return 'third-person singular present (he/she/it goes)';
	case '-D': return 'simple past (went)';
	case 'AE': return 'base form (go; bare be)';
	case 'A*E': return 'plural present of be (are)';
	case 'E-D': return 'plural simple past (were)';
	case '*': return 'present participle (-ing form: going)';
	case '*E': return 'to-infinitive (to + base form: to go)';
	case 'A': return 'modal can + base form (can go)';
	case 'A-D': return 'modal could + base form (could go)';
	default: return displayStroke(stroke);
	}
}

function initialStemLabel(stem) {
	const forms = stem.forms || [];
	for (let i = 0; i < forms.length; i++) {
		if (forms[i].stroke === '*E') return forms[i].text;
	}
	return forms.length ? forms[0].text : stem.stroke;
}

function initialSections() {
	const family = 'initial_verbs';
	const stems = phraseData.initial_verbs.stems || [];
	const tails = phraseData.initial_verbs.tails || [];
	const forms = initialFormOptions();
	return [
		{
			title: 'IV stems',
			options: stems.map(function(stem) {
				const label = initialStemLabel(stem);
				const formTexts = (stem.forms || []).map(function(form) { return form.text; });
				return sectionOption(
					bankKey(family, 'stems', stem.stroke),
					label,
					stem.stroke,
					['initial verbs', stem.stroke, label].concat(formTexts),
					defaultFamilyChecked(family)
				);
			}),
		},
		{
			title: 'IV forms',
			options: forms.map(function(form) {
				return sectionOption(
					bankKey(family, 'forms', form.stroke),
					form.label,
					displayStroke(form.stroke),
					['initial verbs', form.stroke, form.label].concat(form.texts),
					defaultFamilyChecked(family)
				);
			}),
		},
		{
			title: 'IV tails',
			options: tails.map(function(tail) {
				const label = tail.text || 'no tail';
				return sectionOption(
					bankKey(family, 'tails', tail.id),
					label,
					displayStroke(tail.stroke),
					['initial verbs', tail.id, label, tail.stroke],
					defaultFamilyChecked(family)
				);
			}),
		},
	];
}

function nonverbSections() {
	const family = 'nonverbs';
	const prefixes = phraseData.nonverbs.prefixes || [];
	const tails = phraseData.nonverbs.tails || [];
	return [
		{
			title: 'NV prefixes',
			options: prefixes.map(function(prefix) {
				return sectionOption(
					bankKey(family, 'prefixes', prefix.stroke),
					prefix.text,
					prefix.stroke,
					['nonverbs', prefix.text, prefix.stroke],
					defaultFamilyChecked(family)
				);
			}),
		},
		{
			title: 'NV tails',
			options: tails.map(function(tail) {
				const label = tail.text || 'no tail';
				return sectionOption(
					bankKey(family, 'tails', tail.id),
					label,
					displayStroke(tail.stroke),
					['nonverbs', tail.id, label, tail.stroke],
					defaultFamilyChecked(family)
				);
			}),
		},
	];
}

function finalVerbByID(id) {
	const verbs = phraseData.final_verbs.verbs || [];
	for (let i = 0; i < verbs.length; i++) {
		if (verbs[i].id === id) return verbs[i];
	}
	return null;
}

function operatorLabel(op) {
	if (op.modal === 'none') return op.negative ? 'negative, no modal (not)' : 'affirmative, no modal';
	if (op.modal === 'can') {
		return op.negative ? 'negative modal auxiliary (cannot / could not)' : 'modal auxiliary (can / could)';
	}
	if (op.modal === 'should') {
		return op.negative ? 'negative modal auxiliary (should not)' : 'modal auxiliary (should)';
	}
	if (op.modal === 'will') {
		return op.negative ? 'negative modal auxiliary (will not / would not)' : 'modal auxiliary (will / would)';
	}
	return (op.negative ? 'negative modal auxiliary (' : 'modal auxiliary (') + op.modal + (op.negative ? ' not)' : ')');
}

function structureLabel(row) {
	switch (row.kind) {
	case 'simple':
		return 'simple (no perfect/progressive construction: goes, can go)';
	case 'progressive':
		return 'progressive (a form of be + present participle: is going)';
	case 'perfect':
		return 'perfect (a form of have + past participle: has gone)';
	case 'perfect_progressive':
		return 'perfect progressive (a form of have + been + present participle: has been going)';
	default:
		return row.kind.replace(/_/g, ' ');
	}
}

function enderLabel(ender) {
	const verb = finalVerbByID(ender.verb);
	if (!verb) {
		return ender.past
			? 'auxiliary only, past-form selection (main-verb slot empty: did not / could / was / had / had been)'
			: 'auxiliary only (main-verb slot empty: do not / can / be / have / have been)';
	}
	const parts = [verb.base];
	if (ender.suffix) parts.push(ender.suffix);
	if (ender.past) parts.push('(past-form selection)');
	return parts.join(' ');
}

function finalVerbSections() {
	const family = 'final_verbs';
	const finalVerbs = phraseData.final_verbs;
	return [
		{
			title: 'FV starters',
			options: (finalVerbs.starters || []).map(function(starter) {
				const label = starter.label || starter.text;
				return sectionOption(
					bankKey(family, 'starters', starter.stroke),
					label,
					starter.stroke,
					['final verbs', label, starter.text, starter.stroke, starter.agreement],
					defaultFamilyChecked(family)
				);
			}),
		},
		{
			title: 'FV operators (modals / negation)',
			options: (finalVerbs.operators || []).map(function(op) {
				const label = operatorLabel(op);
				return sectionOption(
					bankKey(family, 'operators', op.stroke),
					label,
					displayStroke(op.stroke),
					['final verbs', label, op.stroke, op.modal, op.negative ? 'negative' : 'affirmative'],
					defaultFamilyChecked(family)
				);
			}),
		},
		{
			title: 'FV structures / verb shapes',
			options: (finalVerbs.structures || []).map(function(row) {
				const label = structureLabel(row);
				return sectionOption(
					bankKey(family, 'structures', row.stroke),
					label,
					displayStroke(row.stroke),
					['final verbs', label, row.stroke],
					defaultFamilyChecked(family)
				);
			}),
		},
		{
			title: 'FV main-verb enders',
			options: (finalVerbs.enders || []).map(function(ender, index) {
				const label = enderLabel(ender);
				return sectionOption(
					bankKey(family, 'enders', String(index)),
					label,
					displayStroke(ender.stroke),
					['final verbs', label, ender.stroke, ender.verb || '', ender.suffix || '', ender.past ? 'past' : 'present'],
					defaultFamilyChecked(family)
				);
			}),
		},
	];
}

function focusSections() {
	const sections = [familySection()];
	if (familyEnabled('initial_verbs') && phraseData.initial_verbs) {
		sections.push.apply(sections, initialSections());
	}
	if (familyEnabled('final_verbs') && phraseData.final_verbs) {
		sections.push.apply(sections, finalVerbSections());
	}
	if (familyEnabled('nonverbs') && phraseData.nonverbs) {
		sections.push.apply(sections, nonverbSections());
	}
	return sections;
}

function selectedInitialStems() {
	return (phraseData.initial_verbs.stems || []).filter(function(stem) {
		return bankOptionChecked('initial_verbs', 'stems', stem.stroke);
	});
}

function selectedInitialForms() {
	return initialFormOptions().filter(function(form) {
		return bankOptionChecked('initial_verbs', 'forms', form.stroke);
	});
}

function selectedInitialTails() {
	return (phraseData.initial_verbs.tails || []).filter(function(tail) {
		return bankOptionChecked('initial_verbs', 'tails', tail.id);
	});
}

function findStemForm(stem, stroke) {
	for (let i = 0; i < (stem.forms || []).length; i++) {
		if (stem.forms[i].stroke === stroke) return stem.forms[i];
	}
	return null;
}

function generateInitialVerbPrompts(selectedOnly) {
	const useSelection = selectedOnly !== false;
	if (useSelection && !familyEnabled('initial_verbs')) return [];
	const stems = useSelection ? selectedInitialStems() : (phraseData.initial_verbs.stems || []);
	const forms = useSelection ? selectedInitialForms() : initialFormOptions();
	const tails = useSelection ? selectedInitialTails() : (phraseData.initial_verbs.tails || []);
	const prompts = [];
	stems.forEach(function(stem) {
		forms.forEach(function(formOption) {
			const form = findStemForm(stem, formOption.stroke);
			if (!form) return;
			tails.forEach(function(tail) {
				prompts.push({
					lesson: familyLabels.initial_verbs,
					stroke: combineStrokeParts([stem.stroke, form.stroke, tail.stroke]),
					phrase: phraseFromWords([form.text, tail.text]),
				});
			});
		});
	});
	return prompts;
}

function generateNonverbPrompts(selectedOnly) {
	const useSelection = selectedOnly !== false;
	if (!phraseData.nonverbs) return [];
	if (useSelection && !familyEnabled('nonverbs')) return [];
	const allPrefixes = phraseData.nonverbs.prefixes || [];
	const allTails = phraseData.nonverbs.tails || [];
	const prefixes = useSelection ? allPrefixes.filter(function(prefix) {
		return bankOptionChecked('nonverbs', 'prefixes', prefix.stroke);
	}) : allPrefixes;
	const tails = useSelection ? allTails.filter(function(tail) {
		return bankOptionChecked('nonverbs', 'tails', tail.id);
	}) : allTails;
	const prompts = [];
	prefixes.forEach(function(prefix) {
		tails.forEach(function(tail) {
			prompts.push({
				lesson: familyLabels.nonverbs,
				stroke: combineStrokeParts(['E', prefix.stroke, tail.stroke]),
				phrase: phraseFromWords([prefix.text, tail.text]),
			});
		});
	});
	return prompts;
}

function fvBeWord(starter, past) {
	if (past) return starter.agreement === 'plural' ? 'were' : 'was';
	if (starter.agreement === 'first_singular') return 'am';
	return starter.agreement === 'plural' ? 'are' : 'is';
}

function fvHaveWord(starter, past) {
	if (past) return 'had';
	return starter.agreement === 'third_singular' ? 'has' : 'have';
}

function fvDoWord(starter, past) {
	if (past) return 'did';
	return starter.agreement === 'third_singular' ? 'does' : 'do';
}

function fvVerbKind(verb) {
	if (!verb) return 'other';
	if (verb.id === 'be' || verb.id === 'have' || verb.id === 'do') return verb.id;
	return 'other';
}

function fvFiniteVerbWord(starter, verb, past) {
	const kind = fvVerbKind(verb);
	if (kind === 'be') return fvBeWord(starter, past);
	if (kind === 'have') return fvHaveWord(starter, past);
	if (kind === 'do') return fvDoWord(starter, past);
	if (past) return verb.past;
	return starter.agreement === 'third_singular' ? verb.third : verb.base;
}

function fvModalWord(modal, past, negative) {
	if (modal === 'can') {
		if (negative) return past ? 'could not' : 'cannot';
		return past ? 'could' : 'can';
	}
	if (modal === 'should') return negative ? 'should not' : 'should';
	if (modal === 'will') {
		if (negative) return past ? 'would not' : 'will not';
		return past ? 'would' : 'will';
	}
	return '';
}

function appendVerbAndSuffix(words, verbWord, suffix) {
	appendWord(words, verbWord);
	appendWord(words, suffix);
	return true;
}

function appendModalComplement(words, structureKind, ender) {
	const verb = ender.verbObj;
	if (structureKind === 'simple') {
		return !verb || appendVerbAndSuffix(words, verb.base, ender.suffix);
	}
	if (structureKind === 'progressive') {
		appendWord(words, 'be');
		return !verb || appendVerbAndSuffix(words, verb.present_participle, ender.suffix);
	}
	if (structureKind === 'perfect') {
		appendWord(words, 'have');
		return !verb || appendVerbAndSuffix(words, verb.past_participle, ender.suffix);
	}
	if (structureKind === 'perfect_progressive') {
		appendWord(words, 'have');
		appendWord(words, 'been');
		return !verb || appendVerbAndSuffix(words, verb.present_participle, ender.suffix);
	}
	return false;
}

function buildFvLong(starter, op, structureKind, ender) {
	const verb = ender.verbObj;
	const words = [starter.text];
	if (op.modal === 'none' && structureKind === 'simple' && !verb) {
		if (!op.negative) return null;
		appendWord(words, fvDoWord(starter, ender.past));
		appendWord(words, 'not');
		return phraseFromWords(words);
	}
	if (op.modal !== 'none') {
		appendWord(words, fvModalWord(op.modal, ender.past, op.negative));
		return appendModalComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
	}
	if (structureKind === 'simple') {
		if (op.negative && fvVerbKind(verb) !== 'be') {
			appendWord(words, fvDoWord(starter, ender.past));
			appendWord(words, 'not');
			appendVerbAndSuffix(words, verb.base, ender.suffix);
			return phraseFromWords(words);
		}
		appendWord(words, fvFiniteVerbWord(starter, verb, ender.past));
		if (op.negative) appendWord(words, 'not');
		appendWord(words, ender.suffix);
		return phraseFromWords(words);
	}
	if (structureKind === 'progressive') {
		appendWord(words, fvBeWord(starter, ender.past));
		if (op.negative) appendWord(words, 'not');
		if (verb) appendVerbAndSuffix(words, verb.present_participle, ender.suffix);
		return phraseFromWords(words);
	}
	if (structureKind === 'perfect') {
		appendWord(words, fvHaveWord(starter, ender.past));
		if (op.negative) appendWord(words, 'not');
		if (verb) appendVerbAndSuffix(words, verb.past_participle, ender.suffix);
		return phraseFromWords(words);
	}
	if (structureKind === 'perfect_progressive') {
		appendWord(words, fvHaveWord(starter, ender.past));
		if (op.negative) appendWord(words, 'not');
		appendWord(words, 'been');
		if (verb) appendVerbAndSuffix(words, verb.present_participle, ender.suffix);
		return phraseFromWords(words);
	}
	return null;
}

function selectedFinalVerbRows(bank, valueForRow) {
	return (phraseData.final_verbs[bank] || []).filter(function(row, index) {
		const value = valueForRow ? valueForRow(row, index) : row.stroke;
		return bankOptionChecked('final_verbs', bank, value);
	});
}

function generateFinalVerbPrompts(selectedOnly) {
	const useSelection = selectedOnly !== false;
	if (useSelection && !familyEnabled('final_verbs')) return [];
	const finalVerbs = phraseData.final_verbs;
	const starters = useSelection ? selectedFinalVerbRows('starters') : (finalVerbs.starters || []);
	const operators = useSelection ? selectedFinalVerbRows('operators') : (finalVerbs.operators || []);
	const structures = useSelection ? selectedFinalVerbRows('structures') : (finalVerbs.structures || []);
	const selectedEnders = useSelection
		? selectedFinalVerbRows('enders', function(row, index) { return String(index); })
		: (finalVerbs.enders || []);
	const enders = selectedEnders.map(function(ender) {
		return Object.assign({}, ender, { verbObj: finalVerbByID(ender.verb) });
	});
	const prompts = [];
	starters.forEach(function(starter) {
		operators.forEach(function(op) {
			if (starter.requires_modal && op.modal === 'none') return;
			structures.forEach(function(structure) {
				enders.forEach(function(ender) {
					let stroke = '';
					try {
						stroke = combineStrokeParts(['U', starter.stroke, op.stroke, structure.stroke, ender.stroke]);
					} catch (error) {
						return;
					}
					const phrase = buildFvLong(starter, op, structure.kind, ender);
					if (!phrase) return;
					prompts.push({
						lesson: familyLabels.final_verbs,
						stroke: stroke,
						phrase: phrase,
						hasExplicitVerb: Boolean(ender.verbObj),
					});
				});
			});
		});
	});
	return prompts;
}

function uniquePrompts(prompts) {
	const seen = new Set();
	const out = [];
	prompts.forEach(function(prompt) {
		const key = prompt.lesson + '\n' + prompt.stroke + '\n' + prompt.phrase;
		if (!seen.has(key)) {
			seen.add(key);
			out.push(prompt);
		}
	});
	return out;
}

function generatedPromptPool(selectedOnly) {
	return uniquePrompts([]
		.concat(generateInitialVerbPrompts(selectedOnly))
		.concat(generateFinalVerbPrompts(selectedOnly))
		.concat(generateNonverbPrompts(selectedOnly)));
}

function promptLessonRank(prompt) {
	switch (prompt.lesson) {
	case familyLabels.initial_verbs: return 0;
	case familyLabels.final_verbs: return 1;
	case familyLabels.nonverbs: return 2;
	default: return 3;
	}
}

function promptIsBetter(candidate, current) {
	if (!current) return true;
	const candidateRank = promptLessonRank(candidate);
	const currentRank = promptLessonRank(current);
	if (candidateRank !== currentRank) return candidateRank < currentRank;
	if (candidate.hasExplicitVerb !== current.hasExplicitVerb) {
		return candidate.hasExplicitVerb === true;
	}
	if (candidate.stroke.length !== current.stroke.length) {
		return candidate.stroke.length < current.stroke.length;
	}
	return candidate.stroke < current.stroke;
}

function allPhrasingPromptLookup() {
	if (pastedPromptLookup) return pastedPromptLookup;
	pastedPromptLookup = new Map();
	generatedPromptPool(false).forEach(function(prompt) {
		const key = normalizeAnswer(prompt.phrase);
		const current = pastedPromptLookup.get(key);
		if (promptIsBetter(prompt, current)) pastedPromptLookup.set(key, prompt);
	});
	return pastedPromptLookup;
}

function pastedPhraseLines() {
	return phrasePastedList.value.split(/\r?\n/).map(function(line) {
		return line.trim();
	}).filter(function(line) {
		return line !== '';
	});
}

function pastedPhrasePool() {
	const lines = pastedPhraseLines();
	const lookup = allPhrasingPromptLookup();
	const prompts = [];
	const unmatched = [];
	lines.forEach(function(line) {
		const prompt = lookup.get(normalizeAnswer(line));
		if (prompt) prompts.push(prompt);
		else unmatched.push(line);
	});
	pastedPhraseLineCount = lines.length;
	pastedUnmatchedPhrases = unmatched;
	return prompts;
}

function currentPool() {
	if (phraseSourceSelect.value === 'pasted') return pastedPhrasePool();
	pastedPhraseLineCount = 0;
	pastedUnmatchedPhrases = [];
	return generatedPromptPool(true);
}

function shuffle(items) {
	const out = items.slice();
	for (let i = out.length - 1; i > 0; i--) {
		const j = Math.floor(Math.random() * (i + 1));
		const tmp = out[i];
		out[i] = out[j];
		out[j] = tmp;
	}
	return out;
}

function repeatedShuffledPasses(pool, repetitions) {
	if (!pool.length || repetitions <= 0) return [];
	const out = [];
	for (let i = 0; i < repetitions; i++) {
		const pass = shuffle(pool);
		for (let j = 0; j < pass.length; j++) {
			out.push(pass[j]);
		}
	}
	return out;
}

function randomPrompts(pool, repetitions) {
	if (!pool.length || repetitions <= 0) return [];
	const out = [];
	const count = pool.length * repetitions;
	for (let i = 0; i < count; i++) {
		out.push(pool[Math.floor(Math.random() * pool.length)]);
	}
	return out;
}

function repeatedPromptBlocks(pool, repetitions) {
	if (!pool.length || repetitions <= 0) return [];
	const out = [];
	pool.forEach(function(prompt) {
		for (let i = 0; i < repetitions; i++) {
			out.push(prompt);
		}
	});
	return out;
}

function repetitionCount() {
	const count = Number(phraseCountInput.value || '7');
	if (!Number.isFinite(count)) return 7;
	return Math.max(1, Math.min(100, Math.round(count)));
}

function localStorageGet(key) {
	try {
		return window.localStorage.getItem(key);
	} catch (error) {
		return null;
	}
}

function localStorageSet(key, value) {
	try {
		window.localStorage.setItem(key, value);
	} catch (error) {
		// Ignore storage failures; the trainer should still work without persistence.
	}
}

function optionExists(select, value) {
	for (let i = 0; i < select.options.length; i++) {
		if (select.options[i].value === value) return true;
	}
	return false;
}

function loadSavedPhraseSettings() {
	const raw = localStorageGet(phraseStorageKey);
	if (!raw) return null;
	try {
		const parsed = JSON.parse(raw);
		return parsed && typeof parsed === 'object' ? parsed : null;
	} catch (error) {
		return null;
	}
}

function savePhraseSettings() {
	syncFocusSelectionFromInputs();
	const settings = {
		repetitions: repetitionCount(),
		order: phraseOrderSelect.value,
		source: phraseSourceSelect.value,
		hints: phraseHintsSelect.value,
		showOutlines: phraseShowOutlines.checked,
		pastedPhrases: phrasePastedList.value,
		checkedByKey: Array.from(phraseCheckedByKey.entries()),
	};
	localStorageSet(phraseStorageKey, JSON.stringify(settings));
}

function restorePhraseSettings() {
	const settings = loadSavedPhraseSettings();
	if (!settings) return;

	if (typeof settings.repetitions === 'number' && Number.isFinite(settings.repetitions)) {
		phraseCountInput.value = String(Math.max(1, Math.min(100, Math.round(settings.repetitions))));
	}
	if (typeof settings.order === 'string' && optionExists(phraseOrderSelect, settings.order)) {
		phraseOrderSelect.value = settings.order;
	}
	if (typeof settings.source === 'string' && optionExists(phraseSourceSelect, settings.source)) {
		phraseSourceSelect.value = settings.source;
	}
	if (typeof settings.hints === 'string' && optionExists(phraseHintsSelect, settings.hints)) {
		phraseHintsSelect.value = settings.hints;
	}
	if (typeof settings.showOutlines === 'boolean') {
		phraseShowOutlines.checked = settings.showOutlines;
	}
	if (typeof settings.pastedPhrases === 'string') {
		phrasePastedList.value = settings.pastedPhrases;
	}
	if (Array.isArray(settings.checkedByKey)) {
		phraseCheckedByKey.clear();
		settings.checkedByKey.forEach(function(entry) {
			if (Array.isArray(entry) && entry.length === 2 && typeof entry[0] === 'string' && typeof entry[1] === 'boolean') {
				phraseCheckedByKey.set(entry[0], entry[1]);
			}
		});
	}
}

function normalizePromptFilter(text) {
	return text.trim().toLowerCase();
}

function optionMatchesFilter(option, filter) {
	if (!filter) return true;
	return option.searchText.toLowerCase().includes(filter);
}

function syncFocusSelectionFromInputs() {
	phraseFocusList.querySelectorAll('input[type="checkbox"]').forEach(function(input) {
		phraseCheckedByKey.set(input.value, input.checked);
	});
}

function appendFocusSection(section, filter, showOutlines) {
	const visibleOptions = section.options.filter(function(option) { return optionMatchesFilter(option, filter); });
	if (!visibleOptions.length) return 0;

	const wrapper = document.createElement('section');
	wrapper.className = 'phrase-bank';

	const title = document.createElement('h2');
	title.textContent = section.title;
	wrapper.appendChild(title);

	const grid = document.createElement('div');
	grid.className = 'phrase-bank-options';
	visibleOptions.forEach(function(option) {
		const input = document.createElement('input');
		input.type = 'checkbox';
		input.value = option.key;
		input.checked = optionChecked(option.key, option.defaultChecked);

		const text = document.createElement('span');
		text.textContent = showOutlines && option.detail ? option.title + ' - ' + option.detail : option.title;

		const label = document.createElement('label');
		label.appendChild(input);
		label.appendChild(text);
		grid.appendChild(label);
	});
	wrapper.appendChild(grid);
	phraseFocusList.appendChild(wrapper);
	return visibleOptions.length;
}

function populateFocusOptions() {
	const filter = normalizePromptFilter(phraseFilterInput.value || '');
	const showOutlines = phraseShowOutlines.checked;
	const sections = focusSections();
	let visibleCount = 0;
	phraseFocusList.textContent = '';
	sections.forEach(function(section) {
		visibleCount += appendFocusSection(section, filter, showOutlines);
	});
	if (!visibleCount) {
		const empty = document.createElement('div');
		empty.className = 'small';
		empty.textContent = 'No bank options match.';
		phraseFocusList.appendChild(empty);
	}
}

function updatePromptSourceVisibility() {
	const pasted = phraseSourceSelect.value === 'pasted';
	phraseBankSource.hidden = pasted;
	phrasePastedSource.hidden = !pasted;
}

function updatePastedPhraseStatus(poolLength) {
	if (phraseSourceSelect.value !== 'pasted') return;
	if (pastedPhraseLineCount === 0) {
		phrasePastedStatus.textContent = 'Paste phrases, then choose Restart.';
		return;
	}
	if (pastedUnmatchedPhrases.length === 0) {
		phrasePastedStatus.textContent = 'Matched all ' + poolLength + ' pasted phrase'
			+ (poolLength === 1 ? '.' : 's.');
		return;
	}
	const preview = pastedUnmatchedPhrases.slice(0, 5).join(' / ');
	const remaining = pastedUnmatchedPhrases.length - Math.min(5, pastedUnmatchedPhrases.length);
	phrasePastedStatus.textContent = 'Matched ' + poolLength + ' of ' + pastedPhraseLineCount
		+ '. Not found in the phrase system: ' + preview
		+ (remaining > 0 ? ' / +' + remaining + ' more' : '');
}

function rebuildPhraseQueue() {
	syncFocusSelectionFromInputs();
	updatePromptSourceVisibility();
	populateFocusOptions();
	const pool = currentPool();
	updatePastedPhraseStatus(pool.length);
	const repetitions = repetitionCount();
	const order = phraseOrderSelect.value;
	if (order === 'selected') {
		phraseQueue = repeatedPromptBlocks(pool, repetitions);
	} else if (order === 'random') {
		phraseQueue = randomPrompts(pool, repetitions);
	} else {
		phraseQueue = repeatedShuffledPasses(pool, repetitions);
	}
	phraseIndex = 0;
	phraseMistake = false;
	phraseAnswer.value = '';
	const promptKind = phraseSourceSelect.value === 'pasted' ? 'matched pasted prompt' : 'generated prompt';
	if (order === 'selected') {
		phraseSetSummary.textContent = pool.length + ' ' + promptKind
			+ (pool.length === 1 ? '' : 's')
			+ ' x ' + repetitions + ' each'
			+ ' = ' + phraseQueue.length + ' exercises';
	} else {
		phraseSetSummary.textContent = pool.length + ' ' + promptKind
			+ (pool.length === 1 ? '' : 's')
			+ ' x ' + repetitions
			+ ' = ' + phraseQueue.length + ' exercises';
	}
	renderPhraseTrainer();
}

function normalizeAnswer(text) {
	return text.trim().toLowerCase();
}

function normalizedPromptPhrase(prompt) {
	return normalizeAnswer(prompt.phrase);
}

function currentPhrase() {
	return phraseIndex < phraseQueue.length ? phraseQueue[phraseIndex] : null;
}

function hintShouldShow(prompt) {
	if (!prompt) return false;
	if (phraseHintsSelect.value === 'on') return true;
	return phraseHintsSelect.value === 'after' && phraseMistake;
}

function answerPrefixOk(prompt, answer) {
	if (!prompt || answer === '') return true;
	return normalizedPromptPhrase(prompt).startsWith(answer);
}

function renderPhraseTrainer() {
	const prompt = currentPhrase();
	phraseAnswer.classList.remove('wrong');
	if (!phraseQueue.length) {
		phraseProgress.textContent = '0 / 0';
		phraseBankName.textContent = '';
		phrasePrompt.textContent = 'No phrases available.';
		phraseHint.textContent = '';
		phraseHint.classList.add('hidden');
		phraseStatus.textContent = '';
		phraseAnswer.disabled = true;
		return;
	}
	if (!prompt) {
		phraseProgress.textContent = phraseQueue.length + ' / ' + phraseQueue.length;
		phraseBankName.textContent = '';
		phrasePrompt.textContent = 'Done.';
		phraseHint.textContent = '';
		phraseHint.classList.add('hidden');
		phraseStatus.innerHTML = '<span class="phrase-done">Finished this set.</span>';
		phraseAnswer.disabled = true;
		return;
	}
	const typed = normalizeAnswer(phraseAnswer.value);
	phraseProgress.textContent = (phraseIndex + 1) + ' / ' + phraseQueue.length;
	phraseBankName.textContent = prompt.lesson;
	phrasePrompt.textContent = prompt.phrase;
	phraseHint.textContent = prompt.stroke;
	phraseHint.classList.toggle('hidden', !hintShouldShow(prompt));
	phraseStatus.textContent = '';
	phraseAnswer.disabled = false;
	if (!answerPrefixOk(prompt, typed)) {
		phraseAnswer.classList.add('wrong');
	}
	phraseAnswer.focus({ preventScroll: true });
}

function advancePhraseIfCorrect() {
	const prompt = currentPhrase();
	if (!prompt) return;
	const typed = normalizeAnswer(phraseAnswer.value);
	if (typed === normalizedPromptPhrase(prompt)) {
		phraseIndex++;
		phraseMistake = false;
		phraseAnswer.value = '';
		renderPhraseTrainer();
		return;
	}
	if (typed !== '' && !answerPrefixOk(prompt, typed)) {
		phraseMistake = true;
	}
	renderPhraseTrainer();
}

function requireArray(parent, field, context) {
	if (!parent || !Array.isArray(parent[field])) {
		throw new Error(context + '.' + field + ' must be an array');
	}
}

function validatePhraseData(data) {
	if (!data || typeof data !== 'object') throw new Error('phrasing data must be an object');
	if (!data.initial_verbs || !data.final_verbs) {
		throw new Error('phrasing JSON needs initial_verbs and final_verbs');
	}
	requireArray(data.initial_verbs, 'stems', 'initial_verbs');
	requireArray(data.initial_verbs, 'tails', 'initial_verbs');
	const tailIds = new Set();
	data.initial_verbs.tails.forEach(function(tail, index) {
		if (!tail || typeof tail.id !== 'string') {
			throw new Error('initial_verbs.tails[' + index + '].id must be a string');
		}
		if (tailIds.has(tail.id)) {
			throw new Error('initial_verbs.tails[' + index + '].id duplicates ' + tail.id);
		}
		tailIds.add(tail.id);
	});
	if (data.nonverbs !== undefined) {
		requireArray(data.nonverbs, 'prefixes', 'nonverbs');
		requireArray(data.nonverbs, 'tails', 'nonverbs');
		const nonverbTailIds = new Set();
		data.nonverbs.tails.forEach(function(tail, index) {
			if (!tail || typeof tail.id !== 'string') {
				throw new Error('nonverbs.tails[' + index + '].id must be a string');
			}
			if (nonverbTailIds.has(tail.id)) {
				throw new Error('nonverbs.tails[' + index + '].id duplicates ' + tail.id);
			}
			nonverbTailIds.add(tail.id);
		});
	}
	requireArray(data.final_verbs, 'starters', 'final_verbs');
	requireArray(data.final_verbs, 'operators', 'final_verbs');
	requireArray(data.final_verbs, 'structures', 'final_verbs');
	requireArray(data.final_verbs, 'verbs', 'final_verbs');
	requireArray(data.final_verbs, 'enders', 'final_verbs');
	const enderBits = new Set();
	data.final_verbs.enders.forEach(function(ender, index) {
		if (!ender || typeof ender.stroke !== 'string') {
			throw new Error('final_verbs.enders[' + index + '].stroke must be a string');
		}
		let bits = 0;
		try {
			bits = parseStrokeBits(ender.stroke);
		} catch (error) {
			throw new Error('final_verbs.enders[' + index + '].stroke has invalid outline ' + ender.stroke);
		}
		if (enderBits.has(bits)) {
			throw new Error('final_verbs.enders[' + index + '].stroke duplicates ' + ender.stroke);
		}
		enderBits.add(bits);
	});
	data.final_verbs.starters.forEach(function(starter, index) {
		if (starter.label !== undefined && typeof starter.label !== 'string') {
			throw new Error('final_verbs.starters[' + index + '].label must be a string');
		}
		if (starter.requires_modal !== undefined && typeof starter.requires_modal !== 'boolean') {
			throw new Error('final_verbs.starters[' + index + '].requires_modal must be a boolean');
		}
		if (starter.shared !== undefined && typeof starter.shared !== 'boolean') {
			throw new Error('final_verbs.starters[' + index + '].shared must be a boolean');
		}
	});
}

function showPhraseLoadError(message) {
	phraseProgress.textContent = '0 / 0';
	phraseBankName.textContent = '';
	phrasePrompt.textContent = 'Could not load phrasing data.';
	phraseHint.textContent = '';
	phraseHint.classList.add('hidden');
	phraseStatus.textContent = message;
	phraseAnswer.disabled = true;
	phraseFocusList.textContent = '';
	phraseSetSummary.textContent = '';
}

async function loadPhraseData() {
	try {
		const dataURL = new URL('/phrasing-data.json', window.location.href);
		dataURL.searchParams.set('_', Date.now() + '-' + Math.random());
		const response = await fetch(dataURL, {
			cache: 'no-store',
			headers: {
				'Cache-Control': 'no-cache',
				'Pragma': 'no-cache'
			}
		});
		if (!response.ok) {
			throw new Error('phrasing-data.json returned HTTP ' + response.status);
		}
		const data = await response.json();
		validatePhraseData(data);
		phraseData = data;
		restorePhraseSettings();
		rebuildPhraseQueue();
	} catch (error) {
		showPhraseLoadError(error.message || String(error));
	}
}

phraseCountInput.addEventListener('change', function() {
	rebuildPhraseQueue();
	savePhraseSettings();
});
phraseOrderSelect.addEventListener('change', function() {
	rebuildPhraseQueue();
	savePhraseSettings();
});
phraseSourceSelect.addEventListener('change', function() {
	rebuildPhraseQueue();
	savePhraseSettings();
});
phrasePastedList.addEventListener('input', function() {
	phrasePastedStatus.textContent = 'Choose Restart to use the updated list.';
});
phrasePastedList.addEventListener('change', function() {
	savePhraseSettings();
});
phraseFocusList.addEventListener('change', function() {
	rebuildPhraseQueue();
	savePhraseSettings();
});
phraseFilterInput.addEventListener('input', function() {
	syncFocusSelectionFromInputs();
	populateFocusOptions();
});
phraseShowOutlines.addEventListener('change', function() {
	syncFocusSelectionFromInputs();
	populateFocusOptions();
	savePhraseSettings();
});
phraseSelectAll.addEventListener('click', function() {
	phraseFocusList.querySelectorAll('input[type="checkbox"]').forEach(function(input) {
		input.checked = true;
	});
	rebuildPhraseQueue();
	savePhraseSettings();
});
phraseSelectNone.addEventListener('click', function() {
	phraseFocusList.querySelectorAll('input[type="checkbox"]').forEach(function(input) {
		input.checked = false;
	});
	rebuildPhraseQueue();
	savePhraseSettings();
});
phraseHintsSelect.addEventListener('change', function() {
	savePhraseSettings();
	renderPhraseTrainer();
});
phraseRestart.addEventListener('click', function() {
	rebuildPhraseQueue();
	savePhraseSettings();
});
phraseReroll.addEventListener('click', rebuildPhraseQueue);
phraseAnswer.addEventListener('input', advancePhraseIfCorrect);
phraseAnswer.addEventListener('keydown', function(event) {
	if (event.key === 'Enter' && phraseIndex >= phraseQueue.length) {
		rebuildPhraseQueue();
	}
});
loadPhraseData();
