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
];

const phraseVerbs = [
	{ id: 'be', stroke: 'B', base: 'be', present3ps: 'is', past: 'was', presentParticiple: 'being', pastParticiple: 'been', be: true },
	{ id: 'go', stroke: 'G', base: 'go', present3ps: 'goes', past: 'went', presentParticiple: 'going', pastParticiple: 'gone', be: false },
	{ id: 'believe', stroke: 'BL', base: 'believe', present3ps: 'believes', past: 'believed', presentParticiple: 'believing', pastParticiple: 'believed', be: false },
	{ id: 'understand', stroke: 'RPB', base: 'understand', present3ps: 'understands', past: 'understood', presentParticiple: 'understanding', pastParticiple: 'understood', be: false },
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
