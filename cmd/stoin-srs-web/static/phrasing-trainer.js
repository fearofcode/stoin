const phraseAssignments = [
	{ lesson: 'iv', stroke: 'PW-B', phrase: 'is a' },
	{ lesson: 'iv', stroke: 'PW-BD', phrase: 'was a' },
	{ lesson: 'iv', stroke: 'PW-T', phrase: 'is the' },
	{ lesson: 'iv', stroke: 'PW-TD', phrase: 'was the' },
	{ lesson: 'iv', stroke: 'PW-P', phrase: 'is it' },
	{ lesson: 'iv', stroke: 'PW-PD', phrase: 'was it' },
	{ lesson: 'iv', stroke: 'PW-RT', phrase: 'is that' },
	{ lesson: 'iv', stroke: 'PW-RTD', phrase: 'was that' },

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

	{ lesson: 'nv-immediate', stroke: 'WHR*-T', phrase: 'with the' },
	{ lesson: 'nv-immediate', stroke: 'WHR*-PLT', phrase: 'with them' },
	{ lesson: 'nv-immediate', stroke: 'WHR*-RT', phrase: 'with that' },
	{ lesson: 'nv-immediate', stroke: 'PHR*-RT', phrase: 'anything that' },
	{ lesson: 'nv-immediate', stroke: 'KPHR*-RT', phrase: 'even that' },

	{ lesson: 'nv-else', stroke: 'PHR*-F', phrase: 'anything else' },
	{ lesson: 'nv-else', stroke: 'PHR*-R', phrase: 'something else' },
	{ lesson: 'nv-else', stroke: 'PHR*-P', phrase: 'everybody else' },
	{ lesson: 'nv-else', stroke: 'PHR*-L', phrase: 'everything else' },
	{ lesson: 'nv-else', stroke: 'TPHRA*-F', phrase: 'each of the' },
	{ lesson: 'nv-else', stroke: 'TPHRA*-R', phrase: 'both of the' },
	{ lesson: 'nv-else', stroke: 'TPHRA*-P', phrase: 'one of them' },
	{ lesson: 'nv-else', stroke: 'TPHRA*-B', phrase: 'some of them' },
	{ lesson: 'nv-else', stroke: 'TPHRA*-L', phrase: 'any of them' },
	{ lesson: 'nv-else', stroke: 'TPHRA*-G', phrase: 'all of them' },

	{ lesson: 'nv-functions', stroke: 'KPHR*-F', phrase: 'as if' },
	{ lesson: 'nv-functions', stroke: 'KPHR*-R', phrase: 'as though' },
	{ lesson: 'nv-functions', stroke: 'KPHR*-P', phrase: 'even if' },
	{ lesson: 'nv-functions', stroke: 'KPHR*-L', phrase: 'even though' },
	{ lesson: 'nv-functions', stroke: 'STPHR*-R', phrase: 'in order to' },
	{ lesson: 'nv-functions', stroke: 'STPHR*-B', phrase: 'instead of' },
	{ lesson: 'nv-functions', stroke: 'STPHR*-L', phrase: 'not only' },
	{ lesson: 'nv-functions', stroke: 'STPHR*-G', phrase: 'not yet' },
];

const phraseLessons = [
	{ name: '1. IV Set 1', detail: 'is/was + a/the/it/that', lessonIDs: ['iv'] },
	{ name: '2. FV core', detail: 'common long final-verb phrases', lessonIDs: ['fv-core'] },
	{ name: '3. FV operators', detail: 'not, will, progressive, perfect, suffixes', lessonIDs: ['fv-operators'] },
	{ name: '4. FV contractions', detail: '# contraction forms only', lessonIDs: ['fv-contractions'] },
	{ name: '5. NV immediate', detail: 'with * and * that', lessonIDs: ['nv-immediate'] },
	{ name: '6. NV else/partitives', detail: 'else and of them/of the chunks', lessonIDs: ['nv-else'] },
	{ name: '7. NV functions', detail: 'subordinators and function chunks', lessonIDs: ['nv-functions'] },
	{ name: '8. All implemented', detail: 'IV, FV, and NV Set 1', lessonIDs: 'all' },
];

const phraseLessonSelect = document.getElementById('phrase-lesson');
const phraseCountInput = document.getElementById('phrase-count');
const phraseOrderSelect = document.getElementById('phrase-order');
const phraseFocusList = document.getElementById('phrase-focus-list');
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

function populateFocusOptions(pool) {
	const previousInputs = Array.from(phraseFocusList.querySelectorAll('input[type="checkbox"]'));
	const previousChecked = new Set(previousInputs.filter(function(input) { return input.checked; }).map(function(input) { return input.value; }));
	const hasPrevious = previousInputs.length > 0;
	const hasMatchingPrevious = pool.some(function(prompt) { return previousChecked.has(promptKey(prompt)); });
	phraseFocusList.textContent = '';
	pool.forEach(function(prompt) {
		const key = promptKey(prompt);
		const input = document.createElement('input');
		input.type = 'checkbox';
		input.value = key;
		input.checked = !hasPrevious || (previousChecked.size > 0 && !hasMatchingPrevious) || previousChecked.has(key);

		const text = document.createElement('span');
		text.textContent = prompt.phrase + ' - ' + prompt.stroke;

		const label = document.createElement('label');
		label.appendChild(input);
		label.appendChild(text);
		phraseFocusList.appendChild(label);
	});
}

function selectedPrompts(pool) {
	const selected = new Set(Array.from(phraseFocusList.querySelectorAll('input[type="checkbox"]'))
		.filter(function(input) { return input.checked; })
		.map(function(input) { return input.value; }));
	return pool.filter(function(prompt) { return selected.has(promptKey(prompt)); });
}

function rebuildPhraseQueue() {
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
