const phraseStarters = [
	{ id: 'empty', stroke: '', text: '', form: 'empty', name: 'empty' },
	{ id: 'i', stroke: 'S', text: 'I', form: 'first', name: 'I' },
	{ id: 'we', stroke: 'W', text: 'we', form: 'plural', name: 'we' },
	{ id: 'he', stroke: 'K', text: 'he', form: 'third', name: 'he' },
	{ id: 'she', stroke: 'SK', text: 'she', form: 'third', name: 'she' },
	{ id: 'it', stroke: 'P', text: 'it', form: 'third', name: 'it' },
	{ id: 'they', stroke: 'T', text: 'they', form: 'plural', name: 'they' },
	{ id: 'that', stroke: 'ST', text: 'that', form: 'third', name: 'that' },
	{ id: 'you', stroke: 'PW', text: 'you', form: 'plural', name: 'you' },
	{ id: 'this', stroke: 'TP', text: 'this', form: 'third', name: 'this' },
	{ id: 'there_singular', stroke: 'TK', text: 'there', form: 'third', name: 'there singular' },
	{ id: 'there_plural', stroke: 'TKW', text: 'there', form: 'plural', name: 'there plural' },
	{ id: 'blank_plural', stroke: 'SW', text: '', form: 'plural', name: 'blank plural' },
	{ id: 'who', stroke: 'KW', text: 'who', form: 'third', name: 'who' },
	{ id: 'what', stroke: 'TW', text: 'what', form: 'third', name: 'what' },
	{ id: 'someone', stroke: 'SP', text: 'someone', form: 'third', name: 'someone' },
	{ id: 'something', stroke: 'SPW', text: 'something', form: 'third', name: 'something' },
	{ id: 'everyone', stroke: 'KP', text: 'everyone', form: 'third', name: 'everyone' },
	{ id: 'everything', stroke: 'KPW', text: 'everything', form: 'third', name: 'everything' },
	{ id: 'nobody', stroke: 'SKP', text: 'nobody', form: 'third', name: 'nobody' },
	{ id: 'nothing', stroke: 'SKPW', text: 'nothing', form: 'third', name: 'nothing' },
];

const phraseVerbs = [
	{ id: 'have', stroke: 'F', base: 'have', present3ps: 'has', past: 'had', presentParticiple: 'having', pastParticiple: 'had', be: false },
	{ id: 'run', stroke: 'R', base: 'run', present3ps: 'runs', past: 'ran', presentParticiple: 'running', pastParticiple: 'run', be: false },
	{ id: 'want', stroke: 'P', base: 'want', present3ps: 'wants', past: 'wanted', presentParticiple: 'wanting', pastParticiple: 'wanted', be: false },
	{ id: 'be', stroke: 'B', base: 'be', present3ps: 'is', past: 'was', presentParticiple: 'being', pastParticiple: 'been', be: true },
	{ id: 'look', stroke: 'L', base: 'look', present3ps: 'looks', past: 'looked', presentParticiple: 'looking', pastParticiple: 'looked', be: false },
	{ id: 'go', stroke: 'G', base: 'go', present3ps: 'goes', past: 'went', presentParticiple: 'going', pastParticiple: 'gone', be: false },
	{ id: 'see', stroke: 'FR', base: 'see', present3ps: 'sees', past: 'saw', presentParticiple: 'seeing', pastParticiple: 'seen', be: false },
	{ id: 'happen', stroke: 'FP', base: 'happen', present3ps: 'happens', past: 'happened', presentParticiple: 'happening', pastParticiple: 'happened', be: false },
	{ id: 'say', stroke: 'FB', base: 'say', present3ps: 'says', past: 'said', presentParticiple: 'saying', pastParticiple: 'said', be: false },
	{ id: 'feel', stroke: 'FL', base: 'feel', present3ps: 'feels', past: 'felt', presentParticiple: 'feeling', pastParticiple: 'felt', be: false },
	{ id: 'come', stroke: 'FG', base: 'come', present3ps: 'comes', past: 'came', presentParticiple: 'coming', pastParticiple: 'come', be: false },
	{ id: 'do', stroke: 'RP', base: 'do', present3ps: 'does', past: 'did', presentParticiple: 'doing', pastParticiple: 'done', be: false },
	{ id: 'ask', stroke: 'RB', base: 'ask', present3ps: 'asks', past: 'asked', presentParticiple: 'asking', pastParticiple: 'asked', be: false },
	{ id: 'recall', stroke: 'RL', base: 'recall', present3ps: 'recalls', past: 'recalled', presentParticiple: 'recalling', pastParticiple: 'recalled', be: false },
	{ id: 'forget', stroke: 'RG', base: 'forget', present3ps: 'forgets', past: 'forgot', presentParticiple: 'forgetting', pastParticiple: 'forgotten', be: false },
	{ id: 'know', stroke: 'PB', base: 'know', present3ps: 'knows', past: 'knew', presentParticiple: 'knowing', pastParticiple: 'known', be: false },
	{ id: 'move', stroke: 'PL', base: 'move', present3ps: 'moves', past: 'moved', presentParticiple: 'moving', pastParticiple: 'moved', be: false },
	{ id: 'get', stroke: 'PG', base: 'get', present3ps: 'gets', past: 'got', presentParticiple: 'getting', pastParticiple: 'got', be: false },
	{ id: 'believe', stroke: 'BL', base: 'believe', present3ps: 'believes', past: 'believed', presentParticiple: 'believing', pastParticiple: 'believed', be: false },
	{ id: 'become', stroke: 'BG', base: 'become', present3ps: 'becomes', past: 'became', presentParticiple: 'becoming', pastParticiple: 'become', be: false },
	{ id: 'love', stroke: 'LG', base: 'love', present3ps: 'loves', past: 'loved', presentParticiple: 'loving', pastParticiple: 'loved', be: false },
	{ id: 'read', stroke: 'FRP', base: 'read', present3ps: 'reads', past: 'read', presentParticiple: 'reading', pastParticiple: 'read', be: false },
	{ id: 'care', stroke: 'FRB', base: 'care', present3ps: 'cares', past: 'cared', presentParticiple: 'caring', pastParticiple: 'cared', be: false },
	{ id: 'try', stroke: 'FRPB', base: 'try', present3ps: 'tries', past: 'tried', presentParticiple: 'trying', pastParticiple: 'tried', be: false },
	{ id: 'change', stroke: 'FRL', base: 'change', present3ps: 'changes', past: 'changed', presentParticiple: 'changing', pastParticiple: 'changed', be: false },
	{ id: 'consider', stroke: 'FRG', base: 'consider', present3ps: 'considers', past: 'considered', presentParticiple: 'considering', pastParticiple: 'considered', be: false },
	{ id: 'expect', stroke: 'FPB', base: 'expect', present3ps: 'expects', past: 'expected', presentParticiple: 'expecting', pastParticiple: 'expected', be: false },
	{ id: 'hope', stroke: 'FPL', base: 'hope', present3ps: 'hopes', past: 'hoped', presentParticiple: 'hoping', pastParticiple: 'hoped', be: false },
	{ id: 'hear', stroke: 'FPG', base: 'hear', present3ps: 'hears', past: 'heard', presentParticiple: 'hearing', pastParticiple: 'heard', be: false },
	{ id: 'keep', stroke: 'FBL', base: 'keep', present3ps: 'keeps', past: 'kept', presentParticiple: 'keeping', pastParticiple: 'kept', be: false },
	{ id: 'learn', stroke: 'FBG', base: 'learn', present3ps: 'learns', past: 'learned', presentParticiple: 'learning', pastParticiple: 'learned', be: false },
	{ id: 'leave', stroke: 'FLG', base: 'leave', present3ps: 'leaves', past: 'left', presentParticiple: 'leaving', pastParticiple: 'left', be: false },
	{ id: 'understand', stroke: 'RPB', base: 'understand', present3ps: 'understands', past: 'understood', presentParticiple: 'understanding', pastParticiple: 'understood', be: false },
	{ id: 'remember', stroke: 'RPL', base: 'remember', present3ps: 'remembers', past: 'remembered', presentParticiple: 'remembering', pastParticiple: 'remembered', be: false },
	{ id: 'need', stroke: 'RPG', base: 'need', present3ps: 'needs', past: 'needed', presentParticiple: 'needing', pastParticiple: 'needed', be: false },
	{ id: 'take', stroke: 'RBL', base: 'take', present3ps: 'takes', past: 'took', presentParticiple: 'taking', pastParticiple: 'taken', be: false },
	{ id: 'work', stroke: 'RBG', base: 'work', present3ps: 'works', past: 'worked', presentParticiple: 'working', pastParticiple: 'worked', be: false },
	{ id: 'realize', stroke: 'RLG', base: 'realize', present3ps: 'realizes', past: 'realized', presentParticiple: 'realizing', pastParticiple: 'realized', be: false },
	{ id: 'mean', stroke: 'PBL', base: 'mean', present3ps: 'means', past: 'meant', presentParticiple: 'meaning', pastParticiple: 'meant', be: false },
	{ id: 'think', stroke: 'PBG', base: 'think', present3ps: 'thinks', past: 'thought', presentParticiple: 'thinking', pastParticiple: 'thought', be: false },
	{ id: 'imagine', stroke: 'PLG', base: 'imagine', present3ps: 'imagines', past: 'imagined', presentParticiple: 'imagining', pastParticiple: 'imagined', be: false },
	{ id: 'like', stroke: 'BLG', base: 'like', present3ps: 'likes', past: 'liked', presentParticiple: 'liking', pastParticiple: 'liked', be: false },
	{ id: 'wish', stroke: 'FRPL', base: 'wish', present3ps: 'wishes', past: 'wished', presentParticiple: 'wishing', pastParticiple: 'wished', be: false },
	{ id: 'use', stroke: 'FRPBL', base: 'use', present3ps: 'uses', past: 'used', presentParticiple: 'using', pastParticiple: 'used', be: false },
	{ id: 'give', stroke: 'FRPG', base: 'give', present3ps: 'gives', past: 'gave', presentParticiple: 'giving', pastParticiple: 'given', be: false },
	{ id: 'let', stroke: 'FRBL', base: 'let', present3ps: 'lets', past: 'let', presentParticiple: 'letting', pastParticiple: 'let', be: false },
	{ id: 'tell', stroke: 'FRBG', base: 'tell', present3ps: 'tells', past: 'told', presentParticiple: 'telling', pastParticiple: 'told', be: false },
	{ id: 'live', stroke: 'FRLG', base: 'live', present3ps: 'lives', past: 'lived', presentParticiple: 'living', pastParticiple: 'lived', be: false },
	{ id: 'mind', stroke: 'FPBL', base: 'mind', present3ps: 'minds', past: 'minded', presentParticiple: 'minding', pastParticiple: 'minded', be: false },
	{ id: 'put', stroke: 'FPBG', base: 'put', present3ps: 'puts', past: 'put', presentParticiple: 'putting', pastParticiple: 'put', be: false },
	{ id: 'set', stroke: 'FPLG', base: 'set', present3ps: 'sets', past: 'set', presentParticiple: 'setting', pastParticiple: 'set', be: false },
	{ id: 'seem', stroke: 'FBLG', base: 'seem', present3ps: 'seems', past: 'seemed', presentParticiple: 'seeming', pastParticiple: 'seemed', be: false },
	{ id: 'make', stroke: 'RPBL', base: 'make', present3ps: 'makes', past: 'made', presentParticiple: 'making', pastParticiple: 'made', be: false },
	{ id: 'show', stroke: 'RPBG', base: 'show', present3ps: 'shows', past: 'showed', presentParticiple: 'showing', pastParticiple: 'shown', be: false },
	{ id: 'remain', stroke: 'RPLG', base: 'remain', present3ps: 'remains', past: 'remained', presentParticiple: 'remaining', pastParticiple: 'remained', be: false },
	{ id: 'call', stroke: 'RBLG', base: 'call', present3ps: 'calls', past: 'called', presentParticiple: 'calling', pastParticiple: 'called', be: false },
	{ id: 'find', stroke: 'PBLG', base: 'find', present3ps: 'finds', past: 'found', presentParticiple: 'finding', pastParticiple: 'found', be: false },
];

const phraseTails = [
	{ id: 'none', stroke: '', text: '', name: 'none' },
	{ id: 'the', stroke: 'T', text: 'the', name: 'the' },
	{ id: 'a', stroke: 'S', text: 'a', name: 'a' },
	{ id: 'it', stroke: 'D', text: 'it', name: 'it' },
	{ id: 'that', stroke: 'Z', text: 'that', name: 'that' },
	{ id: 'this', stroke: 'TS', text: 'this', name: 'this' },
	{ id: 'me', stroke: 'TD', text: 'me', name: 'me' },
	{ id: 'those', stroke: 'TZ', text: 'those', name: 'those' },
	{ id: 'her', stroke: 'SD', text: 'her', name: 'her' },
	{ id: 'us', stroke: 'SZ', text: 'us', name: 'us' },
	{ id: 'them', stroke: 'DZ', text: 'them', name: 'them' },
	{ id: 'you', stroke: 'TSD', text: 'you', name: 'you' },
	{ id: 'these', stroke: 'TSZ', text: 'these', name: 'these' },
	{ id: 'him', stroke: 'TDZ', text: 'him', name: 'him' },
	{ id: 'one', stroke: 'SDZ', text: 'one', name: 'one' },
	{ id: 'all', stroke: 'TSDZ', text: 'all', name: 'all' },
];

const phraseLessons = [
	{ name: '1. Verb alone', detail: 'to <verb>' },
	{ name: '2. Tail bank', detail: 'to <verb> <tail>' },
	{ name: '3. Starter bank', detail: '<starter> be' },
	{ name: '4. Starter + tail', detail: '<starter> be <tail>' },
	{ name: '5. Starter + verb', detail: '<starter> <verb>' },
	{ name: '6. Starter + verb + tail', detail: '<starter> <verb> <tail>' },
	{ name: '7. Past bank', detail: '<starter> past <verb>' },
	{ name: '8. Negative bank', detail: '<starter> not <verb>' },
	{ name: '9. Modal bank', detail: 'can / should / will' },
	{ name: '10. Aspect bank', detail: 'progressive / perfect' },
	{ name: '11. Question bank', detail: 'inverted forms' },
	{ name: '12. Mixed core', detail: 'all learned banks' },
];

const phraseLessonSelect = document.getElementById('phrase-lesson');
const phraseVerbSelect = document.getElementById('phrase-verb');
const phraseCountInput = document.getElementById('phrase-count');
const phraseHintsSelect = document.getElementById('phrase-hints');
const phraseRestart = document.getElementById('phrase-restart');
const phraseReroll = document.getElementById('phrase-reroll');
const phraseSetSummary = document.getElementById('phrase-set-summary');
const phraseProgress = document.getElementById('phrase-progress');
const phraseLessonName = document.getElementById('phrase-lesson-name');
const phrasePrompt = document.getElementById('phrase-prompt');
const phraseHint = document.getElementById('phrase-hint');
const phraseAnswer = document.getElementById('phrase-answer');
const phraseStatus = document.getElementById('phrase-status');
let phraseQueue = [];
let phraseIndex = 0;
let phraseMistake = false;

function appendWords() {
	const words = [];
	for (let i = 0; i < arguments.length; i++) {
		if (arguments[i]) words.push(arguments[i]);
	}
	return words.join(' ');
}

function subjectIs3ps(starter) {
	return starter.form === 'empty' || starter.form === 'third';
}

function subjectIsFirstSingular(starter) {
	return starter.form === 'first';
}

function finiteBe(starter, past, negative) {
	if (past) {
		return subjectIs3ps(starter) || subjectIsFirstSingular(starter)
			? (negative ? "wasn't" : 'was')
			: (negative ? "weren't" : 'were');
	}
	if (subjectIsFirstSingular(starter)) return negative ? 'am not' : 'am';
	if (subjectIs3ps(starter)) return negative ? "isn't" : 'is';
	return negative ? "aren't" : 'are';
}

function finiteHave(starter, past, negative) {
	if (past) return negative ? "hadn't" : 'had';
	if (subjectIs3ps(starter)) return negative ? "hasn't" : 'has';
	return negative ? "haven't" : 'have';
}

function doSupport(starter, past, negative) {
	if (past) return negative ? "didn't" : 'did';
	if (subjectIs3ps(starter)) return negative ? "doesn't" : 'does';
	return negative ? "don't" : 'do';
}

function auxWord(grammar) {
	if (grammar.aux === 'can') return grammar.past ? 'could' : 'can';
	if (grammar.aux === 'should') return 'should';
	if (grammar.aux === 'will') return grammar.past ? 'would' : 'will';
	return '';
}

function negativeAuxWord(grammar) {
	if (grammar.aux === 'can') return grammar.past ? "couldn't" : "can't";
	if (grammar.aux === 'should') return "shouldn't";
	if (grammar.aux === 'will') return grammar.past ? "wouldn't" : "won't";
	return '';
}

function simpleVerbForm(starter, verb, past) {
	if (verb.be) return finiteBe(starter, past, false);
	if (past) return verb.past;
	return subjectIs3ps(starter) ? verb.present3ps : verb.base;
}

function simplePredicate(starter, grammar, verb) {
	if (grammar.aux !== 'none') return appendWords(grammar.negative ? negativeAuxWord(grammar) : auxWord(grammar), verb.base);
	if (verb.be) return finiteBe(starter, grammar.past, grammar.negative);
	if (grammar.negative) return appendWords(doSupport(starter, grammar.past, true), verb.base);
	return simpleVerbForm(starter, verb, grammar.past);
}

function progressivePredicate(starter, grammar, verb) {
	if (grammar.aux !== 'none') return appendWords(grammar.negative ? negativeAuxWord(grammar) : auxWord(grammar), 'be', verb.presentParticiple);
	return appendWords(finiteBe(starter, grammar.past, grammar.negative), verb.presentParticiple);
}

function perfectPredicate(starter, grammar, verb) {
	if (grammar.aux !== 'none') return appendWords(grammar.negative ? negativeAuxWord(grammar) : auxWord(grammar), 'have', verb.pastParticiple);
	return appendWords(finiteHave(starter, grammar.past, grammar.negative), verb.pastParticiple);
}

function perfectProgressivePredicate(starter, grammar, verb) {
	if (grammar.aux !== 'none') return appendWords(grammar.negative ? negativeAuxWord(grammar) : auxWord(grammar), 'have', 'been', verb.presentParticiple);
	return appendWords(finiteHave(starter, grammar.past, grammar.negative), 'been', verb.presentParticiple);
}

function bareAuxComplement(grammar, verb) {
	if (grammar.aspect === 'simple') return verb.base;
	if (grammar.aspect === 'progressive') return appendWords('be', verb.presentParticiple);
	if (grammar.aspect === 'perfect') return appendWords('have', verb.pastParticiple);
	return appendWords('have', 'been', verb.presentParticiple);
}

function predicate(starter, grammar, verb) {
	if (grammar.aspect === 'simple') return simplePredicate(starter, grammar, verb);
	if (grammar.aspect === 'progressive') return progressivePredicate(starter, grammar, verb);
	if (grammar.aspect === 'perfect') return perfectPredicate(starter, grammar, verb);
	return perfectProgressivePredicate(starter, grammar, verb);
}

function invertedPredicate(starter, grammar, verb) {
	if (grammar.aspect === 'simple' && grammar.aux === 'none' && !verb.be) {
		return appendWords(doSupport(starter, grammar.past, grammar.negative), starter.text, verb.base);
	}
	if (grammar.aspect === 'simple' && grammar.aux === 'none' && verb.be) {
		return appendWords(finiteBe(starter, grammar.past, grammar.negative), starter.text);
	}
	if (grammar.aux !== 'none') {
		return appendWords(grammar.negative ? negativeAuxWord(grammar) : auxWord(grammar), starter.text, bareAuxComplement(grammar, verb));
	}
	if (grammar.aspect === 'progressive') {
		return appendWords(finiteBe(starter, grammar.past, grammar.negative), starter.text, verb.presentParticiple);
	}
	if (grammar.aspect === 'perfect') {
		return appendWords(finiteHave(starter, grammar.past, grammar.negative), starter.text, verb.pastParticiple);
	}
	return appendWords(finiteHave(starter, grammar.past, grammar.negative), starter.text, 'been', verb.presentParticiple);
}

function grammarStroke(grammar) {
	let stroke = '';
	if (grammar.past) stroke += 'H';
	if (grammar.aspect === 'perfect' || grammar.aspect === 'perfectProgressive') stroke += 'R';
	if (grammar.aux === 'can' || grammar.aux === 'will') stroke += 'A';
	if (grammar.aux === 'should' || grammar.aux === 'will') stroke += 'O';
	if (grammar.negative) stroke += '*';
	if (grammar.aspect === 'progressive' || grammar.aspect === 'perfectProgressive') stroke += 'E';
	if (grammar.inverted) stroke += 'U';
	return stroke;
}

function grammar(options) {
	return {
		past: Boolean(options && options.past),
		negative: Boolean(options && options.negative),
		inverted: Boolean(options && options.inverted),
		aux: options && options.aux ? options.aux : 'none',
		aspect: options && options.aspect ? options.aspect : 'simple',
	};
}

function simpleInfinitive(starter, grammar) {
	return starter.form === 'empty'
		&& !grammar.past
		&& !grammar.negative
		&& !grammar.inverted
		&& grammar.aux === 'none'
		&& grammar.aspect === 'simple';
}

function phraseStroke(starter, grammar, verb, tail) {
	const prefix = starter.stroke + grammarStroke(grammar);
	const right = verb.stroke + tail.stroke;
	if (prefix && right) return prefix + '-' + right;
	if (right) return '-' + right;
	return prefix;
}

function makePhrase(starter, grammar, verb, tail, lesson) {
	const body = simpleInfinitive(starter, grammar)
		? appendWords('to', verb.base)
		: grammar.inverted
			? invertedPredicate(starter, grammar, verb)
			: appendWords(starter.text, predicate(starter, grammar, verb));
	return {
		stroke: phraseStroke(starter, grammar, verb, tail),
		phrase: appendWords(body, tail.text),
		lesson: lesson,
	};
}

function byID(list, id) {
	return list.find(function(item) { return item.id === id; });
}

function focusedVerbs() {
	const value = phraseVerbSelect.value;
	if (value === 'all') return phraseVerbs.slice();
	return [byID(phraseVerbs, value) || phraseVerbs[0]];
}

function nonEmptyStarters() {
	return phraseStarters.filter(function(starter) { return starter.form !== 'empty'; });
}

function nonEmptyTails() {
	return phraseTails.filter(function(tail) { return tail.id !== 'none'; });
}

function coreTails() {
	return phraseTails.filter(function(tail) {
		return ['none', 'the', 'a', 'it', 'that'].indexOf(tail.id) >= 0;
	});
}

function coreTailsWithoutNone() {
	return coreTails().filter(function(tail) { return tail.id !== 'none'; });
}

function eachProduct(a, b, fn) {
	const out = [];
	a.forEach(function(x) {
		b.forEach(function(y) {
			out.push(fn(x, y));
		});
	});
	return out;
}

function promptsForLesson(index, verbs) {
	const be = byID(phraseVerbs, 'be');
	const starters = nonEmptyStarters();
	const tails = nonEmptyTails();
	const basics = coreTails();
	const basicObjects = coreTailsWithoutNone();
	const simple = grammar({});

	if (index === 0) {
		return verbs.map(function(verb) {
			return makePhrase(phraseStarters[0], simple, verb, phraseTails[0], phraseLessons[index].name);
		});
	}
	if (index === 1) {
		return eachProduct(verbs, tails, function(verb, tail) {
			return makePhrase(phraseStarters[0], simple, verb, tail, phraseLessons[index].name);
		});
	}
	if (index === 2) {
		return starters.map(function(starter) {
			return makePhrase(starter, simple, be, phraseTails[0], phraseLessons[index].name);
		});
	}
	if (index === 3) {
		return starters.flatMap(function(starter) {
			return tails.map(function(tail) {
				return makePhrase(starter, simple, be, tail, phraseLessons[index].name);
			});
		});
	}
	if (index === 4) {
		return starters.flatMap(function(starter) {
			return verbs.map(function(verb) {
				return makePhrase(starter, simple, verb, phraseTails[0], phraseLessons[index].name);
			});
		});
	}
	if (index === 5) {
		return starters.flatMap(function(starter) {
			return verbs.flatMap(function(verb) {
				return basicObjects.map(function(tail) {
					return makePhrase(starter, simple, verb, tail, phraseLessons[index].name);
				});
			});
		});
	}
	if (index === 6) {
		const past = grammar({ past: true });
		return starters.flatMap(function(starter) {
			return verbs.flatMap(function(verb) {
				return basics.map(function(tail) {
					return makePhrase(starter, past, verb, tail, phraseLessons[index].name);
				});
			});
		});
	}
	if (index === 7) {
		const negative = grammar({ negative: true });
		const pastNegative = grammar({ past: true, negative: true });
		return starters.flatMap(function(starter) {
			return verbs.flatMap(function(verb) {
				return [negative, pastNegative].map(function(g) {
					return makePhrase(starter, g, verb, phraseTails[0], phraseLessons[index].name);
				});
			});
		});
	}
	if (index === 8) {
		const modals = [grammar({ aux: 'can' }), grammar({ aux: 'should' }), grammar({ aux: 'will' }), grammar({ past: true, aux: 'can' }), grammar({ past: true, aux: 'will' })];
		return starters.flatMap(function(starter) {
			return verbs.flatMap(function(verb) {
				return modals.map(function(g) {
					return makePhrase(starter, g, verb, phraseTails[0], phraseLessons[index].name);
				});
			});
		});
	}
	if (index === 9) {
		const aspects = [grammar({ aspect: 'progressive' }), grammar({ aspect: 'perfect' }), grammar({ aspect: 'perfectProgressive' })];
		return starters.flatMap(function(starter) {
			return verbs.flatMap(function(verb) {
				return aspects.map(function(g) {
					return makePhrase(starter, g, verb, phraseTails[0], phraseLessons[index].name);
				});
			});
		});
	}
	if (index === 10) {
		const questions = [
			grammar({ inverted: true }),
			grammar({ negative: true, inverted: true }),
			grammar({ aux: 'can', inverted: true }),
			grammar({ aspect: 'perfect', inverted: true }),
		];
		return starters.flatMap(function(starter) {
			return verbs.flatMap(function(verb) {
				return questions.map(function(g) {
					return makePhrase(starter, g, verb, phraseTails[0], phraseLessons[index].name);
				});
			});
		});
	}

	const mixedGrammars = [
		grammar({}),
		grammar({ past: true }),
		grammar({ negative: true }),
		grammar({ past: true, negative: true }),
		grammar({ aux: 'can' }),
		grammar({ aux: 'should' }),
		grammar({ aux: 'will' }),
		grammar({ past: true, aux: 'can' }),
		grammar({ past: true, aux: 'will' }),
		grammar({ aspect: 'progressive' }),
		grammar({ aspect: 'perfect' }),
		grammar({ aspect: 'perfectProgressive' }),
		grammar({ inverted: true }),
		grammar({ negative: true, inverted: true }),
		grammar({ aux: 'can', inverted: true }),
	];
	return starters.flatMap(function(starter) {
		return verbs.flatMap(function(verb) {
			return mixedGrammars.flatMap(function(g) {
				return basics.map(function(tail) {
					return makePhrase(starter, g, verb, tail, phraseLessons[index].name);
				});
			});
		});
	});
}

function uniquePrompts(prompts) {
	const seen = new Set();
	const out = [];
	prompts.forEach(function(prompt) {
		const key = prompt.stroke + '\n' + prompt.phrase;
		if (!seen.has(key)) {
			seen.add(key);
			out.push(prompt);
		}
	});
	return out;
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

function randomDraw(pool, count) {
	if (!pool.length || count <= 0) return [];
	const out = [];
	let shuffled = [];
	while (out.length < count) {
		if (shuffled.length === 0) shuffled = shuffle(pool);
		out.push(shuffled.pop());
	}
	return out;
}

function currentMode() {
	const checked = document.querySelector('input[name="phrase-mode"]:checked');
	return checked ? checked.value : 'ordered';
}

function selectedLessonIndex() {
	return Number(phraseLessonSelect.value || '0');
}

function currentPool() {
	const lessonIndex = selectedLessonIndex();
	const verbs = focusedVerbs();
	if (currentMode() === 'ordered') {
		return uniquePrompts(promptsForLesson(lessonIndex, verbs));
	}
	let prompts = [];
	for (let i = 0; i <= lessonIndex; i++) {
		prompts = prompts.concat(promptsForLesson(i, verbs));
	}
	return uniquePrompts(prompts);
}

function rebuildPhraseQueue() {
	const pool = currentPool();
	const mode = currentMode();
	if (mode === 'cumulative') {
		const count = Math.max(1, Math.min(500, Number(phraseCountInput.value || '40')));
		phraseQueue = randomDraw(pool, count);
	} else {
		phraseQueue = pool;
	}
	phraseIndex = 0;
	phraseMistake = false;
	phraseAnswer.value = '';
	phraseSetSummary.textContent = pool.length + ' available prompt' + (pool.length === 1 ? '' : 's');
	renderPhraseTrainer();
}

function normalizeAnswer(text) {
	return text.trim();
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
	return prompt.phrase.startsWith(answer);
}

function renderPhraseTrainer() {
	const prompt = currentPhrase();
	phraseAnswer.classList.remove('wrong');
	if (!phraseQueue.length) {
		phraseProgress.textContent = '0 / 0';
		phraseLessonName.textContent = '';
		phrasePrompt.textContent = 'No phrases available.';
		phraseHint.textContent = '';
		phraseHint.classList.add('hidden');
		phraseStatus.textContent = '';
		phraseAnswer.disabled = true;
		return;
	}
	if (!prompt) {
		phraseProgress.textContent = phraseQueue.length + ' / ' + phraseQueue.length;
		phraseLessonName.textContent = '';
		phrasePrompt.textContent = 'Done.';
		phraseHint.textContent = '';
		phraseHint.classList.add('hidden');
		phraseStatus.innerHTML = '<span class="phrase-done">Finished this set.</span>';
		phraseAnswer.disabled = true;
		return;
	}
	const typed = normalizeAnswer(phraseAnswer.value);
	phraseProgress.textContent = (phraseIndex + 1) + ' / ' + phraseQueue.length;
	phraseLessonName.textContent = prompt.lesson;
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
	if (typed === prompt.phrase) {
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

function populatePhraseControls() {
	phraseLessons.forEach(function(lesson, index) {
		const option = document.createElement('option');
		option.value = String(index);
		option.textContent = lesson.name + ' - ' + lesson.detail;
		phraseLessonSelect.appendChild(option);
	});
	phraseVerbs.forEach(function(verb) {
		const option = document.createElement('option');
		option.value = verb.id;
		option.textContent = verb.base + ' (' + verb.stroke + ')';
		phraseVerbSelect.appendChild(option);
	});
	const all = document.createElement('option');
	all.value = 'all';
	all.textContent = 'all implemented verbs';
	phraseVerbSelect.appendChild(all);
}

populatePhraseControls();
phraseLessonSelect.addEventListener('change', rebuildPhraseQueue);
phraseVerbSelect.addEventListener('change', rebuildPhraseQueue);
phraseCountInput.addEventListener('change', rebuildPhraseQueue);
phraseHintsSelect.addEventListener('change', renderPhraseTrainer);
document.querySelectorAll('input[name="phrase-mode"]').forEach(function(input) {
	input.addEventListener('change', rebuildPhraseQueue);
});
phraseRestart.addEventListener('click', rebuildPhraseQueue);
phraseReroll.addEventListener('click', rebuildPhraseQueue);
phraseAnswer.addEventListener('input', advancePhraseIfCorrect);
phraseAnswer.addEventListener('keydown', function(event) {
	if (event.key === 'Enter' && phraseIndex >= phraseQueue.length) {
		rebuildPhraseQueue();
	}
});
rebuildPhraseQueue();
