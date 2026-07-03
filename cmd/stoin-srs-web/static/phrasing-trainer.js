let phraseAssignments = [];
let phraseLessons = [];

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
const phraseStorageKey = 'stoin.phrasingTrainer.v1';

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

function repetitionCount() {
	const count = Number(phraseCountInput.value || '7');
	if (!Number.isFinite(count)) return 7;
	return Math.max(1, Math.min(100, Math.round(count)));
}

function currentPool() {
	return uniquePrompts(promptsForLesson(selectedLessonIndex()));
}

function promptKey(prompt) {
	return prompt ? prompt.stroke + '\n' + prompt.phrase : '';
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
		lessonIndex: selectedLessonIndex(),
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

	if (Number.isInteger(settings.lessonIndex) && settings.lessonIndex >= 0 && settings.lessonIndex < phraseLessonSelect.options.length) {
		phraseLessonSelect.value = String(settings.lessonIndex);
	}
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
	const repetitions = repetitionCount();
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
	phraseLessonSelect.textContent = '';
	phraseLessons.forEach(function(lesson, index) {
		const option = document.createElement('option');
		option.value = String(index);
		option.textContent = lesson.name + ' - ' + lesson.detail;
		phraseLessonSelect.appendChild(option);
	});
}

function validatePhraseData(data) {
	if (!data || !data.trainer || !Array.isArray(data.trainer.assignments) || !Array.isArray(data.trainer.lessons)) {
		throw new Error('phrasing JSON needs trainer.lessons and trainer.assignments arrays');
	}
	data.trainer.assignments.forEach(function(prompt, index) {
		if (!prompt || typeof prompt.lesson !== 'string' || typeof prompt.stroke !== 'string' || typeof prompt.phrase !== 'string') {
			throw new Error('trainer.assignments[' + index + '] needs lesson, stroke, and phrase strings');
		}
	});
	data.trainer.lessons.forEach(function(lesson, index) {
		if (!lesson || typeof lesson.name !== 'string' || typeof lesson.detail !== 'string'
			|| (!Array.isArray(lesson.lessonIDs) && lesson.lessonIDs !== 'all')) {
			throw new Error('trainer.lessons[' + index + '] needs name, detail, and lessonIDs');
		}
	});
}

function showPhraseLoadError(message) {
	phraseProgress.textContent = '0 / 0';
	phraseLessonName.textContent = '';
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
		const response = await fetch('/phrasing-data.json', { cache: 'no-store' });
		if (!response.ok) {
			throw new Error('phrasing-data.json returned HTTP ' + response.status);
		}
		const data = await response.json();
		validatePhraseData(data);
		phraseAssignments = data.trainer.assignments.slice();
		phraseLessons = data.trainer.lessons.slice();
		populatePhraseControls();
		restorePhraseSettings();
		rebuildPhraseQueue();
	} catch (error) {
		showPhraseLoadError(error.message || String(error));
	}
}

phraseLessonSelect.addEventListener('change', function() {
	rebuildPhraseQueue();
	savePhraseSettings();
});
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
	populateFocusOptions(currentPool());
});
phraseShowOutlines.addEventListener('change', function() {
	syncFocusSelectionFromInputs();
	populateFocusOptions(currentPool());
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
