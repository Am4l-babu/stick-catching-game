#pragma once

#include <Arduino.h>

// Web control panel served by the ESP8266 at http://192.168.4.1
// Kept in its own header so the .ino stays readable (and so PlatformIO's
// .ino prototype scanner does not mistake the embedded JavaScript
// "function ..." lines for C++ function declarations).
//
// PROGMEM keeps the page in flash. handleRoot() streams it from there
// with send_P(), so loading the page does not copy it into RAM.

// =====================================================
// HTML PAGE
// =====================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stick Catcher</title>
<style>

body { font-family: Arial, sans-serif; background: #101010; color: white; margin: 0; padding: 15px; }

h1 { text-align: center; margin-bottom: 20px; }

h2 { margin: 0 0 6px; }

.card { background: #202020; padding: 18px; margin: 12px 0; border-radius: 15px; }

.status { text-align: center; font-size: 20px; padding: 15px; background: #151515; border-radius: 10px; }

.sub { text-align: center; color: #bbb; font-size: 15px; margin-top: 8px; min-height: 18px; }

button { width: 100%; padding: 15px; margin-top: 10px; border: none; border-radius: 10px; font-size: 18px; font-weight: bold; color: white; }

button:disabled { opacity: .45; }

.start { background: #16a34a; }
.stop { background: #dc2626; }
.test { background: #2563eb; }
.save { background: #9333ea; }
.plain { background: #3a3a3a; font-size: 15px; }

.test.sel { outline: 3px solid #00ff99; }

.grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; }

/* More than 6 sticks: smaller buttons, more per row */
.grid.many { grid-template-columns: repeat(4, 1fr); }
.grid.many button { padding: 12px 0; }
.grid.lots { grid-template-columns: repeat(6, 1fr); }
.grid.lots button { padding: 10px 0; font-size: 15px; }

/* One dot per stick: hanging, dropped, caught, missed */
.lanes { display: flex; flex-wrap: wrap; justify-content: center; gap: 5px; margin-top: 12px; }
.lane { width: 26px; height: 26px; border-radius: 50%; display: grid; place-items: center;
        font-size: 11px; font-weight: bold; border: 2px solid #555; color: #aaa; }
.lane.d { border-color: #fbbf24; background: #fbbf24; color: #000; }
.lane.c { border-color: #16a34a; background: #16a34a; color: #fff; }
.lane.m { border-color: #dc2626; background: #dc2626; color: #fff; }

label { display: block; margin-top: 15px; }

input[type=range] { width: 100%; }

select { width: 100%; padding: 12px; margin-top: 10px; font-size: 17px; border-radius: 10px; background: #151515; color: white; border: 1px solid #444; }

.value { color: #00ff99; font-weight: bold; }

.hint { color: #999; font-size: 14px; margin: 10px 0 0; }

.row { display: flex; justify-content: space-between; align-items: center; margin-top: 12px; }

.info { color: #999; font-size: 14px; line-height: 1.6; margin-top: 12px; }

a { color: #7dd3fc; }

#toast { position: fixed; left: 50%; bottom: 20px; transform: translateX(-50%); background: #16a34a; color: white;
         padding: 10px 18px; border-radius: 99px; font-weight: bold; opacity: 0; transition: opacity .3s; pointer-events: none; }
#toast.show { opacity: 1; }
#toast.bad { background: #dc2626; }

</style>
</head>
<body>

<h1>🎯 STICK CATCHER</h1>


<!-- ============================================ -->

<div class="card">

<div class="status">Status: <span id="status">WAITING</span></div>

<div class="sub" id="roundInfo"></div>

<div class="lanes" id="lanes"></div>

<div class="sub" id="scoreInfo"></div>

<button class="start" onclick="get('/start')">▶ START GAME</button>

<button class="stop" onclick="get('/stop')">■ STOP / RESET</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>🎯 Sticks</h2>

<label>Number of sticks: <span id="sticksValue" class="value">6</span></label>
<input type="range" id="stickCount" min="1" max="16" value="6" oninput="sticksValue.textContent = this.value">

<p id="sticksHint" class="hint">Servos on PCA9685 channels 0 to 5.</p>

<button class="save" onclick="saveSticks()">💾 SAVE STICK COUNT</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>🎮 Game Mode</h2>

<select id="mode" onchange="showMode()">
<option value="0">Classic</option>
<option value="1">Speed-up</option>
<option value="2">Double drop</option>
<option value="3">Marathon</option>
</select>

<p id="modeHint" class="hint"></p>

<div id="marathonBox">

<label>Rounds: <span id="roundsValue" class="value">5</span></label>
<input type="range" id="rounds" min="2" max="20" value="5" oninput="roundsValue.textContent = this.value">

<label>Reload pause: <span id="pauseValue" class="value">15 s</span></label>
<input type="range" id="pause" min="3" max="60" value="15" oninput="pauseValue.textContent = this.value + ' s'">

</div>

<button class="save" onclick="saveMode()">💾 SAVE MODE</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>⏱ Timing</h2>

<label>Start delay: <span id="startValue" class="value">5000 ms</span></label>
<input type="range" id="startDelay" min="1000" max="15000" step="500" value="5000" oninput="startValue.textContent = this.value + ' ms'">

<label>Minimum random delay: <span id="minValue" class="value">500 ms</span></label>
<input type="range" id="minDelay" min="100" max="5000" step="100" value="500" oninput="minValue.textContent = this.value + ' ms'">

<label>Maximum random delay: <span id="maxValue" class="value">2000 ms</span></label>
<input type="range" id="maxDelay" min="200" max="10000" step="100" value="2000" oninput="maxValue.textContent = this.value + ' ms'">

<label>Servo release time: <span id="releaseValue" class="value">200 ms</span></label>
<input type="range" id="releaseTime" min="50" max="1000" step="10" value="200" oninput="releaseValue.textContent = this.value + ' ms'">

<button class="save" onclick="saveTiming()">💾 SAVE TIMING</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>⚙ Servo Angles</h2>

<label>HOLD angle: <span id="holdValue" class="value">0°</span></label>
<input type="range" id="holdAngle" min="0" max="180" value="0" oninput="holdValue.textContent = this.value + '°'">

<label>RELEASE angle: <span id="releaseAngleValue" class="value">90°</span></label>
<input type="range" id="releaseAngle" min="0" max="180" value="90" oninput="releaseAngleValue.textContent = this.value + '°'">

<button class="save" onclick="saveServo()">💾 SAVE SERVO SETTINGS</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>🧪 Test &amp; Fine-tune</h2>

<p class="hint">Tap a stick to fire its hook and select it for fine-tuning.</p>

<!-- Filled in by buildTestButtons() -->
<div class="grid" id="tests"></div>

<label>Trim for <b id="trimStick">STICK 1</b>: <span id="trimValue" class="value">0°</span></label>
<input type="range" id="trim" min="-45" max="45" value="0" oninput="trimValue.textContent = signed(this.value) + '°'">

<p class="hint">Added to this hook's HOLD and RELEASE angles, for a horn that sits a little off. The hook moves to its new HOLD position when you save.</p>

<button class="save" onclick="saveTrim()">💾 SAVE TRIM</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>🔧 Board</h2>

<div class="row">
<span>🔊 Buzzer</span>
<input type="checkbox" id="sound" onchange="get('/sound?on=' + (this.checked ? 1 : 0))" style="width:24px;height:24px">
</div>

<div class="info" id="boardInfo"></div>

<p class="hint">Settings are saved on the board and kept after a restart.</p>

<button class="plain" onclick="restoreDefaults()">↺ RESTORE DEFAULTS</button>

<p class="info">Firmware update: <a href="/update">/update</a> (user <b>admin</b>)</p>

</div>


<div id="toast"></div>


<script>

let N = 6;
let selected = 0;
let trims = [];

const MODE_HINTS = [
  'Every stick drops once, one at a time.',
  'Each round the gaps get 15% shorter. STOP puts the speed back to normal.',
  'Two sticks drop at the same moment.',
  'Several rounds in a row, with a pause to reload the sticks between them.'
];


function $(id) {
  return document.getElementById(id);
}


function signed(v) {
  v = +v;
  return v > 0 ? '+' + v : String(v);
}


// ================================================
// MESSAGES
// ================================================

let toastTimer = 0;

function say(text, bad) {
  let t = $('toast');
  t.textContent = text;
  t.className = bad ? 'show bad' : 'show';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.className = '', 1800);
}


// GET a URL, show the board's answer, reject on errors
function get(url) {
  return fetch(url)
    .then(r => r.text().then(text => {
      if (!r.ok) {
        say(text === 'GAME RUNNING' ? 'Stop the game first' : text, true);
        throw new Error(text);
      }
      if (url.indexOf('/start') < 0 && url.indexOf('/stop') < 0) {
        say(text);
      }
      return text;
    }));
}


// ================================================
// STICKS
// ================================================

// One test button per stick, and the channel hint
function buildTestButtons(count) {

  let grid = $('tests');
  grid.innerHTML = '';
  grid.className = count > 16 ? 'grid lots' : count > 6 ? 'grid many' : 'grid';

  for (let i = 0; i < count; i++) {
    let b = document.createElement('button');
    b.className = 'test' + (i === selected ? ' sel' : '');
    b.textContent = count > 6 ? (i + 1) : 'STICK ' + (i + 1);
    b.onclick = () => testServo(i);
    grid.appendChild(b);
  }

  let hint = 'Servo 1 on board 1, channel 0.';

  if (count > 1) {
    hint = 'Servos 1 to ' + Math.min(count, 16) + ' on board 1 (0x40), channels 0 to ' + (Math.min(count, 16) - 1) + '.';
  }

  if (count > 16) {
    hint += ' Servos 17 to ' + count + ' on board 2 (0x41), channels 0 to ' + (count - 17) + '.';
  }

  $('sticksHint').textContent = hint;
}


function setSticks(count) {
  N = count;
  if (selected >= N) {
    selected = 0;
  }
  $('stickCount').value = N;
  $('sticksValue').textContent = N;
  buildTestButtons(N);
  selectStick(selected);
}


function saveSticks() {
  get('/sticks?count=' + $('stickCount').value)
    .then(text => setSticks(+text))
    .catch(() => setSticks(N));
}


// ================================================
// GAME MODE
// ================================================

function showMode() {
  let m = +$('mode').value;
  $('modeHint').textContent = MODE_HINTS[m];
  $('marathonBox').style.display = m === 3 ? 'block' : 'none';
}


function saveMode() {
  get('/mode?mode=' + $('mode').value +
      '&rounds=' + $('rounds').value +
      '&pause=' + ($('pause').value * 1000))
    .catch(loadConfig);
}


// ================================================
// SAVE TIMING / SERVO SETTINGS
// ================================================

function saveTiming() {
  get('/settings?start=' + $('startDelay').value +
      '&min=' + $('minDelay').value +
      '&max=' + $('maxDelay').value +
      '&release=' + $('releaseTime').value);
}


function saveServo() {
  get('/servo?hold=' + $('holdAngle').value +
      '&release=' + $('releaseAngle').value);
}


// ================================================
// TEST AND TRIM
// ================================================

function selectStick(i) {
  selected = i;
  let buttons = $('tests').children;
  for (let k = 0; k < buttons.length; k++) {
    buttons[k].classList.toggle('sel', k === i);
  }
  $('trimStick').textContent = 'STICK ' + (i + 1);
  $('trim').value = trims[i] || 0;
  $('trimValue').textContent = signed(trims[i] || 0) + '°';
}


function testServo(i) {
  selectStick(i);
  fetch('/test?servo=' + i)
    .then(r => r.text())
    .then(text => { if (text === 'GAME RUNNING') say('Stop the game first', true); });
}


function saveTrim() {
  let v = +$('trim').value;
  get('/trim?servo=' + selected + '&value=' + v)
    .then(() => trims[selected] = v);
}


// ================================================
// BOARD
// ================================================

function restoreDefaults() {
  if (confirm('Put every setting back to its default?')) {
    get('/defaults').then(loadConfig);
  }
}


// ================================================
// LOAD CURRENT SETTINGS FROM THE BOARD
// ================================================

function setSlider(id, valueId, value, text) {
  $(id).value = value;
  $(valueId).textContent = text;
}


function loadConfig() {
  return fetch('/config')
    .then(r => r.json())
    .then(c => {

      trims = c.trim;

      $('stickCount').max = c.maxSticks;
      setSticks(c.sticks);

      $('mode').value = c.mode;
      setSlider('rounds', 'roundsValue', c.rounds, c.rounds);
      setSlider('pause', 'pauseValue', Math.round(c.pause / 1000), Math.round(c.pause / 1000) + ' s');
      showMode();

      setSlider('startDelay', 'startValue', c.start, c.start + ' ms');
      setSlider('minDelay', 'minValue', c.min, c.min + ' ms');
      setSlider('maxDelay', 'maxValue', c.max, c.max + ' ms');
      setSlider('releaseTime', 'releaseValue', c.release, c.release + ' ms');
      setSlider('holdAngle', 'holdValue', c.hold, c.hold + '°');
      setSlider('releaseAngle', 'releaseAngleValue', c.releaseAngle, c.releaseAngle + '°');

      $('sound').checked = !!c.sound;

      $('boardInfo').innerHTML =
        'Firmware ' + c.version + '<br>' +
        'Servo boards: ' + c.boards + ' (up to ' + c.maxSticks + ' sticks)<br>' +
        'Floor sensors: ' + (c.sensors ? c.sensors + ' lanes, scoring on' : 'none (no scoring)');
    });
}


// ================================================
// LIVE STATE
// ================================================

const MODE_NAMES = ['Classic', 'Speed-up', 'Double drop', 'Marathon'];

function showState(s) {

  $('status').textContent = s.text;

  // Another phone changed the stick count
  if (s.sticks !== N) {
    loadConfig();
  }

  let lanes = $('lanes');
  if (lanes.children.length !== s.lanes.length) {
    lanes.innerHTML = '';
    for (let i = 0; i < s.lanes.length; i++) {
      let d = document.createElement('div');
      d.textContent = i + 1;
      lanes.appendChild(d);
    }
  }
  for (let i = 0; i < s.lanes.length; i++) {
    lanes.children[i].className = 'lane ' + s.lanes[i];
  }

  let info = MODE_NAMES[s.mode];
  if (s.round > 0) {
    info += ' · round ' + s.round + ' of ' + s.rounds;
  }
  if (s.mode === 1) {
    info += ' · gaps ×' + s.speed.toFixed(2);
  }
  $('roundInfo').textContent = info;

  let score = '';
  if (s.sensors && (s.caught || s.missed)) {
    score = 'Caught ' + s.caught + ' · missed ' + s.missed;
    if (s.round > 0) {
      score += ' · total ' + s.totalCaught + ' caught';
    }
    if (s.best >= 0) {
      score += ' · best ' + s.best;
    }
  }
  $('scoreInfo').textContent = score;
}


function poll() {
  fetch('/state')
    .then(r => r.json())
    .then(showState)
    .catch(() => $('status').textContent = 'NOT CONNECTED')
    .finally(() => setTimeout(poll, 500));
}


showMode();
buildTestButtons(N);
loadConfig().finally(poll);

</script>

</body>
</html>
)rawliteral";
