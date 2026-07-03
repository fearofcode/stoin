const phraseAssignments = [
	{ lesson: 'iv', stroke: 'PW-B', phrase: 'is a' },
	{ lesson: 'iv', stroke: 'PW-BD', phrase: 'was a' },
	{ lesson: 'iv', stroke: 'PWE-B', phrase: 'are a' },
	{ lesson: 'iv', stroke: 'PWE-BD', phrase: 'were a' },
	{ lesson: 'iv', stroke: 'PW-T', phrase: 'is the' },
	{ lesson: 'iv', stroke: 'PW-TD', phrase: 'was the' },
	{ lesson: 'iv', stroke: 'PWE-T', phrase: 'are the' },
	{ lesson: 'iv', stroke: 'PWE-TD', phrase: 'were the' },
	{ lesson: 'iv', stroke: 'PW-P', phrase: 'is it' },
	{ lesson: 'iv', stroke: 'PW-PD', phrase: 'was it' },
	{ lesson: 'iv', stroke: 'PWE-P', phrase: 'are it' },
	{ lesson: 'iv', stroke: 'PWE-PD', phrase: 'were it' },
	{ lesson: 'iv', stroke: 'PW-RT', phrase: 'is that' },
	{ lesson: 'iv', stroke: 'PW-RTD', phrase: 'was that' },
	{ lesson: 'iv', stroke: 'PWE-RT', phrase: 'are that' },
	{ lesson: 'iv', stroke: 'PWE-RTD', phrase: 'were that' },
	{ lesson: 'iv', stroke: 'H-B', phrase: 'has a' },
	{ lesson: 'iv', stroke: 'H-BD', phrase: 'had a' },
	{ lesson: 'iv', stroke: 'HE-B', phrase: 'have a' },
	{ lesson: 'iv', stroke: 'H-T', phrase: 'has the' },
	{ lesson: 'iv', stroke: 'H-TD', phrase: 'had the' },
	{ lesson: 'iv', stroke: 'HE-T', phrase: 'have the' },
	{ lesson: 'iv', stroke: 'H-P', phrase: 'has it' },
	{ lesson: 'iv', stroke: 'H-PD', phrase: 'had it' },
	{ lesson: 'iv', stroke: 'HE-P', phrase: 'have it' },
	{ lesson: 'iv', stroke: 'H-RT', phrase: 'has that' },
	{ lesson: 'iv', stroke: 'H-RTD', phrase: 'had that' },
	{ lesson: 'iv', stroke: 'HE-RT', phrase: 'have that' },

	{ lesson: 'fv-core', stroke: 'SKWHR-B', phrase: 'she is' },
	{ lesson: 'fv-core', stroke: 'SKWHR-BD', phrase: 'she was' },
	{ lesson: 'fv-core', stroke: 'KWHR-B', phrase: 'he is' },
	{ lesson: 'fv-core', stroke: 'TWH-BD', phrase: 'they were' },
	{ lesson: 'fv-core', stroke: 'SWR-F', phrase: 'I have' },
	{ lesson: 'fv-core', stroke: 'SWR-FD', phrase: 'I had' },
	{ lesson: 'fv-core', stroke: 'KPWR-G', phrase: 'you go' },
	{ lesson: 'fv-core', stroke: 'KPWR-GD', phrase: 'you went' },
	{ lesson: 'fv-core', stroke: 'SKWHR-PBG', phrase: 'she thinks' },
	{ lesson: 'fv-core', stroke: 'SKWHR-PBGD', phrase: 'she thought' },

	{ lesson: 'fv-operators', stroke: 'SKWHR*E', phrase: 'she is not' },
	{ lesson: 'fv-operators', stroke: 'SKWHR*ED', phrase: 'she was not' },
	{ lesson: 'fv-operators', stroke: 'SKWHRAO-G', phrase: 'she will go' },
	{ lesson: 'fv-operators', stroke: 'SKWHRAO*G', phrase: 'she will not go' },
	{ lesson: 'fv-operators', stroke: 'SKWHREG', phrase: 'she is going' },
	{ lesson: 'fv-operators', stroke: 'SKWHR-FG', phrase: 'she has gone' },
	{ lesson: 'fv-operators', stroke: 'SKWHR-GTD', phrase: 'she went to' },
	{ lesson: 'fv-operators', stroke: 'KPWR-PBT', phrase: 'you know that' },
	{ lesson: 'fv-operators', stroke: 'TWH-TS', phrase: 'they have to' },

	{ lesson: 'fv-contractions', stroke: '#SKWHR-B', phrase: "she's" },
	{ lesson: 'fv-contractions', stroke: '#SKWHR*E', phrase: "she isn't" },
	{ lesson: 'fv-contractions', stroke: '#SKWHR*ED', phrase: "she wasn't" },
	{ lesson: 'fv-contractions', stroke: '#SKWHRAO-G', phrase: "she'll go" },
	{ lesson: 'fv-contractions', stroke: '#SKWHRAO*G', phrase: "she won't go" },
	{ lesson: 'fv-contractions', stroke: '#SWR-F', phrase: "I've" },
	{ lesson: 'fv-contractions', stroke: '#KWHR-FG', phrase: "he's gone" },
	{ lesson: 'fv-contractions', stroke: '#TWHAO-G', phrase: "they'll go" },

	{ lesson: 'nv-immediate', stroke: 'TW-B', phrase: 'with a' },
	{ lesson: 'nv-immediate', stroke: 'TW-T', phrase: 'with the' },
	{ lesson: 'nv-immediate', stroke: 'TW-PLT', phrase: 'with them' },
	{ lesson: 'nv-immediate', stroke: 'TW-RT', phrase: 'with that' },
	{ lesson: 'nv-immediate', stroke: 'TKPWH*-RT', phrase: 'anything that' },
	{ lesson: 'nv-immediate', stroke: 'TKPWH*-F', phrase: 'anything else' },
	{ lesson: 'nv-immediate', stroke: 'SRAO*E-B', phrase: 'even a' },
	{ lesson: 'nv-immediate', stroke: 'SRAO*E-RT', phrase: 'even that' },

	{ lesson: 'nv-functions', stroke: 'S*-F', phrase: 'as if' },
	{ lesson: 'nv-functions', stroke: 'S*-GT', phrase: 'as though' },
	{ lesson: 'nv-functions', stroke: 'SRAO*E-F', phrase: 'even if' },
	{ lesson: 'nv-functions', stroke: 'SRAO*E-GT', phrase: 'even though' },
	{ lesson: 'nv-functions', stroke: 'TPHORTD', phrase: 'in order to' },
	{ lesson: 'nv-functions', stroke: 'STPHEFD', phrase: 'instead of' },
];

const phraseLessons = [
	{ name: '1. IV Set 1', detail: 'is/was/are/were and has/had/have rows', lessonIDs: ['iv'] },
	{ name: '2. FV core', detail: 'common long final-verb phrases', lessonIDs: ['fv-core'] },
	{ name: '3. FV operators', detail: 'not, will, progressive, perfect, suffixes', lessonIDs: ['fv-operators'] },
	{ name: '4. FV contractions', detail: '# contraction forms only', lessonIDs: ['fv-contractions'] },
	{ name: '5. NV immediate', detail: 'TW with, TKPWH* anything, SRAO*E even', lessonIDs: ['nv-immediate'] },
	{ name: '6. NV functions', detail: 'S* as, SRAO*E even, and two custom chunks', lessonIDs: ['nv-functions'] },
	{ name: '7. All implemented', detail: 'IV, FV, and NV Set 1', lessonIDs: 'all' },
];

const phraseLessonSelect = document.getElementById('phrase-lesson');
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
const phraseLessonName = document.getElementById('phrase-lesson-name');
const phrasePrompt = document.getElementById('phrase-prompt');
const phraseHint = document.getElementById('phrase-hint');
const phraseAnswer = document.getElementById('phrase-answer');
const phraseStatus = document.getElementById('phrase-status');
let phraseQueue = [];
let phraseIndex = 0;
let phraseMistake = false;
const phraseCheckedByKey = new Map();

function promptsForLesson(index) {
	const lesson = phraseLessons[index] || phraseLessons[0];
	if (lesson.lessonIDs === 'all') return phraseAssignments.slice();
	const ids = new Set(lesson.lessonIDs);
	return phraseAssignments.filter(function(prompt) { return ids.has(prompt.lesson); });
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

function selectedLessonIndex() {
	return Number(phraseLessonSelect.value || '0');
}

function currentPool() {
	return uniquePrompts(promptsForLesson(selectedLessonIndex()));
}

function promptKey(prompt) {
	return prompt ? prompt.stroke + '\n' + prompt.phrase : '';
}

function normalizePromptFilter(text) {
	return text.trim().toLowerCase();
}

function promptMatchesFilter(prompt, filter) {
	if (!filter) return true;
	return prompt.phrase.toLowerCase().includes(filter)
		|| prompt.stroke.toLowerCase().includes(filter)
		|| prompt.lesson.toLowerCase().includes(filter);
}

function syncFocusSelectionFromInputs() {
	phraseFocusList.querySelectorAll('input[type="checkbox"]').forEach(function(input) {
		phraseCheckedByKey.set(input.value, input.checked);
	});
}

function populateFocusOptions(pool) {
	const filter = normalizePromptFilter(phraseFilterInput.value || '');
	const showOutlines = phraseShowOutlines.checked;
	const visiblePool = pool.filter(function(prompt) { return promptMatchesFilter(prompt, filter); });
	phraseFocusList.textContent = '';
	visiblePool.forEach(function(prompt) {
		const key = promptKey(prompt);
		const input = document.createElement('input');
		input.type = 'checkbox';
		input.value = key;
		input.checked = !phraseCheckedByKey.has(key) || phraseCheckedByKey.get(key);

		const text = document.createElement('span');
		text.textContent = showOutlines ? prompt.phrase + ' - ' + prompt.stroke : prompt.phrase;

		const label = document.createElement('label');
		label.appendChild(input);
		label.appendChild(text);
		phraseFocusList.appendChild(label);
	});
	if (!visiblePool.length) {
		const empty = document.createElement('div');
		empty.className = 'small';
		empty.textContent = 'No phrases match.';
		phraseFocusList.appendChild(empty);
	}
}

function selectedPrompts(pool) {
	return pool.filter(function(prompt) {
		const key = promptKey(prompt);
		return !phraseCheckedByKey.has(key) || phraseCheckedByKey.get(key);
	});
}

function rebuildPhraseQueue() {
	syncFocusSelectionFromInputs();
	const fullPool = currentPool();
	const repetitions = Math.max(1, Math.min(100, Number(phraseCountInput.value || '7')));
	populateFocusOptions(fullPool);
	const pool = selectedPrompts(fullPool);
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
		phraseSetSummary.textContent = pool.length + ' selected'
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
phraseOrderSelect.addEventListener('change', rebuildPhraseQueue);
phraseFocusList.addEventListener('change', rebuildPhraseQueue);
phraseFilterInput.addEventListener('input', function() {
	syncFocusSelectionFromInputs();
	populateFocusOptions(currentPool());
});
phraseShowOutlines.addEventListener('change', function() {
	syncFocusSelectionFromInputs();
	populateFocusOptions(currentPool());
});
phraseSelectAll.addEventListener('click', function() {
	phraseFocusList.querySelectorAll('input[type="checkbox"]').forEach(function(input) {
		input.checked = true;
	});
	rebuildPhraseQueue();
});
phraseSelectNone.addEventListener('click', function() {
	phraseFocusList.querySelectorAll('input[type="checkbox"]').forEach(function(input) {
		input.checked = false;
	});
	rebuildPhraseQueue();
});
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
