// =====================================================
// STICK CATCHER - SERVO TEST
//
// Standalone sketch to check wiring and calibrate the
// hook servos (1 to 32) BEFORE flashing the real game.
//
// Open the Serial Monitor at 115200 baud (line ending:
// "No line ending" or "Newline" both work) and type a
// command. Type ? for the list.
//
// Same hardware as the game:
//   PCA9685 SDA = GPIO4 (D2), SCL = GPIO5 (D1)
//   Servos 1-16 on the PCA9685 at 0x40, CH0-CH15
//   Servos 17-32 on a second PCA9685 at 0x41 (optional)
// =====================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>


// =====================================================
// SETTINGS (keep in sync with stick_catching_game.ino)
// =====================================================

#define SERVO_MIN 150
#define SERVO_MAX 600

// Set to the number of servos you built (1 to 16, or up
// to 32 with a second PCA9685 at 0x41)
const int TOTAL_SERVOS = 6;

const int SERVOS_PER_BOARD = 16;

int holdAngle = 0;
int releaseAngle = 90;
int releaseTime = 400;

// Angle change per '+' / '-'
const int NUDGE_STEP = 5;

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);


// =====================================================
// STATE
// =====================================================

// Servo used by w / + / - (0..TOTAL_SERVOS-1)
int selected = 0;

// Servo number being typed (for numbers above 9)
int typedNumber = 0;
unsigned long typedAt = 0;

// How long to wait for a second digit
const unsigned long TYPE_TIMEOUT = 700;

// Last angle sent to each servo (for + / -)
int currentAngle[TOTAL_SERVOS];


// =====================================================
// SERVO HELPERS
// =====================================================

uint16_t angleToPulse(int angle) {

  angle = constrain(angle, 0, 180);

  return map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
}


void setServoAngle(int servo, int angle) {

  angle = constrain(angle, 0, 180);

  currentAngle[servo] = angle;

  Adafruit_PWMServoDriver& board = servo < SERVOS_PER_BOARD ? pwm1 : pwm2;

  board.setPWM(servo % SERVOS_PER_BOARD, 0, angleToPulse(angle));
}


void printServo(int servo) {

  Serial.print("Servo ");
  Serial.print(servo + 1);
  Serial.print(servo < SERVOS_PER_BOARD ? " (0x40 CH" : " (0x41 CH");
  Serial.print(servo % SERVOS_PER_BOARD);
  Serial.print(") -> ");
  Serial.print(currentAngle[servo]);
  Serial.print(" deg, pulse ");
  Serial.println(angleToPulse(currentAngle[servo]));
}


// =====================================================
// TESTS
// =====================================================

// Release, wait, return to hold - exactly what the game does
void fireServo(int servo) {

  Serial.print("Firing servo ");
  Serial.println(servo + 1);

  setServoAngle(servo, releaseAngle);
  delay(releaseTime);

  setServoAngle(servo, holdAngle);
  delay(200);
}


void fireAll() {

  Serial.print("Firing all servos in order 1..");
  Serial.println(TOTAL_SERVOS);

  for (int i = 0; i < TOTAL_SERVOS; i++) {

    fireServo(i);
    delay(300);
  }

  Serial.println("Done.");
}


void allTo(int angle) {

  for (int i = 0; i < TOTAL_SERVOS; i++) {

    setServoAngle(i, angle);
    delay(100);
  }
}


// Slow 0 -> 180 -> 0 sweep of the selected servo.
// Use it to see the real travel and spot binding or buzzing.
void sweep(int servo) {

  Serial.print("Sweeping servo ");
  Serial.println(servo + 1);

  for (int a = 0; a <= 180; a += 2) {

    setServoAngle(servo, a);
    delay(20);
  }

  for (int a = 180; a >= 0; a -= 2) {

    setServoAngle(servo, a);
    delay(20);
  }

  setServoAngle(servo, holdAngle);
  Serial.println("Done.");
}


// Confirm the PCA9685 answers on the I2C bus
void scanI2C() {

  Serial.println("Scanning I2C bus...");

  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {

    Wire.beginTransmission(addr);

    if (Wire.endTransmission() == 0) {

      Serial.print("  device at 0x");
      Serial.print(addr, HEX);

      if (addr == 0x40) {
        Serial.print("  <- PCA9685 board 1 (expected)");
      }

      if (addr == 0x41) {
        Serial.print("  <- PCA9685 board 2 (servos 17-32)");
      }

      if (addr == 0x20 || addr == 0x21) {
        Serial.print("  <- MCP23017 floor sensors");
      }

      Serial.println();

      found++;
    }
  }

  if (found == 0) {
    Serial.println("  nothing found - check SDA/SCL wiring and PCA9685 power.");
  }
}


// =====================================================
// SERIAL MENU
// =====================================================

void printHelp() {

  Serial.println();
  Serial.println("================================");
  Serial.println("   STICK CATCHER - SERVO TEST");
  Serial.println("================================");
  Serial.print("1-");
  Serial.print(TOTAL_SERVOS);
  Serial.println(TOTAL_SERVOS > 9 ? " select servo and fire it" : "  select servo and fire it");
  Serial.print("a    fire all servos, 1 to ");
  Serial.println(TOTAL_SERVOS);
  Serial.println("w    sweep selected servo 0-180-0");
  Serial.println("+    selected servo +5 deg");
  Serial.println("-    selected servo -5 deg");
  Serial.println("h    all servos to HOLD");
  Serial.println("r    all servos to RELEASE");
  Serial.println("i    scan I2C bus");
  Serial.println("?    show this help");
  Serial.println("--------------------------------");
  Serial.print("HOLD = ");
  Serial.print(holdAngle);
  Serial.print(" deg, RELEASE = ");
  Serial.print(releaseAngle);
  Serial.print(" deg, SERVO_MIN = ");
  Serial.print(SERVO_MIN);
  Serial.print(", SERVO_MAX = ");
  Serial.println(SERVO_MAX);
  Serial.print("Selected servo: ");
  Serial.println(selected + 1);
  Serial.println("================================");
}


// Fire the servo number that was typed
void commitNumber() {

  int n = typedNumber;
  typedNumber = 0;

  if (n < 1 || n > TOTAL_SERVOS) {

    Serial.print("No servo ");
    Serial.print(n);
    Serial.print(" - use 1 to ");
    Serial.println(TOTAL_SERVOS);
    return;
  }

  selected = n - 1;
  fireServo(selected);
}


void handleCommand(char c) {

  // Digits build a servo number. It fires as soon as
  // another digit can't make a valid number (so "3"
  // fires at once with 6 servos, while "1" waits
  // briefly in case you are typing "12").
  if (c >= '0' && c <= '9') {

    typedNumber = typedNumber * 10 + (c - '0');
    typedAt = millis();

    if (typedNumber * 10 > TOTAL_SERVOS) {
      commitNumber();
    }
    return;
  }

  // Any other key (including Enter) ends the number
  if (typedNumber > 0) {
    commitNumber();
  }

  switch (c) {

    case 'a':
    case 'A':
      fireAll();
      break;

    case 'w':
    case 'W':
      sweep(selected);
      break;

    case '+':
    case '=':
      setServoAngle(selected, currentAngle[selected] + NUDGE_STEP);
      printServo(selected);
      break;

    case '-':
    case '_':
      setServoAngle(selected, currentAngle[selected] - NUDGE_STEP);
      printServo(selected);
      break;

    case 'h':
    case 'H':
      Serial.println("All servos -> HOLD");
      allTo(holdAngle);
      break;

    case 'r':
    case 'R':
      Serial.println("All servos -> RELEASE");
      allTo(releaseAngle);
      break;

    case 'i':
    case 'I':
      scanI2C();
      break;

    case '?':
      printHelp();
      break;

    case '\r':
    case '\n':
    case ' ':
      break;

    default:
      Serial.print("Unknown command '");
      Serial.print(c);
      Serial.println("' - type ? for help");
      break;
  }
}


// =====================================================
// SETUP / LOOP
// =====================================================

void setup() {

  Serial.begin(115200);
  delay(500);

  Wire.begin(4, 5);

  pwm1.begin();
  pwm1.setOscillatorFrequency(27000000);
  pwm1.setPWMFreq(50);

  if (TOTAL_SERVOS > SERVOS_PER_BOARD) {
    pwm2.begin();
    pwm2.setOscillatorFrequency(27000000);
    pwm2.setPWMFreq(50);
  }

  delay(500);

  // Start with every hook at HOLD
  allTo(holdAngle);

  scanI2C();
  printHelp();
}


void loop() {

  while (Serial.available()) {

    handleCommand((char)Serial.read());
  }

  if (typedNumber > 0 && millis() - typedAt >= TYPE_TIMEOUT) {
    commitNumber();
  }
}
