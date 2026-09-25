#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "web_page.h"

// =====================================================
// WIFI ACCESS POINT
// =====================================================

const char* AP_SSID = "STICK-CATCHER";
const char* AP_PASSWORD = "12345678";

ESP8266WebServer server(80);


// =====================================================
// PCA9685
// =====================================================

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// PCA9685 channels
// Servo 1 = CH0
// Servo 2 = CH1
// Servo 3 = CH2
// Servo 4 = CH3
// Servo 5 = CH4
// Servo 6 = CH5

const uint8_t servoChannel[6] = {
  0,
  1,
  2,
  3,
  4,
  5
};


// =====================================================
// ESP8266 GPIO
// =====================================================

// Physical START button
// GPIO14 = D5 on NodeMCU
#define START_BUTTON 14

// I2C
// SDA = GPIO4
// SCL = GPIO5


// =====================================================
// SERVO SETTINGS
// =====================================================

// Initial position
int holdAngle = 0;

// Position where hook releases stick
int releaseAngle = 90;

// Time servo stays at release position
int releaseTime = 200;


// =====================================================
// GAME TIMING
// =====================================================

// Delay between START button and first stick
int startDelay = 5000;

// Random delay between sticks
int minDelay = 500;
int maxDelay = 2000;


// =====================================================
// PCA9685 SERVO CALIBRATION
// =====================================================

// These are starting values.
// Calibrate these for your actual SG90s.

#define SERVO_MIN 150
#define SERVO_MAX 600


// =====================================================
// GAME STATE
// =====================================================

enum GameState {

  WAITING,
  COUNTDOWN,
  RELEASE_STICK,
  BETWEEN_STICKS,
  RESETTING

};

GameState gameState = WAITING;


// =====================================================
// GAME VARIABLES
// =====================================================

// Number of sticks
const int TOTAL_STICKS = 6;

// Randomized order
int stickOrder[TOTAL_STICKS];

// Number already released
int releasedCount = 0;

// Currently selected servo
int currentStick = -1;

// Timing variables
unsigned long countdownStart = 0;

unsigned long lastActionTime = 0;

unsigned long currentDelay = 0;


// =====================================================
// BUTTON
// =====================================================

bool lastButtonState = HIGH;


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

void setServoAngle(int stick, int angle) {

  if (stick < 0 || stick >= TOTAL_STICKS) {
    return;
  }

  uint16_t pulse = angleToPulse(angle);

  pwm.setPWM(
    servoChannel[stick],
    0,
    pulse);
}


// =====================================================
// RESET ALL SERVOS
// =====================================================

void resetAllServos() {

  Serial.println();
  Serial.println("Resetting all servos...");

  for (int i = 0; i < TOTAL_STICKS; i++) {

    setServoAngle(
      i,
      holdAngle);

    delay(100);
  }

  Serial.println("All servos at HOLD position.");
}


// =====================================================
// GENERATE RANDOM ORDER
// =====================================================

void generateRandomOrder() {

  // Start with:
  //
  // 0 1 2 3 4 5

  for (int i = 0; i < TOTAL_STICKS; i++) {

    stickOrder[i] = i;
  }


  // Fisher-Yates shuffle
  //
  // Ensures every stick is used exactly once.

  for (int i = TOTAL_STICKS - 1; i > 0; i--) {

    int j = random(
      0,
      i + 1);

    int temp = stickOrder[i];

    stickOrder[i] = stickOrder[j];

    stickOrder[j] = temp;
  }


  // Serial monitor
  Serial.print("Random order: ");

  for (int i = 0; i < TOTAL_STICKS; i++) {

    Serial.print(
      stickOrder[i] + 1);

    if (i < TOTAL_STICKS - 1) {
      Serial.print(" -> ");
    }
  }

  Serial.println();
}


// =====================================================
// START GAME
// =====================================================

void startGame() {

  // Don't start if already running
  if (gameState != WAITING) {
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("START BUTTON PRESSED");
  Serial.println("================================");


  // Make sure all hooks are at initial position
  resetAllServos();


  // Create a new random order
  generateRandomOrder();


  // Reset counters
  releasedCount = 0;

  currentStick = -1;


  // Start 5-second countdown
  countdownStart = millis();

  gameState = COUNTDOWN;


  Serial.print("Starting in ");
  Serial.print(startDelay);
  Serial.println(" ms...");
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

  currentStick = -1;


  // Return all hooks to initial position
  resetAllServos();


  Serial.println("Waiting for START button...");
}


// =====================================================
// RELEASE ONE STICK
// =====================================================

void releaseCurrentStick() {

  // Safety
  if (releasedCount >= TOTAL_STICKS) {

    gameState = RESETTING;

    return;
  }


  // Get next stick from shuffled list
  currentStick =
    stickOrder[releasedCount];


  Serial.println();
  Serial.print("Releasing STICK ");
  Serial.println(currentStick + 1);


  // ---------------------------------------------------
  // Move hook to release position
  // ---------------------------------------------------

  setServoAngle(
    currentStick,
    releaseAngle);


  // Give the servo time to move
  delay(releaseTime);


  // ---------------------------------------------------
  // IMPORTANT
  //
  // We DON'T return the servo to 0° here.
  //
  // It remains at release position until all 6
  // sticks have fallen.
  // ---------------------------------------------------


  releasedCount++;


  Serial.print("Released: ");
  Serial.print(releasedCount);
  Serial.print("/");
  Serial.println(TOTAL_STICKS);


  // ---------------------------------------------------
  // If all sticks are released
  // ---------------------------------------------------

  if (releasedCount >= TOTAL_STICKS) {

    gameState = RESETTING;

    return;
  }


  // ---------------------------------------------------
  // Generate random delay for next stick
  // ---------------------------------------------------

  currentDelay =
    random(
      minDelay,
      maxDelay + 1);


  lastActionTime = millis();


  Serial.print("Next stick after ");
  Serial.print(currentDelay);
  Serial.println(" ms");


  gameState = BETWEEN_STICKS;
}


// =====================================================
// RESET AFTER GAME
// =====================================================

void resetAfterGame() {

  Serial.println();
  Serial.println("================================");
  Serial.println("ALL 6 STICKS HAVE FALLEN");
  Serial.println("================================");


  // Small pause before resetting
  delay(500);


  // Return all hooks to 0°
  resetAllServos();


  currentStick = -1;

  releasedCount = 0;


  Serial.println();
  Serial.println("GAME READY");
  Serial.println("Waiting for START button...");


  gameState = WAITING;
}


// =====================================================
// GAME STATE MACHINE
// =====================================================

void gameLoop() {

  unsigned long now = millis();


  // ===================================================
  // WAITING
  // ===================================================

  if (gameState == WAITING) {

    return;
  }


  // ===================================================
  // COUNTDOWN
  // ===================================================

  if (gameState == COUNTDOWN) {

    unsigned long elapsed =
      now - countdownStart;


    if (elapsed >= startDelay) {

      Serial.println();
      Serial.println("GO!");

      gameState = RELEASE_STICK;
    }

    return;
  }


  // ===================================================
  // RELEASE NEXT STICK
  // ===================================================

  if (gameState == RELEASE_STICK) {

    releaseCurrentStick();

    return;
  }


  // ===================================================
  // WAIT BETWEEN STICKS
  // ===================================================

  if (gameState == BETWEEN_STICKS) {

    if (
      now - lastActionTime >= currentDelay) {

      gameState = RELEASE_STICK;
    }

    return;
  }


  // ===================================================
  // RESET
  // ===================================================

  if (gameState == RESETTING) {

    resetAfterGame();

    return;
  }
}


// =====================================================
// WEB ROOT
// =====================================================

void handleRoot() {

  server.send(
    200,
    "text/html",
    getHTML());
}


// =====================================================
// WEB START
// =====================================================

void handleStart() {

  startGame();

  server.send(
    200,
    "text/plain",
    "STARTED");
}


// =====================================================
// WEB STOP
// =====================================================

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

    startDelay =
      server.arg("start").toInt();
  }


  if (server.hasArg("min")) {

    minDelay =
      server.arg("min").toInt();
  }


  if (server.hasArg("max")) {

    maxDelay =
      server.arg("max").toInt();
  }


  if (server.hasArg("release")) {

    releaseTime =
      server.arg("release").toInt();
  }


  // Safety check

  if (minDelay > maxDelay) {

    int temp = minDelay;

    minDelay = maxDelay;

    maxDelay = temp;
  }


  // Limits

  startDelay =
    constrain(
      startDelay,
      1000,
      30000);


  minDelay =
    constrain(
      minDelay,
      50,
      30000);


  maxDelay =
    constrain(
      maxDelay,
      50,
      30000);


  releaseTime =
    constrain(
      releaseTime,
      50,
      2000);


  Serial.println();
  Serial.println("Timing settings updated.");

  Serial.print("Start delay: ");
  Serial.println(startDelay);

  Serial.print("Min delay: ");
  Serial.println(minDelay);

  Serial.print("Max delay: ");
  Serial.println(maxDelay);

  Serial.print("Release time: ");
  Serial.println(releaseTime);


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

    holdAngle =
      server.arg("hold").toInt();
  }


  if (server.hasArg("release")) {

    releaseAngle =
      server.arg("release").toInt();
  }


  holdAngle =
    constrain(
      holdAngle,
      0,
      180);


  releaseAngle =
    constrain(
      releaseAngle,
      0,
      180);


  Serial.println();
  Serial.println("Servo settings updated.");

  Serial.print("Hold angle: ");
  Serial.println(holdAngle);

  Serial.print("Release angle: ");
  Serial.println(releaseAngle);


  // Only move servos if game isn't running

  if (gameState == WAITING) {

    resetAllServos();
  }


  server.send(
    200,
    "text/plain",
    "SERVO SETTINGS SAVED");
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
    servo < 0 || servo >= TOTAL_STICKS) {

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


  // Release

  setServoAngle(
    servo,
    releaseAngle);


  delay(releaseTime);


  // Return to hold

  setServoAngle(
    servo,
    holdAngle);


  server.send(
    200,
    "text/plain",
    "SERVO TESTED");
}


// =====================================================
// WEB STATUS
// =====================================================

void handleStatus() {

  String status;


  switch (gameState) {


    case WAITING:

      status = "WAITING";

      break;


    case COUNTDOWN:

      {

        unsigned long elapsed =
          millis() - countdownStart;


        unsigned long remaining =
          0;


        if (elapsed < startDelay) {

          remaining =
            (startDelay - elapsed + 999) / 1000;
        }


        status =
          "STARTING IN " + String(remaining) + "s";
      }

      break;


    case RELEASE_STICK:

      status = "RELEASING";

      break;


    case BETWEEN_STICKS:

      status =
        "WAITING - " + String(releasedCount) + "/6";

      break;


    case RESETTING:

      status = "RESETTING";

      break;
  }


  server.send(
    200,
    "text/plain",
    status);
}


// =====================================================
// SETUP
// =====================================================

void setup() {


  Serial.begin(115200);

  delay(500);


  Serial.println();
  Serial.println();
  Serial.println("================================");
  Serial.println("       STICK CATCHER");
  Serial.println("================================");


  // ===================================================
  // START BUTTON
  // ===================================================

  pinMode(
    START_BUTTON,
    INPUT_PULLUP);


  // ===================================================
  // I2C
  // ===================================================

  // GPIO4 = SDA
  // GPIO5 = SCL

  Wire.begin(
    4,
    5);


  // ===================================================
  // PCA9685
  // ===================================================

  pwm.begin();


  // PCA9685 oscillator

  pwm.setOscillatorFrequency(
    27000000);


  // SG90 = approximately 50Hz

  pwm.setPWMFreq(50);


  delay(500);


  // ===================================================
  // INITIAL SERVO POSITION
  // ===================================================

  resetAllServos();


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


  WiFi.softAP(
    AP_SSID,
    AP_PASSWORD);


  Serial.println();

  Serial.println(
    "WiFi Access Point:");


  Serial.print(
    "SSID: ");

  Serial.println(
    AP_SSID);


  Serial.print(
    "Password: ");

  Serial.println(
    AP_PASSWORD);


  Serial.print(
    "IP address: ");

  Serial.println(
    WiFi.softAPIP());


  // ===================================================
  // WEB SERVER
  // ===================================================

  server.on(
    "/",
    handleRoot);


  server.on(
    "/start",
    handleStart);


  server.on(
    "/stop",
    handleStop);


  server.on(
    "/settings",
    handleSettings);


  server.on(
    "/servo",
    handleServoSettings);


  server.on(
    "/test",
    handleTestServo);


  server.on(
    "/status",
    handleStatus);


  server.begin();


  Serial.println();

  Serial.println(
    "Web server started.");


  Serial.println();

  Serial.println(
    "Connect your phone/laptop to:");


  Serial.println(
    "STICK-CATCHER");


  Serial.println();

  Serial.println(
    "Then open:");


  Serial.println(
    "http://192.168.4.1");


  Serial.println();

  Serial.println(
    "Waiting for physical START button...");


  Serial.println(
    "================================");
}


// =====================================================
// LOOP
// =====================================================

void loop() {


  // Handle web requests

  server.handleClient();


  // ===================================================
  // PHYSICAL START BUTTON
  // ===================================================

  bool buttonState =
    digitalRead(
      START_BUTTON);


  // Detect HIGH -> LOW transition

  if (
    lastButtonState == HIGH && buttonState == LOW) {


    // Debounce

    delay(30);


    if (
      digitalRead(
        START_BUTTON)
      == LOW) {


      // Only start if waiting

      if (
        gameState == WAITING) {

        startGame();
      }
    }
  }


  lastButtonState =
    buttonState;


  // ===================================================
  // GAME STATE MACHINE
  // ===================================================

  gameLoop();
}