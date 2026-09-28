// =====================================================
// STICK CATCHER
//
// ESP8266 + PCA9685 servo driver(s) + Wi-Fi web panel.
// Sticks hang from servo hooks and drop one by one,
// in a random order, at random intervals.
// =====================================================

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>

#include "settings.h"
#include "buzzer.h"
#include "lane_sensors.h"
#include "web_page.h"

#define FIRMWARE_VERSION "3.0.0"


// =====================================================
// WIFI ACCESS POINT
// =====================================================

const char* AP_SSID = "STICK-CATCHER";
const char* AP_PASSWORD = "12345678";

// Password for wireless firmware updates, both from
// PlatformIO (OTA) and from http://192.168.4.1/update
// (user name "admin"). Change it before you take the
// game somewhere public.
const char* OTA_PASSWORD = "stickcatcher";

IPAddress apIP(192, 168, 4, 1);

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// Answers every DNS name with 192.168.4.1, so phones
// open the control panel by themselves (captive portal)
DNSServer dnsServer;


// =====================================================
// PCA9685 SERVO BOARDS
// =====================================================

// Board 1 (address 0x40): servos 1-16 on CH0-CH15
// Board 2 (address 0x41, solder jumper A0 closed):
//         servos 17-32 on CH0-CH15. Optional.

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);

bool board2Found = false;

// 16 with one board, 32 with two
int availableSticks = STICKS_PER_BOARD;


// =====================================================
// ESP8266 GPIO
// =====================================================

// Physical START button to GND
// GPIO14 = D5 on NodeMCU
#define START_BUTTON 14

// Passive buzzer (+ to this pin, - to GND)
// GPIO12 = D6
#define BUZZER_PIN 12

// Status LED through a 220 ohm resistor to GND
// GPIO13 = D7
#define LED_PIN 13

// I2C
// SDA = GPIO4 (D2)
// SCL = GPIO5 (D1)


// =====================================================
// PCA9685 SERVO CALIBRATION
// =====================================================

// These are starting values.
// Calibrate these for your actual SG90s.

#define SERVO_MIN 150
#define SERVO_MAX 600


// =====================================================
// GAME TUNING
// =====================================================

// Servos are reset one after another, this far apart,
// so they don't all start moving at once (current spike)
const unsigned long RESET_STEP_MS = 100;

// Floor sensors: a landing within this time after a
// drop is a miss. No landing means the stick was caught.
const unsigned long MISS_WINDOW_MS = 1500;

// Speed-up mode: each round's gaps are this fraction of
// the previous round's
const float SPEEDUP_FACTOR = 0.85;

// Shortest gap speed-up mode will go down to
const unsigned long SPEEDUP_MIN_GAP = 50;


// =====================================================
// GAME STATE
// =====================================================

enum GameState {

  WAITING,         // idle, waiting for START
  COUNTDOWN,       // start delay running
  RELEASE_STICK,   // drop the next stick now
  BETWEEN_STICKS,  // random gap before the next drop
  ROUND_END,       // last stick dropped, scoring
  RESETTING,       // hooks going back to HOLD
  RELOADING        // marathon: pause to reload sticks
};

GameState gameState = WAITING;


// =====================================================
// GAME VARIABLES
// =====================================================

// Everything the web panel can change (see settings.h)
Settings cfg;

Buzzer buzzer;

LaneSensors sensors;

// Randomized order (first cfg.sticks entries are used)
int stickOrder[MAX_STICKS];

// Number already released
int releasedCount = 0;

// Per stick, this round:
//   'h' hanging, 'd' dropped, 'c' caught, 'm' missed
// (caught / missed need floor sensors)
char laneState[MAX_STICKS];

unsigned long droppedAt[MAX_STICKS];

// Timing variables
unsigned long countdownStart = 0;

unsigned long lastActionTime = 0;

unsigned long currentDelay = 0;

unsigned long roundEndAt = 0;

unsigned long reloadUntil = 0;

int lastBeepSecond = -1;

// Speed-up mode: rounds played at the current pace
int speedLevel = 0;

// Marathon: current round (1..), 0 when not in a marathon
int marathonRound = 0;

// Scores (floor sensors only)
int roundCaught = 0;
int roundMissed = 0;
int totalCaught = 0;
int totalMissed = 0;
int bestCaught = -1;

// Non-blocking servo reset: next servo to move, -1 = idle
int resetNext = -1;

unsigned long resetDueAt = 0;

// Test fire: when each hook goes back to HOLD (0 = not testing)
unsigned long testReturnAt[MAX_STICKS];

// Floor sensor bits from the previous read
uint32_t lastSensorBits = 0;

unsigned long lastSensorRead = 0;


// =====================================================
// BUTTON
// =====================================================

bool buttonRaw = HIGH;

bool buttonStable = HIGH;

unsigned long buttonChangedAt = 0;


// =====================================================
// CONVERT ANGLE TO PCA9685 PULSE
// =====================================================

uint16_t angleToPulse(int angle) {

  angle = constrain(angle, 0, 180);

  return map(
    angle,
    0,
    180,
    SERVO_MIN,
    SERVO_MAX);
}


// =====================================================
// SET ONE SERVO
// =====================================================

// angle is the global HOLD/RELEASE angle; the hook's own
// trim is added here

void setServoAngle(int stick, int angle) {

  if (stick < 0 || stick >= availableSticks) {
    return;
  }

  int trimmed =
    constrain(angle + cfg.trim[stick], 0, 180);

  Adafruit_PWMServoDriver& board =
    stick < STICKS_PER_BOARD ? pwm1 : pwm2;

  board.setPWM(
    stick % STICKS_PER_BOARD,
    0,
    angleToPulse(trimmed));
}


// =====================================================
// RESET ALL SERVOS (NON-BLOCKING)
// =====================================================

// Starts moving the hooks back to HOLD, one every
// RESET_STEP_MS. updateReset() does the work from loop(),
// so the web panel keeps responding.

void beginReset() {

  Serial.println();
  Serial.println("Resetting all servos...");

  for (int i = 0; i < MAX_STICKS; i++) {
    testReturnAt[i] = 0;
  }

  resetNext = 0;

  resetDueAt = millis();
}


bool resetBusy() {

  return resetNext >= 0;
}


void updateReset(unsigned long now) {

  if (resetNext < 0 || (long)(now - resetDueAt) < 0) {
    return;
  }

  if (resetNext >= cfg.sticks) {

    resetNext = -1;

    Serial.println("All servos at HOLD position.");

    return;
  }

  setServoAngle(
    resetNext,
    cfg.holdAngle);

  resetNext++;

  resetDueAt = now + RESET_STEP_MS;
}


// Test-fired hooks go back to HOLD after releaseTime

void updateTests(unsigned long now) {

  for (int i = 0; i < MAX_STICKS; i++) {

    if (testReturnAt[i] && (long)(now - testReturnAt[i]) >= 0) {

      testReturnAt[i] = 0;

      setServoAngle(
        i,
        cfg.holdAngle);
    }
  }
}


// =====================================================
// GENERATE RANDOM ORDER
// =====================================================

void generateRandomOrder() {

  // Start with:
  //
  // 0 1 2 ... sticks-1

  for (int i = 0; i < cfg.sticks; i++) {

    stickOrder[i] = i;
  }


  // Fisher-Yates shuffle
  //
  // Ensures every stick is used exactly once.

  for (int i = cfg.sticks - 1; i > 0; i--) {

    int j = random(
      0,
      i + 1);

    int temp = stickOrder[i];

    stickOrder[i] = stickOrder[j];

    stickOrder[j] = temp;
  }


  // Serial monitor
  Serial.print("Random order: ");

  for (int i = 0; i < cfg.sticks; i++) {

    Serial.print(
      stickOrder[i] + 1);

    if (i < cfg.sticks - 1) {
      Serial.print(" -> ");
    }
  }

  Serial.println();
}


void clearLanes() {

  for (int i = 0; i < MAX_STICKS; i++) {

    laneState[i] = 'h';

    droppedAt[i] = 0;
  }
}


// =====================================================
// START GAME
// =====================================================

void beginRound() {

  // Make sure all hooks are at initial position
  beginReset();


  // Create a new random order
  generateRandomOrder();


  // Reset counters
  releasedCount = 0;

  roundCaught = 0;

  roundMissed = 0;

  clearLanes();


  countdownStart = millis();

  lastBeepSecond = -1;

  gameState = COUNTDOWN;


  if (marathonRound > 0) {

    Serial.print("Marathon round ");
    Serial.print(marathonRound);
    Serial.print(" of ");
    Serial.println(cfg.marathonRounds);
  }

  if (cfg.mode == MODE_SPEEDUP) {

    Serial.print("Speed level ");
    Serial.println(speedLevel + 1);
  }

  Serial.print("Starting in ");
  Serial.print(cfg.startDelay);
  Serial.println(" ms...");
}


void startGame() {

  // Don't start if already running
  if (gameState != WAITING) {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("START BUTTON PRESSED");
  Serial.println("================================");


  marathonRound =
    cfg.mode == MODE_MARATHON ? 1 : 0;

  totalCaught = 0;

  totalMissed = 0;


  buzzer.start();

  beginRound();
}


// =====================================================
// STOP GAME
// =====================================================

void stopGame() {

  Serial.println();
  Serial.println("================================");
  Serial.println("GAME STOPPED");
  Serial.println("================================");


  gameState = WAITING;

  releasedCount = 0;

  marathonRound = 0;

  speedLevel = 0;

  clearLanes();


  buzzer.stopped();


  // Return all hooks to initial position
  beginReset();


  Serial.println("Waiting for START button...");
}


// =====================================================
// RELEASE STICKS
// =====================================================

void releaseStick(int stick, unsigned long now) {

  Serial.println();
  Serial.print("Releasing STICK ");
  Serial.println(stick + 1);


  // ---------------------------------------------------
  // IMPORTANT
  //
  // The hook stays at the release position until every
  // stick has fallen.
  // ---------------------------------------------------

  setServoAngle(
    stick,
    cfg.releaseAngle);


  laneState[stick] = 'd';

  droppedAt[stick] = now;

  releasedCount++;
}


void releaseStep(unsigned long now) {

  releaseStick(
    stickOrder[releasedCount],
    now);


  // Double drop: a second stick at the same moment

  if (cfg.mode == MODE_DOUBLE && releasedCount < cfg.sticks) {

    releaseStick(
      stickOrder[releasedCount],
      now);
  }


  buzzer.release();


  Serial.print("Released: ");
  Serial.print(releasedCount);
  Serial.print("/");
  Serial.println(cfg.sticks);


  // ---------------------------------------------------
  // If all sticks are released
  // ---------------------------------------------------

  if (releasedCount >= cfg.sticks) {

    // Small pause before scoring and resetting
    roundEndAt = now + 500;

    gameState = ROUND_END;

    return;
  }


  // ---------------------------------------------------
  // Random gap before the next stick
  // ---------------------------------------------------

  unsigned long gap =
    random(
      cfg.minDelay,
      cfg.maxDelay + 1);


  if (cfg.mode == MODE_SPEEDUP) {

    gap = max(
      SPEEDUP_MIN_GAP,
      (unsigned long)(gap * pow(SPEEDUP_FACTOR, speedLevel)));
  }


  // The hook gets releaseTime to move, then the gap runs
  currentDelay = cfg.releaseTime + gap;

  lastActionTime = now;


  Serial.print("Next stick after ");
  Serial.print(gap);
  Serial.println(" ms");


  gameState = BETWEEN_STICKS;
}


// =====================================================
// FLOOR SENSORS
// =====================================================

bool lanesPending() {

  if (!sensors.present()) {
    return false;
  }

  for (int i = 0; i < cfg.sticks; i++) {

    if (laneState[i] == 'd') {
      return true;
    }
  }

  return false;
}


void updateSensors(unsigned long now) {

  if (!sensors.present() || now - lastSensorRead < 5) {
    return;
  }

  lastSensorRead = now;


  uint32_t bits = sensors.read();

  // Sensors that just became active
  uint32_t landed = bits & ~lastSensorBits;

  lastSensorBits = bits;


  for (int i = 0; i < cfg.sticks; i++) {

    if (laneState[i] != 'd') {
      continue;
    }


    // A lane without a sensor can't be scored
    if (i >= sensors.lanes()) {

      laneState[i] = 'c';

      continue;
    }


    if (landed & (1UL << i)) {

      laneState[i] = 'm';

      roundMissed++;

      totalMissed++;

      buzzer.miss();

      Serial.print("Stick ");
      Serial.print(i + 1);
      Serial.println(" MISSED");
    }

    else if (now - droppedAt[i] >= MISS_WINDOW_MS) {

      laneState[i] = 'c';

      roundCaught++;

      totalCaught++;

      Serial.print("Stick ");
      Serial.print(i + 1);
      Serial.println(" caught");
    }
  }
}


// =====================================================
// ROUND OVER
// =====================================================

void finishRound() {

  Serial.println();
  Serial.println("================================");
  Serial.print("ALL ");
  Serial.print(cfg.sticks);
  Serial.println(" STICKS HAVE FALLEN");
  Serial.println("================================");


  if (sensors.present()) {

    Serial.print("Round result: ");
    Serial.print(roundCaught);
    Serial.print("/");
    Serial.print(cfg.sticks);
    Serial.println(" caught");

    if (roundCaught > bestCaught) {
      bestCaught = roundCaught;
    }
  }


  if (cfg.mode == MODE_SPEEDUP) {

    speedLevel++;

    Serial.print("Next round is faster: gaps x");
    Serial.println(pow(SPEEDUP_FACTOR, speedLevel), 2);
  }


  buzzer.roundDone();


  // Return all hooks to HOLD
  beginReset();

  gameState = RESETTING;
}


void afterReset(unsigned long now) {

  // Marathon: pause, then the next round starts by itself

  if (marathonRound > 0 && marathonRound < cfg.marathonRounds) {

    marathonRound++;

    reloadUntil = now + cfg.reloadPause;

    gameState = RELOADING;

    Serial.println();
    Serial.print("Reload the sticks! Next round in ");
    Serial.print(cfg.reloadPause / 1000);
    Serial.println(" s");

    return;
  }


  if (marathonRound > 0) {

    Serial.println();
    Serial.println("MARATHON COMPLETE");

    if (sensors.present()) {

      Serial.print("Total: ");
      Serial.print(totalCaught);
      Serial.print("/");
      Serial.print(cfg.sticks * cfg.marathonRounds);
      Serial.println(" caught");
    }

    marathonRound = 0;
  }


  Serial.println();
  Serial.println("GAME READY");
  Serial.println("Waiting for START button...");


  gameState = WAITING;
}


// =====================================================
// GAME STATE MACHINE
// =====================================================

void gameLoop(unsigned long now) {

  switch (gameState) {


    case WAITING:

      break;


    case COUNTDOWN: {

      unsigned long elapsed =
        now - countdownStart;


      // Beep on the last three seconds

      if (elapsed < cfg.startDelay) {

        int remaining =
          (cfg.startDelay - elapsed + 999) / 1000;

        if (remaining <= 3 && remaining != lastBeepSecond) {

          lastBeepSecond = remaining;

          buzzer.tick();
        }
      }


      // Go once the delay is over and every hook is at HOLD

      if (elapsed >= cfg.startDelay && !resetBusy()) {

        Serial.println();
        Serial.println("GO!");

        buzzer.go();

        gameState = RELEASE_STICK;
      }

      break;
    }


    case RELEASE_STICK:

      releaseStep(now);

      break;


    case BETWEEN_STICKS:

      if (now - lastActionTime >= currentDelay) {

        gameState = RELEASE_STICK;
      }

      break;


    case ROUND_END:

      // Wait for the floor sensors to decide every stick

      if ((long)(now - roundEndAt) >= 0 && !lanesPending()) {

        finishRound();
      }

      break;


    case RESETTING:

      if (!resetBusy()) {

        afterReset(now);
      }

      break;


    case RELOADING:

      if ((long)(now - reloadUntil) >= 0) {

        Serial.println();
        Serial.println("================================");
        Serial.println("NEXT ROUND");
        Serial.println("================================");

        beginRound();
      }

      break;
  }
}


// =====================================================
// STATUS LED
// =====================================================

void updateLed(unsigned long now) {

  bool on;

  switch (gameState) {

    case WAITING:
      on = false;
      break;

    // Fast blink: get ready
    case COUNTDOWN:
      on = (now / 250) % 2;
      break;

    // Slow blink: reload the sticks
    case RELOADING:
      on = (now / 600) % 2;
      break;

    // Solid: round running
    default:
      on = true;
      break;
  }

  digitalWrite(
    LED_PIN,
    on ? HIGH : LOW);
}


// =====================================================
// PHYSICAL START BUTTON
// =====================================================

// Debounced without delay(): the reading must stay the
// same for 30 ms before it counts

void readButton(unsigned long now) {

  bool reading =
    digitalRead(
      START_BUTTON);


  if (reading != buttonRaw) {

    buttonRaw = reading;

    buttonChangedAt = now;
  }


  if (buttonRaw != buttonStable && now - buttonChangedAt >= 30) {

    buttonStable = buttonRaw;


    // HIGH -> LOW = pressed. Only starts if waiting.

    if (buttonStable == LOW) {

      startGame();
    }
  }
}


// =====================================================
// TEXT HELPERS
// =====================================================

const char* modeName(int mode) {

  switch (mode) {

    case MODE_SPEEDUP:
      return "SPEED-UP";

    case MODE_DOUBLE:
      return "DOUBLE DROP";

    case MODE_MARATHON:
      return "MARATHON";

    default:
      return "CLASSIC";
  }
}


String statusText() {

  unsigned long now = millis();


  switch (gameState) {


    case WAITING:

      return "WAITING";


    case COUNTDOWN: {

      unsigned long elapsed =
        now - countdownStart;


      unsigned long remaining =
        0;


      if (elapsed < cfg.startDelay) {

        remaining =
          (cfg.startDelay - elapsed + 999) / 1000;
      }


      return "STARTING IN " + String(remaining) + "s";
    }


    case RELEASE_STICK:

      return "RELEASING";


    case BETWEEN_STICKS:

      return "WAITING - " + String(releasedCount) + "/" + String(cfg.sticks);


    case ROUND_END:

      return "ROUND OVER";


    case RESETTING:

      return "RESETTING";


    case RELOADING: {

      long remaining =
        ((long)(reloadUntil - now) + 999) / 1000;

      return "RELOAD - NEXT ROUND IN " + String(max(remaining, 0L)) + "s";
    }
  }

  return "";
}


const char* stateName() {

  switch (gameState) {

    case COUNTDOWN:      return "COUNTDOWN";
    case RELEASE_STICK:  return "RELEASE_STICK";
    case BETWEEN_STICKS: return "BETWEEN_STICKS";
    case ROUND_END:      return "ROUND_END";
    case RESETTING:      return "RESETTING";
    case RELOADING:      return "RELOADING";
    default:             return "WAITING";
  }
}


void saveSettings() {

  settingsSave(cfg);

  Serial.println("Settings saved to flash.");
}


// Answers 409 and returns false while a round runs

bool requireIdle() {

  if (gameState == WAITING) {
    return true;
  }

  server.send(
    409,
    "text/plain",
    "GAME RUNNING");

  return false;
}


// =====================================================
// WEB ROOT
// =====================================================

// The page lives in flash (PROGMEM) and is streamed from
// there, so it doesn't take ~20 KB of RAM on every load.

void handleRoot() {

  server.send_P(
    200,
    "text/html",
    INDEX_HTML);
}


// Captive portal: send any other address to the panel

void handleNotFound() {

  server.sendHeader(
    "Location",
    "http://192.168.4.1/",
    true);

  server.send(
    302,
    "text/plain",
    "");
}


// =====================================================
// WEB START / STOP
// =====================================================

void handleStart() {

  startGame();

  server.send(
    200,
    "text/plain",
    "STARTED");
}


void handleStop() {

  stopGame();

  server.send(
    200,
    "text/plain",
    "STOPPED");
}


// =====================================================
// WEB TIMING SETTINGS
// =====================================================

void handleSettings() {


  if (server.hasArg("start")) {

    cfg.startDelay =
      constrain(server.arg("start").toInt(), 0, 60000);
  }


  if (server.hasArg("min")) {

    cfg.minDelay =
      constrain(server.arg("min").toInt(), 0, 60000);
  }


  if (server.hasArg("max")) {

    cfg.maxDelay =
      constrain(server.arg("max").toInt(), 0, 60000);
  }


  if (server.hasArg("release")) {

    cfg.releaseTime =
      constrain(server.arg("release").toInt(), 0, 60000);
  }


  // Safety check and limits (see settings.h)

  settingsClamp(
    cfg,
    availableSticks);


  Serial.println();
  Serial.println("Timing settings updated.");

  Serial.print("Start delay: ");
  Serial.println(cfg.startDelay);

  Serial.print("Min delay: ");
  Serial.println(cfg.minDelay);

  Serial.print("Max delay: ");
  Serial.println(cfg.maxDelay);

  Serial.print("Release time: ");
  Serial.println(cfg.releaseTime);


  saveSettings();


  server.send(
    200,
    "text/plain",
    "TIMING SAVED");
}


// =====================================================
// WEB SERVO SETTINGS
// =====================================================

void handleServoSettings() {


  if (server.hasArg("hold")) {

    cfg.holdAngle =
      constrain(server.arg("hold").toInt(), 0, 180);
  }


  if (server.hasArg("release")) {

    cfg.releaseAngle =
      constrain(server.arg("release").toInt(), 0, 180);
  }


  Serial.println();
  Serial.println("Servo settings updated.");

  Serial.print("Hold angle: ");
  Serial.println(cfg.holdAngle);

  Serial.print("Release angle: ");
  Serial.println(cfg.releaseAngle);


  saveSettings();


  // Only move servos if game isn't running

  if (gameState == WAITING) {

    beginReset();
  }


  server.send(
    200,
    "text/plain",
    "SERVO SETTINGS SAVED");
}


// =====================================================
// WEB STICK COUNT
// =====================================================

void handleSticks() {


  if (!server.hasArg("count")) {

    server.send(
      400,
      "text/plain",
      "NO COUNT");

    return;
  }


  // The shuffled order is built for the current
  // count, so only change it between rounds

  if (!requireIdle()) {
    return;
  }


  cfg.sticks =
    constrain(
      server.arg("count").toInt(),
      1,
      availableSticks);


  Serial.println();
  Serial.print("Stick count updated: ");
  Serial.println(cfg.sticks);


  saveSettings();

  clearLanes();


  // Move any newly added hooks to HOLD

  beginReset();


  server.send(
    200,
    "text/plain",
    String(cfg.sticks));
}


// =====================================================
// WEB GAME MODE
// =====================================================

void handleMode() {


  if (!requireIdle()) {
    return;
  }


  if (server.hasArg("mode")) {

    cfg.mode =
      constrain(server.arg("mode").toInt(), 0, MODE_COUNT - 1);
  }


  if (server.hasArg("rounds")) {

    cfg.marathonRounds =
      constrain(server.arg("rounds").toInt(), 0, 255);
  }


  if (server.hasArg("pause")) {

    cfg.reloadPause =
      constrain(server.arg("pause").toInt(), 0, 60000);
  }


  settingsClamp(
    cfg,
    availableSticks);


  // A new mode starts at normal speed
  speedLevel = 0;


  Serial.println();
  Serial.print("Game mode: ");
  Serial.println(modeName(cfg.mode));


  saveSettings();


  server.send(
    200,
    "text/plain",
    "MODE SAVED");
}


// =====================================================
// WEB TEST SERVO
// =====================================================

void handleTestServo() {


  if (!server.hasArg("servo")) {

    server.send(
      400,
      "text/plain",
      "NO SERVO");

    return;
  }


  int servo =
    server.arg("servo").toInt();


  if (
    servo < 0 || servo >= cfg.sticks) {

    server.send(
      400,
      "text/plain",
      "INVALID SERVO");

    return;
  }


  // Don't test while game running

  if (gameState != WAITING) {

    server.send(
      200,
      "text/plain",
      "GAME RUNNING");

    return;
  }


  Serial.print("Testing servo ");
  Serial.println(servo + 1);


  // Release now, back to HOLD after releaseTime
  // (updateTests() in loop)

  setServoAngle(
    servo,
    cfg.releaseAngle);


  testReturnAt[servo] =
    millis() + cfg.releaseTime;


  server.send(
    200,
    "text/plain",
    "SERVO TESTED");
}


// =====================================================
// WEB TRIM ONE HOOK
// =====================================================

// Corrects a hook whose horn sits a little off, by adding
// a few degrees to both its HOLD and RELEASE angles

void handleTrim() {


  if (!server.hasArg("servo") || !server.hasArg("value")) {

    server.send(
      400,
      "text/plain",
      "NEED servo AND value");

    return;
  }


  int servo =
    server.arg("servo").toInt();


  if (servo < 0 || servo >= availableSticks) {

    server.send(
      400,
      "text/plain",
      "INVALID SERVO");

    return;
  }


  cfg.trim[servo] =
    constrain(server.arg("value").toInt(), -MAX_TRIM, MAX_TRIM);


  Serial.print("Trim servo ");
  Serial.print(servo + 1);
  Serial.print(": ");
  Serial.print(cfg.trim[servo]);
  Serial.println(" deg");


  saveSettings();


  // Show the new HOLD position straight away

  if (gameState == WAITING && !testReturnAt[servo]) {

    setServoAngle(
      servo,
      cfg.holdAngle);
  }


  server.send(
    200,
    "text/plain",
    "TRIM SAVED");
}


// =====================================================
// WEB SOUND
// =====================================================

void handleSound() {


  if (server.hasArg("on")) {

    cfg.sound =
      server.arg("on").toInt() ? 1 : 0;
  }


  buzzer.setEnabled(cfg.sound);

  buzzer.saved();


  saveSettings();


  server.send(
    200,
    "text/plain",
    cfg.sound ? "SOUND ON" : "SOUND OFF");
}


// =====================================================
// WEB RESTORE DEFAULTS
// =====================================================

void handleDefaults() {


  if (!requireIdle()) {
    return;
  }


  settingsDefaults(cfg);

  settingsClamp(
    cfg,
    availableSticks);


  Serial.println();
  Serial.println("Settings restored to defaults.");


  saveSettings();

  buzzer.setEnabled(cfg.sound);

  speedLevel = 0;

  clearLanes();

  beginReset();


  server.send(
    200,
    "text/plain",
    "DEFAULTS RESTORED");
}


// =====================================================
// WEB CONFIG
// =====================================================

// Current settings as JSON, so the control panel
// shows what the board is really using.

void handleConfig() {

  String json = "{";

  json += "\"version\":\"" FIRMWARE_VERSION "\"";
  json += ",\"sticks\":" + String(cfg.sticks);
  json += ",\"maxSticks\":" + String(availableSticks);
  json += ",\"boards\":" + String(board2Found ? 2 : 1);
  json += ",\"sensors\":" + String(sensors.lanes());
  json += ",\"start\":" + String(cfg.startDelay);
  json += ",\"min\":" + String(cfg.minDelay);
  json += ",\"max\":" + String(cfg.maxDelay);
  json += ",\"release\":" + String(cfg.releaseTime);
  json += ",\"hold\":" + String(cfg.holdAngle);
  json += ",\"releaseAngle\":" + String(cfg.releaseAngle);
  json += ",\"mode\":" + String(cfg.mode);
  json += ",\"rounds\":" + String(cfg.marathonRounds);
  json += ",\"pause\":" + String(cfg.reloadPause);
  json += ",\"sound\":" + String(cfg.sound);

  json += ",\"trim\":[";

  for (int i = 0; i < availableSticks; i++) {

    if (i) {
      json += ",";
    }

    json += String(cfg.trim[i]);
  }

  json += "]}";


  server.send(
    200,
    "application/json",
    json);
}


// =====================================================
// WEB LIVE STATE
// =====================================================

// Everything the panel shows while a round runs. Polled
// twice a second, so every connected phone stays in sync.

void handleState() {

  String lanes;

  for (int i = 0; i < cfg.sticks; i++) {
    lanes += laneState[i];
  }


  String json = "{";

  json += "\"state\":\"" + String(stateName()) + "\"";
  json += ",\"text\":\"" + statusText() + "\"";
  json += ",\"sticks\":" + String(cfg.sticks);
  json += ",\"released\":" + String(releasedCount);
  json += ",\"lanes\":\"" + lanes + "\"";
  json += ",\"mode\":" + String(cfg.mode);
  json += ",\"speed\":" + String(pow(SPEEDUP_FACTOR, speedLevel), 2);
  json += ",\"round\":" + String(marathonRound);
  json += ",\"rounds\":" + String(cfg.marathonRounds);
  json += ",\"sensors\":" + String(sensors.present() ? "true" : "false");
  json += ",\"caught\":" + String(roundCaught);
  json += ",\"missed\":" + String(roundMissed);
  json += ",\"totalCaught\":" + String(totalCaught);
  json += ",\"totalMissed\":" + String(totalMissed);
  json += ",\"best\":" + String(bestCaught);

  json += "}";


  server.send(
    200,
    "application/json",
    json);
}


// =====================================================
// WEB STATUS
// =====================================================

// Plain-text status, kept for scripts (see README)

void handleStatus() {

  server.send(
    200,
    "text/plain",
    statusText());
}


// =====================================================
// SETUP
// =====================================================

bool i2cFound(uint8_t address) {

  Wire.beginTransmission(address);

  return Wire.endTransmission() == 0;
}


void setup() {


  Serial.begin(115200);

  delay(500);


  Serial.println();
  Serial.println();
  Serial.println("================================");
  Serial.println("       STICK CATCHER");
  Serial.println("================================");

  Serial.print("Firmware ");
  Serial.println(FIRMWARE_VERSION);


  // ===================================================
  // PINS
  // ===================================================

  pinMode(
    START_BUTTON,
    INPUT_PULLUP);

  pinMode(
    LED_PIN,
    OUTPUT);

  buzzer.begin(BUZZER_PIN);


  // ===================================================
  // I2C
  // ===================================================

  // GPIO4 = SDA
  // GPIO5 = SCL

  Wire.begin(
    4,
    5);


  // ===================================================
  // PCA9685 BOARDS
  // ===================================================

  pwm1.begin();

  // PCA9685 oscillator
  pwm1.setOscillatorFrequency(27000000);

  // SG90 = approximately 50Hz
  pwm1.setPWMFreq(50);


  board2Found = i2cFound(0x41);

  if (board2Found) {

    pwm2.begin();
    pwm2.setOscillatorFrequency(27000000);
    pwm2.setPWMFreq(50);

    availableSticks = MAX_STICKS;
  }

  Serial.print("Servo boards: ");
  Serial.print(board2Found ? 2 : 1);
  Serial.print(" (up to ");
  Serial.print(availableSticks);
  Serial.println(" sticks)");


  // ===================================================
  // SAVED SETTINGS
  // ===================================================

  if (settingsLoad(cfg)) {

    Serial.println("Settings loaded from flash.");

  } else {

    Serial.println("No saved settings, using defaults.");
  }


  if (cfg.sticks > availableSticks) {

    Serial.print("WARNING: ");
    Serial.print(cfg.sticks);
    Serial.println(" sticks saved but board 2 (0x41) not found.");
  }

  settingsClamp(
    cfg,
    availableSticks);


  buzzer.setEnabled(cfg.sound);


  Serial.print("Sticks: ");
  Serial.print(cfg.sticks);
  Serial.print(" of ");
  Serial.println(availableSticks);

  Serial.print("Mode: ");
  Serial.println(modeName(cfg.mode));


  // ===================================================
  // FLOOR SENSORS (OPTIONAL)
  // ===================================================

  if (sensors.begin()) {

    Serial.print("Floor sensors: ");
    Serial.print(sensors.lanes());
    Serial.println(" lanes (scoring on)");

  } else {

    Serial.println("Floor sensors: none (no scoring)");
  }


  // ===================================================
  // INITIAL SERVO POSITION
  // ===================================================

  clearLanes();

  delay(500);

  beginReset();


  // ===================================================
  // RANDOM SEED
  // ===================================================

  randomSeed(
    micros());


  // ===================================================
  // WIFI AP
  // ===================================================

  WiFi.mode(
    WIFI_AP);

  WiFi.softAPConfig(
    apIP,
    apIP,
    IPAddress(255, 255, 255, 0));

  WiFi.softAP(
    AP_SSID,
    AP_PASSWORD);


  Serial.println();
  Serial.println("WiFi Access Point:");

  Serial.print("SSID: ");
  Serial.println(AP_SSID);

  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);

  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());


  // Captive portal DNS
  dnsServer.start(
    53,
    "*",
    apIP);


  // ===================================================
  // WEB SERVER
  // ===================================================

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/settings", handleSettings);
  server.on("/servo", handleServoSettings);
  server.on("/test", handleTestServo);
  server.on("/trim", handleTrim);
  server.on("/sticks", handleSticks);
  server.on("/mode", handleMode);
  server.on("/sound", handleSound);
  server.on("/defaults", handleDefaults);
  server.on("/config", handleConfig);
  server.on("/state", handleState);
  server.on("/status", handleStatus);

  server.onNotFound(handleNotFound);


  // Firmware upload page: http://192.168.4.1/update
  httpUpdater.setup(
    &server,
    "/update",
    "admin",
    OTA_PASSWORD);


  server.begin();


  Serial.println();
  Serial.println("Web server started.");


  // ===================================================
  // WIRELESS UPDATES (PlatformIO / Arduino IDE)
  // ===================================================

  ArduinoOTA.setHostname("stick-catcher");

  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {

    // Park everything before the flash is rewritten
    gameState = WAITING;
    buzzer.stop();
    digitalWrite(LED_PIN, LOW);

    Serial.println("OTA update started...");
  });

  ArduinoOTA.begin();


  Serial.println();
  Serial.println("Connect your phone/laptop to:");
  Serial.println(AP_SSID);

  Serial.println();
  Serial.println("Then open:");
  Serial.println("http://192.168.4.1");

  Serial.println();
  Serial.println("Waiting for physical START button...");

  Serial.println("================================");
}


// =====================================================
// LOOP
// =====================================================

// Nothing in here waits: every step checks the time and
// returns, so web requests are answered during a round.

void loop() {

  unsigned long now = millis();


  dnsServer.processNextRequest();

  server.handleClient();

  ArduinoOTA.handle();


  readButton(now);

  updateReset(now);

  updateTests(now);

  updateSensors(now);


  gameLoop(now);


  buzzer.update();

  updateLed(now);
}
