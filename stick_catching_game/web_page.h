#pragma once

#include <Arduino.h>

// Web control panel served by the ESP8266 at http://192.168.4.1
// Kept in its own header so the .ino stays readable (and so PlatformIO's
// .ino prototype scanner does not mistake the embedded JavaScript
// "function ..." lines for C++ function declarations).

// =====================================================
// HTML PAGE
// =====================================================

String getHTML() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="utf-8">

<meta name="viewport"
content="width=device-width,initial-scale=1">

<title>Stick Catcher</title>

<style>

body {

  font-family: Arial, sans-serif;

  background: #101010;

  color: white;

  margin: 0;

  padding: 15px;

}


h1 {

  text-align: center;

  margin-bottom: 20px;

}


.card {

  background: #202020;

  padding: 18px;

  margin: 12px 0;

  border-radius: 15px;

}


.status {

  text-align: center;

  font-size: 20px;

  padding: 15px;

  background: #151515;

  border-radius: 10px;

}


button {

  width: 100%;

  padding: 15px;

  margin-top: 10px;

  border: none;

  border-radius: 10px;

  font-size: 18px;

  font-weight: bold;

}


.start {

  background: #16a34a;

  color: white;

}


.stop {

  background: #dc2626;

  color: white;

}


.test {

  background: #2563eb;

  color: white;

}


.save {

  background: #9333ea;

  color: white;

}


.grid {

  display: grid;

  grid-template-columns: repeat(2, 1fr);

  gap: 8px;

}


/* More than 6 sticks: four smaller buttons per row */

.grid.many {

  grid-template-columns: repeat(4, 1fr);

}


.grid.many button {

  padding: 12px 0;

}


.hint {

  color: #999;

  font-size: 14px;

  margin: 10px 0 0;

}


label {

  display: block;

  margin-top: 15px;

}


input[type=range] {

  width: 100%;

}


.value {

  color: #00ff99;

  font-weight: bold;

}


</style>

</head>


<body>


<h1>🎯 STICK CATCHER</h1>


<!-- ============================================ -->

<div class="card">

<div class="status">

Status:

<span id="status">

WAITING

</span>

</div>


<button
class="start"
onclick="startGame()">

▶ START GAME

</button>


<button
class="stop"
onclick="stopGame()">

■ STOP / RESET

</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>🎯 Sticks</h2>


<label>

Number of sticks:

<span
id="sticksValue"
class="value">

6

</span>

</label>


<input
type="range"
id="stickCount"
min="1"
max="16"
value="6"
oninput="
sticksValue.innerHTML =
this.value
">


<p
id="sticksHint"
class="hint">

Servos on PCA9685 channels 0 to 5.

</p>


<button
class="save"
onclick="saveSticks()">

💾 SAVE STICK COUNT

</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>⏱ Timing</h2>


<label>

Start delay:

<span
id="startValue"
class="value">

5000 ms

</span>

</label>


<input
type="range"
id="startDelay"
min="1000"
max="15000"
step="500"
value="5000"
oninput="
startValue.innerHTML =
this.value + ' ms'
">


<label>

Minimum random delay:

<span
id="minValue"
class="value">

500 ms

</span>

</label>


<input
type="range"
id="minDelay"
min="100"
max="5000"
step="100"
value="500"
oninput="
minValue.innerHTML =
this.value + ' ms'
">


<label>

Maximum random delay:

<span
id="maxValue"
class="value">

2000 ms

</span>

</label>


<input
type="range"
id="maxDelay"
min="200"
max="10000"
step="100"
value="2000"
oninput="
maxValue.innerHTML =
this.value + ' ms'
">


<label>

Servo release time:

<span
id="releaseValue"
class="value">

200 ms

</span>

</label>


<input
type="range"
id="releaseTime"
min="50"
max="1000"
step="10"
value="200"
oninput="
releaseValue.innerHTML =
this.value + ' ms'
">


<button
class="save"
onclick="saveTiming()">

💾 SAVE TIMING

</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>⚙ Servo Angles</h2>


<label>

HOLD angle:

<span
id="holdValue"
class="value">

0°

</span>

</label>


<input
type="range"
id="holdAngle"
min="0"
max="180"
value="0"
oninput="
holdValue.innerHTML =
this.value + '°'
">


<label>

RELEASE angle:

<span
id="releaseAngleValue"
class="value">

90°

</span>

</label>


<input
type="range"
id="releaseAngle"
min="0"
max="180"
value="90"
oninput="
releaseAngleValue.innerHTML =
this.value + '°'
">


<button
class="save"
onclick="saveServo()">

💾 SAVE SERVO SETTINGS

</button>

</div>


<!-- ============================================ -->

<div class="card">

<h2>🧪 Test Individual Servos</h2>


<!-- Filled in by buildTestButtons() -->

<div
class="grid"
id="tests">

</div>

</div>


<script>


// ================================================
// START
// ================================================

function startGame() {

  fetch('/start');

}


// ================================================
// STOP
// ================================================

function stopGame() {

  fetch('/stop');

}


// ================================================
// SAVE TIMING
// ================================================

function saveTiming() {

  let start =
    document.getElementById(
      'startDelay'
    ).value;


  let min =
    document.getElementById(
      'minDelay'
    ).value;


  let max =
    document.getElementById(
      'maxDelay'
    ).value;


  let release =
    document.getElementById(
      'releaseTime'
    ).value;


  fetch(
    '/settings?start=' +
    start +
    '&min=' +
    min +
    '&max=' +
    max +
    '&release=' +
    release
  );

}


// ================================================
// SAVE SERVO SETTINGS
// ================================================

function saveServo() {

  let hold =
    document.getElementById(
      'holdAngle'
    ).value;


  let release =
    document.getElementById(
      'releaseAngle'
    ).value;


  fetch(
    '/servo?hold=' +
    hold +
    '&release=' +
    release
  );

}


// ================================================
// TEST SERVO
// ================================================

function testServo(number) {

  fetch(
    '/test?servo=' +
    number
  );

}


// ================================================
// STICK COUNT
// ================================================

// One test button per stick, and the channel hint

function buildTestButtons(count) {

  let grid =
    document.getElementById(
      'tests'
    );


  grid.innerHTML = '';

  grid.className =
    count > 6 ? 'grid many' : 'grid';


  for (let i = 0; i < count; i++) {

    let b =
      document.createElement(
        'button'
      );


    b.className = 'test';

    b.innerHTML =
      count > 6 ? (i + 1) : 'STICK ' + (i + 1);

    b.onclick = function() {
      testServo(i);
    };


    grid.appendChild(b);
  }


  document.getElementById(
    'sticksHint'
  ).innerHTML =
    count == 1
      ? 'Servo on PCA9685 channel 0.'
      : 'Servos on PCA9685 channels 0 to ' + (count - 1) + '.';

}


function setStickSlider(count) {

  document.getElementById(
    'stickCount'
  ).value = count;


  document.getElementById(
    'sticksValue'
  ).innerHTML = count;

}


function saveSticks() {

  let count =
    document.getElementById(
      'stickCount'
    ).value;


  fetch(
    '/sticks?count=' +
    count
  )

  .then(
    response =>
      response.text().then(
        text => {

          if (!response.ok) {

            alert(
              'Stick count not changed: ' + text +
              '. Stop the game first.'
            );

            loadConfig();

            return;
          }


          setStickSlider(+text);

          buildTestButtons(+text);

        }
      )
  );

}


// ================================================
// LOAD CURRENT SETTINGS FROM THE BOARD
// ================================================

function setSlider(id, valueId, value, unit) {

  document.getElementById(
    id
  ).value = value;


  document.getElementById(
    valueId
  ).innerHTML = value + unit;

}


function loadConfig() {

  fetch('/config')

  .then(
    response => response.json()
  )

  .then(
    c => {

      document.getElementById(
        'stickCount'
      ).max = c.maxSticks;


      setStickSlider(c.sticks);

      buildTestButtons(c.sticks);


      setSlider('startDelay', 'startValue', c.start, ' ms');

      setSlider('minDelay', 'minValue', c.min, ' ms');

      setSlider('maxDelay', 'maxValue', c.max, ' ms');

      setSlider('releaseTime', 'releaseValue', c.release, ' ms');

      setSlider('holdAngle', 'holdValue', c.hold, '°');

      setSlider('releaseAngle', 'releaseAngleValue', c.releaseAngle, '°');

    }
  );

}


buildTestButtons(6);

loadConfig();


// ================================================
// STATUS
// ================================================

setInterval(function() {

  fetch('/status')

  .then(
    response => response.text()
  )

  .then(
    data => {

      document.getElementById(
        'status'
      ).innerHTML = data;

    }
  );

}, 500);


</script>


</body>

</html>

)rawliteral";


  return html;
}
