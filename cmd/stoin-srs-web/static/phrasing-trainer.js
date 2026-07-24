let phraseData = null;

const phraseCountInput = document.getElementById('phrase-count');
const phraseOrderSelect = document.getElementById('phrase-order');
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
const phraseCheckedByKey = new Map();
const phraseStorageKey = 'stoin.phrasingTrainer.v13';

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
	case 'E': return 'base form / non-third-person present (go; are for be)';
	case 'E-D': return 'plural simple past (were)';
	case '*': return 'present participle (-ing form: going)';
	case 'U': return 'to-infinitive (to + base form: to go)';
	case 'EU': return 'bare infinitive (base form without to: be)';
	case 'A': return 'modal can + base form (can go)';
	case 'A-D': return 'modal could + base form (could go)';
	default: return displayStroke(stroke);
	}
}

function initialStemLabel(stem) {
	const forms = stem.forms || [];
	for (let i = 0; i < forms.length; i++) {
		if (forms[i].stroke === 'U') return forms[i].text;
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
			? 'auxiliary only, past-form selection (main-verb slot empty: could / was / had / had been)'
			: 'auxiliary only (main-verb slot empty: can / be / have / have been)';
	}
	const parts = [verb.base];
	if (ender.suffix) parts.push(ender.suffix);
	if (ender.past) parts.push('(past-form selection)');
	return parts.join(' ');
}

function finalVerbSections() {
	const family = 'final_verbs';
	const finalVerbs = phraseData.final_verbs;
	const modes = [
		{ id: 'long', label: 'long forms', stroke: '' },
		{ id: 'contraction', label: finalVerbs.contraction_stroke + ' contractions', stroke: finalVerbs.contraction_stroke },
	];
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
		{
			title: 'FV output',
			options: modes.map(function(mode) {
				return sectionOption(
					bankKey(family, 'modes', mode.id),
					mode.label,
					mode.stroke,
					['final verbs', mode.id, mode.label, mode.stroke],
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

function stemAllowsTail(stem, tail) {
	return !Array.isArray(stem.tails) || stem.tails.includes(tail.id);
}

function tailAllowsStemForm(tail, stem, form) {
	return (!Array.isArray(tail.stems) || tail.stems.includes(stem.stroke))
		&& (!Array.isArray(tail.forms) || tail.forms.includes(form.stroke));
}

function generateInitialVerbPrompts() {
	if (!familyEnabled('initial_verbs')) return [];
	const prompts = [];
	selectedInitialStems().forEach(function(stem) {
		selectedInitialForms().forEach(function(formOption) {
			const form = findStemForm(stem, formOption.stroke);
			if (!form) return;
			selectedInitialTails().forEach(function(tail) {
				if (!stemAllowsTail(stem, tail) || !tailAllowsStemForm(tail, stem, form)) return;
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

function generateNonverbPrompts() {
	if (!familyEnabled('nonverbs')) return [];
	const prefixes = (phraseData.nonverbs.prefixes || []).filter(function(prefix) {
		return bankOptionChecked('nonverbs', 'prefixes', prefix.stroke);
	});
	const tails = (phraseData.nonverbs.tails || []).filter(function(tail) {
		return bankOptionChecked('nonverbs', 'tails', tail.id);
	});
	const prompts = [];
	prefixes.forEach(function(prefix) {
		tails.forEach(function(tail) {
			if (!Array.isArray(prefix.tails) || prefix.tails.indexOf(tail.id) === -1) return;
			prompts.push({
				lesson: familyLabels.nonverbs,
				stroke: combineStrokeParts([prefix.stroke, tail.stroke]),
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

function fvModalNegativeContraction(modal, past) {
	if (modal === 'can') return past ? "couldn't" : "can't";
	if (modal === 'should') return "shouldn't";
	if (modal === 'will') return past ? "wouldn't" : "won't";
	return null;
}

function fvBeNegativeContraction(starter, past) {
	if (past) return starter.agreement === 'plural' ? "weren't" : "wasn't";
	if (starter.agreement === 'first_singular') return null;
	return starter.agreement === 'plural' ? "aren't" : "isn't";
}

function fvHaveNegativeContraction(starter, past) {
	if (past) return "hadn't";
	return starter.agreement === 'third_singular' ? "hasn't" : "haven't";
}

function fvDoNegativeContraction(starter, past) {
	if (past) return "didn't";
	return starter.agreement === 'third_singular' ? "doesn't" : "don't";
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

function appendBeContractionComplement(words, structureKind, ender) {
	const verb = ender.verbObj;
	if (structureKind === 'simple' && verb && fvVerbKind(verb) === 'be') {
		appendWord(words, ender.suffix);
		return true;
	}
	if (structureKind === 'progressive') {
		return !verb || appendVerbAndSuffix(words, verb.present_participle, ender.suffix);
	}
	return false;
}

function appendHaveContractionComplement(words, structureKind, ender) {
	const verb = ender.verbObj;
	if (structureKind === 'perfect') {
		return !verb || appendVerbAndSuffix(words, verb.past_participle, ender.suffix);
	}
	if (structureKind === 'perfect_progressive') {
		appendWord(words, 'been');
		return !verb || appendVerbAndSuffix(words, verb.present_participle, ender.suffix);
	}
	return false;
}

function buildFvLong(starter, op, structureKind, ender) {
	const verb = ender.verbObj;
	if (op.modal === 'none' && structureKind === 'simple' && !verb) return null;
	const words = [starter.text];
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

function buildFvContraction(starter, op, structureKind, ender) {
	const verb = ender.verbObj;
	const words = [];
	if (op.modal !== 'none') {
		if (op.negative) {
			const modal = fvModalNegativeContraction(op.modal, ender.past);
			if (!modal) return null;
			appendWord(words, starter.text);
			appendWord(words, modal);
			return appendModalComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
		}
		if (op.modal === 'will') {
			const contraction = ender.past ? starter.d_contraction : starter.will_contraction;
			if (!contraction) return null;
			appendWord(words, contraction);
			return appendModalComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
		}
		return null;
	}
	if (op.negative && structureKind === 'simple' && verb && fvVerbKind(verb) !== 'be') {
		appendWord(words, starter.text);
		appendWord(words, fvDoNegativeContraction(starter, ender.past));
		appendVerbAndSuffix(words, verb.base, ender.suffix);
		return phraseFromWords(words);
	}
	if (op.negative
		&& (structureKind === 'progressive'
			|| (structureKind === 'simple' && verb && fvVerbKind(verb) === 'be'))) {
		if (starter.agreement === 'first_singular' && !ender.past) {
			if (!starter.be_contraction) return null;
			appendWord(words, starter.be_contraction);
			appendWord(words, 'not');
			return appendBeContractionComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
		}
		const negative = fvBeNegativeContraction(starter, ender.past);
		if (!negative) return null;
		appendWord(words, starter.text);
		appendWord(words, negative);
		return appendBeContractionComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
	}
	if (!op.negative
		&& !ender.past
		&& (structureKind === 'progressive'
			|| (structureKind === 'simple' && verb && fvVerbKind(verb) === 'be'))) {
		if (!starter.be_contraction) return null;
		appendWord(words, starter.be_contraction);
		return appendBeContractionComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
	}
	if (structureKind === 'perfect' || structureKind === 'perfect_progressive') {
		if (op.negative) {
			const negative = fvHaveNegativeContraction(starter, ender.past);
			if (!negative) return null;
			appendWord(words, starter.text);
			appendWord(words, negative);
			return appendHaveContractionComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
		}
		const contraction = ender.past ? starter.d_contraction : starter.have_contraction;
		if (!contraction) return null;
		appendWord(words, contraction);
		return appendHaveContractionComplement(words, structureKind, ender) ? phraseFromWords(words) : null;
	}
	return null;
}

function selectedFinalVerbRows(bank, valueForRow) {
	return (phraseData.final_verbs[bank] || []).filter(function(row, index) {
		const value = valueForRow ? valueForRow(row, index) : row.stroke;
		return bankOptionChecked('final_verbs', bank, value);
	});
}

function selectedFinalVerbModes() {
	return ['long', 'contraction'].filter(function(mode) {
		return bankOptionChecked('final_verbs', 'modes', mode);
	});
}

function starterAllowsEnder(starter, ender) {
	if (!Array.isArray(starter.enders)) return true;
	const enderBits = parseStrokeBits(ender.stroke);
	return starter.enders.some(function(allowedStroke) {
		return parseStrokeBits(allowedStroke) === enderBits;
	});
}

function generateFinalVerbPrompts() {
	if (!familyEnabled('final_verbs')) return [];
	const finalVerbs = phraseData.final_verbs;
	const starters = selectedFinalVerbRows('starters');
	const operators = selectedFinalVerbRows('operators');
	const structures = selectedFinalVerbRows('structures');
	const enders = selectedFinalVerbRows('enders', function(row, index) { return String(index); }).map(function(ender) {
		return Object.assign({}, ender, { verbObj: finalVerbByID(ender.verb) });
	});
	const modes = selectedFinalVerbModes();
	const prompts = [];
	starters.forEach(function(starter) {
		operators.forEach(function(op) {
			structures.forEach(function(structure) {
				enders.forEach(function(ender) {
					if (!starterAllowsEnder(starter, ender)) return;
					let longStroke = '';
					try {
						longStroke = combineStrokeParts([starter.stroke, op.stroke, structure.stroke, ender.stroke]);
					} catch (error) {
						return;
					}
					modes.forEach(function(mode) {
						let stroke = longStroke;
						if (mode === 'contraction') {
							try {
								stroke = combineStrokeParts([
									finalVerbs.contraction_stroke,
									starter.stroke,
									op.stroke,
									structure.stroke,
									ender.stroke,
								]);
							} catch (error) {
								return;
							}
						}
						const phrase = mode === 'contraction'
							? buildFvContraction(starter, op, structure.kind, ender)
							: buildFvLong(starter, op, structure.kind, ender);
						if (!phrase) return;
						prompts.push({
							lesson: familyLabels.final_verbs,
							stroke: stroke,
							phrase: phrase,
						});
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

function currentPool() {
	return uniquePrompts([]
		.concat(generateInitialVerbPrompts())
		.concat(generateFinalVerbPrompts())
		.concat(generateNonverbPrompts()));
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
		hints: phraseHintsSelect.value,
		showOutlines: phraseShowOutlines.checked,
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
	if (typeof settings.hints === 'string' && optionExists(phraseHintsSelect, settings.hints)) {
		phraseHintsSelect.value = settings.hints;
	}
	if (typeof settings.showOutlines === 'boolean') {
		phraseShowOutlines.checked = settings.showOutlines;
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

function rebuildPhraseQueue() {
	syncFocusSelectionFromInputs();
	populateFocusOptions();
	const pool = currentPool();
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
	if (order === 'selected') {
		phraseSetSummary.textContent = pool.length + ' generated prompt'
			+ (pool.length === 1 ? '' : 's')
			+ ' x ' + repetitions + ' each'
			+ ' = ' + phraseQueue.length + ' exercises';
	} else {
		phraseSetSummary.textContent = pool.length + ' prompt'
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
		['stems', 'forms'].forEach(function(field) {
			if (tail[field] === undefined) return;
			if (!Array.isArray(tail[field]) || tail[field].some(function(value) { return typeof value !== 'string'; })) {
				throw new Error('initial_verbs.tails[' + index + '].' + field + ' must be an array of stroke strings');
			}
			if (new Set(tail[field]).size !== tail[field].length) {
				throw new Error('initial_verbs.tails[' + index + '].' + field + ' must not contain duplicates');
			}
		});
		tailIds.add(tail.id);
	});
	data.initial_verbs.stems.forEach(function(stem, index) {
		if (stem.tails === undefined) return;
		if (!Array.isArray(stem.tails)) {
			throw new Error('initial_verbs.stems[' + index + '].tails must be an array');
		}
		const seenTailIds = new Set();
		stem.tails.forEach(function(tailId) {
			if (typeof tailId !== 'string' || !tailIds.has(tailId)) {
				throw new Error('initial_verbs.stems[' + index + '].tails references unknown tail ' + String(tailId));
			}
			if (seenTailIds.has(tailId)) {
				throw new Error('initial_verbs.stems[' + index + '].tails duplicates ' + tailId);
			}
			seenTailIds.add(tailId);
		});
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
		data.nonverbs.prefixes.forEach(function(prefix, index) {
			if (!prefix || !Array.isArray(prefix.tails)) {
				throw new Error('nonverbs.prefixes[' + index + '].tails must be an array');
			}
			const seenTailIds = new Set();
			prefix.tails.forEach(function(tailId) {
				if (typeof tailId !== 'string' || !nonverbTailIds.has(tailId)) {
					throw new Error('nonverbs.prefixes[' + index + '].tails references unknown tail ' + String(tailId));
				}
				if (seenTailIds.has(tailId)) {
					throw new Error('nonverbs.prefixes[' + index + '].tails duplicates ' + tailId);
				}
				seenTailIds.add(tailId);
			});
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
		['be_contraction', 'have_contraction', 'will_contraction', 'd_contraction'].forEach(function(field) {
			if (starter[field] !== undefined && typeof starter[field] !== 'string') {
				throw new Error('final_verbs.starters[' + index + '].' + field + ' must be a string');
			}
		});
		if (starter.enders === undefined) return;
		if (!Array.isArray(starter.enders)) {
			throw new Error('final_verbs.starters[' + index + '].enders must be an array');
		}
		const seenEnderBits = new Set();
		starter.enders.forEach(function(enderStroke, allowedIndex) {
			if (typeof enderStroke !== 'string') {
				throw new Error('final_verbs.starters[' + index + '].enders[' + allowedIndex + '] must be a string');
			}
			let bits = 0;
			try {
				bits = parseStrokeBits(enderStroke);
			} catch (error) {
				throw new Error('final_verbs.starters[' + index + '].enders[' + allowedIndex + '] has invalid outline ' + enderStroke);
			}
			if (!enderBits.has(bits)) {
				throw new Error('final_verbs.starters[' + index + '].enders references unknown ender ' + enderStroke);
			}
			if (seenEnderBits.has(bits)) {
				throw new Error('final_verbs.starters[' + index + '].enders duplicates ' + enderStroke);
			}
			seenEnderBits.add(bits);
		});
	});
	if (typeof data.final_verbs.contraction_stroke !== 'string') {
		throw new Error('final_verbs.contraction_stroke must be a string');
	}
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
phraseRestart.addEventListener('click', rebuildPhraseQueue);
phraseReroll.addEventListener('click', rebuildPhraseQueue);
phraseAnswer.addEventListener('input', advancePhraseIfCorrect);
phraseAnswer.addEventListener('keydown', function(event) {
	if (event.key === 'Enter' && phraseIndex >= phraseQueue.length) {
		rebuildPhraseQueue();
	}
});
loadPhraseData();
