const phraseTails = [
	{ id: 'none', stroke: '', text: '', name: 'none' },
	{ id: 'the', stroke: 'T', text: 'the', name: 'the' },
	{ id: 'a', stroke: 'B', text: 'a', name: 'a' },
	{ id: 'an', stroke: 'PB', text: 'an', name: 'an' },
	{ id: 'it', stroke: 'P', text: 'it', name: 'it' },
	{ id: 'that', stroke: 'RT', text: 'that', name: 'that' },
	{ id: 'this', stroke: 'TS', text: 'this', name: 'this' },
	{ id: 'these', stroke: 'SZ', text: 'these', name: 'these' },
	{ id: 'those', stroke: 'TZ', text: 'those', name: 'those' },
	{ id: 'me', stroke: 'PL', text: 'me', name: 'me' },
	{ id: 'you', stroke: 'RP', text: 'you', name: 'you' },
	{ id: 'your', stroke: 'R', text: 'your', name: 'your' },
	{ id: 'us', stroke: 'S', text: 'us', name: 'us' },
	{ id: 'her', stroke: 'FR', text: 'her', name: 'her' },
	{ id: 'him', stroke: 'FL', text: 'him', name: 'him' },
	{ id: 'them', stroke: 'PLT', text: 'them', name: 'them' },
	{ id: 'all', stroke: 'L', text: 'all', name: 'all' },
	{ id: 'one', stroke: 'PBT', text: 'one', name: 'one' },
];

const phraseLessons = [
	{ name: '1. Present IV tails', detail: 'is <tail>', tense: 'present', tailIDs: 'all' },
	{ name: '2. Past IV tails', detail: 'was <tail>', tense: 'past', tailIDs: 'all' },
	{ name: '3. Mixed IV be', detail: 'is/was <tail>', tense: 'mixed', tailIDs: 'all' },
];

const phraseLessonSelect = document.getElementById('phrase-lesson');
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

function byID(list, id) {
	return list.find(function(item) { return item.id === id; });
}

function tailsForLesson(lesson) {
	if (lesson.tailIDs === 'all') return phraseTails.slice();
	return lesson.tailIDs.map(function(id) { return byID(phraseTails, id); }).filter(Boolean);
}

function canonicalRightStroke(stroke) {
	const order = 'FRPBLGTSDZ';
	let out = '';
	for (let i = 0; i < order.length; i++) {
		if (stroke.indexOf(order[i]) !== -1) out += order[i];
	}
	return out;
}

function phraseStroke(tense, tail) {
	const right = canonicalRightStroke(tail.stroke + (tense === 'past' ? 'D' : ''));
	return right ? 'PW-' + right : 'PW';
}

function makePhrase(tense, tail, lessonName) {
	const be = tense === 'past' ? 'was' : 'is';
	return {
		stroke: phraseStroke(tense, tail),
		phrase: appendWords(be, tail.text),
		lesson: lessonName,
	};
}

function promptsForLesson(index) {
	const lesson = phraseLessons[index] || phraseLessons[0];
	const tails = tailsForLesson(lesson);
	if (lesson.tense === 'mixed') {
		return tails.flatMap(function(tail) {
			return [
				makePhrase('present', tail, lesson.name),
				makePhrase('past', tail, lesson.name),
			];
		});
	}
	return tails.map(function(tail) {
		return makePhrase(lesson.tense, tail, lesson.name);
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

function repeatedShuffledPasses(pool, repetitions) {
	if (!pool.length || repetitions <= 0) return [];
	const out = [];
	for (let i = 0; i < repetitions; i++) {
		out.push.apply(out, shuffle(pool));
	}
	return out;
}

function selectedLessonIndex() {
	return Number(phraseLessonSelect.value || '0');
}

function currentPool() {
	return uniquePrompts(promptsForLesson(selectedLessonIndex()));
}

function rebuildPhraseQueue() {
	const pool = currentPool();
	const repetitions = Math.max(1, Math.min(100, Number(phraseCountInput.value || '7')));
	phraseQueue = repeatedShuffledPasses(pool, repetitions);
	phraseIndex = 0;
	phraseMistake = false;
	phraseAnswer.value = '';
	phraseSetSummary.textContent = pool.length + ' prompt'
		+ (pool.length === 1 ? '' : 's')
		+ ' x ' + repetitions
		+ ' = ' + phraseQueue.length + ' exercises';
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
}

populatePhraseControls();
phraseLessonSelect.addEventListener('change', rebuildPhraseQueue);
phraseCountInput.addEventListener('change', rebuildPhraseQueue);
phraseHintsSelect.addEventListener('change', renderPhraseTrainer);
phraseRestart.addEventListener('click', rebuildPhraseQueue);
phraseReroll.addEventListener('click', rebuildPhraseQueue);
phraseAnswer.addEventListener('input', advancePhraseIfCorrect);
phraseAnswer.addEventListener('keydown', function(event) {
	if (event.key === 'Enter' && phraseIndex >= phraseQueue.length) {
		rebuildPhraseQueue();
	}
});
rebuildPhraseQueue();
