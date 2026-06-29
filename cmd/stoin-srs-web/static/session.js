const sessionConfig = window.stoinSession || {};
const items = sessionConfig.items || [];
const isReview = Boolean(sessionConfig.isReview);
const correctDebounceMs = 50;
const autoSubmitSeconds = 3;
let index = 0;
let currentInput = '';
let correctTimer = 0;
let autoSubmitTimer = 0;
let autoSubmitRemaining = 0;
const results = Array(items.length).fill('');
const form = document.getElementById('session-form');
const submit = document.getElementById('submit');
const autoSubmitStatus = document.getElementById('auto-submit-status');
const lines = Array.from(document.querySelectorAll('.session-line'));
const lineInputs = Array.from(document.querySelectorAll('.session-line-input'));
const skipButtons = Array.from(document.querySelectorAll('.session-skip'));
const tokens = Array.from(document.querySelectorAll('.session-token'));
const lineByItem = [];
lines.forEach((line, lineIndex) => {
	const start = Number(line.dataset.start);
	const end = Number(line.dataset.end);
	for (let i = start; i <= end; i++) lineByItem[i] = lineIndex;
	line.addEventListener('click', function() {
		focusCurrentInput(false);
	});
});
function norm(s) { return s.trim(); }
function currentLineIndex() {
	return index < items.length ? lineByItem[index] : -1;
}
function inputPrefixIsOk() {
	if (index >= items.length) return true;
	const typed = norm(currentInput);
	return typed === '' || items[index].text.startsWith(typed);
}
function currentInputIsCorrect() {
	return index < items.length && norm(currentInput) === items[index].text;
}
function cancelCorrectTimer() {
	if (!correctTimer) return;
	window.clearTimeout(correctTimer);
	correctTimer = 0;
}
function anyMissed() {
	return results.some((result) => result === 'missed');
}
function cancelAutoSubmit() {
	if (autoSubmitTimer) {
		window.clearInterval(autoSubmitTimer);
		autoSubmitTimer = 0;
	}
	autoSubmitStatus.textContent = '';
}
function submitReviewForm() {
	autoSubmitStatus.textContent = 'Submitting...';
	if (form.requestSubmit) {
		form.requestSubmit(submit);
	} else {
		form.submit();
	}
}
function updateAutoSubmitStatus() {
	autoSubmitStatus.textContent = 'Submitting in ' + autoSubmitRemaining + '...';
}
function scheduleAutoSubmit() {
	if (!isReview || index < items.length || anyMissed() || autoSubmitTimer) return;
	autoSubmitRemaining = autoSubmitSeconds;
	updateAutoSubmitStatus();
	autoSubmitTimer = window.setInterval(function() {
		autoSubmitRemaining--;
		if (autoSubmitRemaining <= 0) {
			window.clearInterval(autoSubmitTimer);
			autoSubmitTimer = 0;
			submitReviewForm();
			return;
		}
		updateAutoSubmitStatus();
	}, 1000);
}
function scheduleCorrectCompletion() {
	cancelCorrectTimer();
	const scheduledIndex = index;
	const scheduledInput = currentInput;
	correctTimer = window.setTimeout(function() {
		correctTimer = 0;
		if (index === scheduledIndex && currentInput === scheduledInput && currentInputIsCorrect()) {
			completeCurrent(true);
		}
	}, correctDebounceMs);
}
function makeSpan(className, text) {
	const span = document.createElement('span');
	span.className = className;
	if (text !== undefined) span.textContent = text;
	return span;
}
function renderTypingEcho() {
	const visibleInput = currentInput.trimStart();
	const echo = makeSpan(currentInput && !inputPrefixIsOk() ? 'typing-echo input-error' : 'typing-echo', visibleInput);
	echo.appendChild(makeSpan('typing-cursor'));
	return echo;
}
function renderLineEcho(line, lineIndex) {
	const echo = line.querySelector('.session-echo');
	const start = Number(line.dataset.start);
	const end = Number(line.dataset.end);
	const currentLine = currentLineIndex();
	let wrote = false;
	echo.replaceChildren();
	for (let i = start; i <= end && i < index; i++) {
		if (results[i] === 'missed') {
			const marker = makeSpan('typed-missed');
			marker.setAttribute('aria-label', 'skipped');
			echo.appendChild(marker);
		} else {
			echo.appendChild(makeSpan('typed-word', items[i].text));
		}
		wrote = true;
	}
	if (lineIndex === currentLine && index < items.length) {
		echo.appendChild(renderTypingEcho());
		wrote = true;
	}
	if (!wrote) echo.appendChild(makeSpan('input-spacer'));
}
function updateTokenStates() {
	tokens.forEach((token) => {
		const itemIndex = Number(token.dataset.index);
		token.className = 'session-token';
		if (itemIndex < index) {
			token.classList.add(results[itemIndex] === 'missed' ? 'missed' : 'past');
		} else if (itemIndex === index) {
			token.classList.add('current');
			if (currentInput && !inputPrefixIsOk()) token.classList.add('wrong');
		} else {
			token.classList.add('future');
		}
	});
}
function syncControls() {
	const currentLine = currentLineIndex();
	lines.forEach((line, lineIndex) => {
		const current = lineIndex === currentLine;
		line.classList.toggle('current', current);
		lineInputs[lineIndex].disabled = !current;
		skipButtons[lineIndex].disabled = !current;
		if (current) {
			if (lineInputs[lineIndex].value !== currentInput) lineInputs[lineIndex].value = currentInput;
		} else {
			lineInputs[lineIndex].value = '';
		}
	});
	if (index >= items.length) {
		submit.disabled = false;
		submit.focus();
		if (isReview && anyMissed()) {
			cancelAutoSubmit();
			autoSubmitStatus.textContent = 'Skipped item present; press Enter to submit.';
		} else {
			scheduleAutoSubmit();
		}
	} else {
		cancelAutoSubmit();
	}
}
function focusCurrentInput(scroll) {
	if (index >= items.length) return;
	const currentLine = currentLineIndex();
	const input = lineInputs[currentLine];
	if (!input || input.disabled) return;
	if (scroll) lines[currentLine].scrollIntoView({block: 'center'});
	input.focus({preventScroll: true});
	input.setSelectionRange(input.value.length, input.value.length);
}
function renderSession(options) {
	const opts = options || {};
	updateTokenStates();
	lines.forEach(renderLineEcho);
	syncControls();
	if (opts.focus !== false) focusCurrentInput(opts.scroll !== false);
}
function completeCurrent(correct) {
	if (index >= items.length) return;
	cancelCorrectTimer();
	const answer = currentInput;
	document.getElementById('answer_' + index).value = answer;
	document.getElementById('result_' + index).value = correct ? 'correct' : 'missed';
	results[index] = correct ? 'correct' : 'missed';
	const oldLine = currentLineIndex();
	if (oldLine >= 0) lineInputs[oldLine].value = '';
	currentInput = '';
	index++;
	renderSession();
}
lineInputs.forEach((input, lineIndex) => {
	input.addEventListener('input', function() {
		if (lineIndex !== currentLineIndex()) return;
		cancelCorrectTimer();
		currentInput = input.value;
		if (currentInputIsCorrect()) {
			scheduleCorrectCompletion();
			renderSession({focus: false, scroll: false});
		} else {
			renderSession({focus: false, scroll: false});
		}
	});
});
skipButtons.forEach((button, lineIndex) => {
	button.addEventListener('click', function() {
		if (lineIndex === currentLineIndex()) completeCurrent(false);
	});
});
renderSession();
