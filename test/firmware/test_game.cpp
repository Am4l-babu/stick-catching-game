// Host test for the Stick Catcher firmware: boots the sketch against fakes and plays games.
//
// The headers next to this file stand in for the Arduino, ESP8266 and Adafruit
// libraries (fake clock, I2C devices, flash, web server), so the real game code
// runs on a PC. Build and run from the repository root:
//
//   g++ -std=c++17 -I test/firmware -I stick_catching_game test/firmware/test_game.cpp -o test_game
//   ./test_game
//
// CI runs this on every push (.github/workflows/build.yml).
#include "fakes.h"

unsigned long g_millis = 0;
std::mt19937 g_rng(1);
int g_pinMode[64], g_pinOut[64], g_pinIn[64];
std::vector<ToneEvent> g_tones;
FakeSerial Serial;
std::map<uint8_t, bool> g_i2cPresent;
std::map<uint8_t, FakeMcp> g_mcp;
TwoWire Wire;
std::vector<PwmEvent> g_pwm;
std::vector<uint8_t> g_flash;
int g_flashWrites = 0;
EEPROMClass EEPROM;
FakeWiFi WiFi;
FakeOTA ArduinoOTA;

#include "stick_catching_game.ino"

// ------------------------------------------------------------------ harness
static int passed = 0, failed = 0;
#define CHECK(cond, msg) do { if (cond) passed++; else { failed++; printf("FAIL  %s  (line %d)\n", msg, __LINE__); } } while (0)

static const uint16_t HOLD0 = 150, REL90 = 375;
static uint16_t pulse(int deg) { return (uint16_t)angleToPulse(deg); }

static void step(unsigned long ms) { for (unsigned long i = 0; i < ms; i++) { g_millis++; loop(); } }

template <class F> static bool runUntil(F done, unsigned long maxMs) {
  for (unsigned long i = 0; i < maxMs; i++) { g_millis++; loop(); if (done()) return true; }
  return false;
}

static uint16_t lastPulse(uint8_t addr, uint8_t ch) {
  for (auto it = g_pwm.rbegin(); it != g_pwm.rend(); ++it) if (it->addr == addr && it->ch == ch) return it->off;
  return 0;
}

static bool has(const std::string& text) { return Serial.out.find(text) != std::string::npos; }

// Simulated power cycle: globals back to their initial values, flash kept
static void reboot() {
  gameState = WAITING; board2Found = false; availableSticks = STICKS_PER_BOARD;
  releasedCount = 0; speedLevel = 0; marathonRound = 0; resetNext = -1; bestCaught = -1;
  roundCaught = roundMissed = totalCaught = totalMissed = 0; lastSensorBits = 0; lastSensorRead = 0;
  buttonRaw = buttonStable = HIGH; buttonChangedAt = 0;
  memset(testReturnAt, 0, sizeof testReturnAt);
  memset(&cfg, 0, sizeof cfg);
  sensors = LaneSensors(); buzzer = Buzzer();
  for (auto& kv : g_mcp) kv.second.pins = 0xFFFF;
  for (int i = 0; i < 64; i++) g_pinIn[i] = HIGH;
  Serial.out.clear(); g_pwm.clear(); g_tones.clear();
  setup();
  step(3500);                     // boot reset of up to 32 hooks
}

struct Release { unsigned long at; uint8_t addr, ch; };
static std::vector<Release> releasesSince(size_t from, uint16_t relPulse) {
  std::vector<Release> r;
  for (size_t i = from; i < g_pwm.size(); i++) if (g_pwm[i].off == relPulse) r.push_back({g_pwm[i].at, g_pwm[i].addr, g_pwm[i].ch});
  return r;
}

static void json(const char* tag, const Reply& r) { printf("JSON %s %s\n", tag, r.body.c_str()); }

// Plays one full round/game from WAITING back to WAITING; returns release events
static std::vector<Release> playGame(unsigned long maxMs = 120000) {
  size_t from = g_pwm.size();
  server.request("/start");
  runUntil([] { return gameState == WAITING; }, maxMs);
  return releasesSince(from, pulse(cfg.releaseAngle));
}

// ------------------------------------------------------------------ tests
int main() {
  g_i2cPresent[0x40] = true;

  // T1 first boot
  reboot();
  CHECK(has("No saved settings, using defaults."), "T1 defaults on first boot");
  CHECK(cfg.sticks == 6 && cfg.mode == MODE_CLASSIC, "T1 default sticks/mode");
  bool allHold = true; for (int c = 0; c < 6; c++) allHold &= lastPulse(0x40, c) == HOLD0;
  CHECK(allHold, "T1 hooks 1-6 at HOLD after boot");
  CHECK(lastPulse(0x40, 6) == 0, "T1 hook 7 untouched");
  CHECK(g_flashWrites == 0, "T1 boot does not write flash");
  CHECK(has("Servo boards: 1 (up to 16 sticks)") && has("Floor sensors: none"), "T1 hardware report");

  // T2 classic round, web stays responsive
  {
    size_t from = g_pwm.size(); unsigned long t0 = g_millis;
    CHECK(server.request("/start").body == "STARTED", "T2 start");
    CHECK(server.request("/sticks", {{"count", "8"}}).code == 409, "T2 /sticks refused mid-round");
    CHECK(server.request("/defaults").code == 409, "T2 /defaults refused mid-round");
    bool sawMid = false;
    runUntil([&] {
      if (!sawMid && gameState == BETWEEN_STICKS && releasedCount == 3) {
        Reply st = server.request("/state"); sawMid = true;
        CHECK(st.code == 200 && st.body.find("\"lanes\":\"") != std::string::npos, "T2 /state mid-round");
        size_t p = st.body.find("\"lanes\":\"") + 9; int d = 0;
        for (int i = 0; i < 6; i++) d += st.body[p + i] == 'd';
        CHECK(d == 3, "T2 three lanes dropped");
        CHECK(server.request("/status").body == "WAITING - 3/6", "T2 status text");
        json("state_mid", st);
      }
      return gameState == WAITING; }, 60000);
    auto rel = releasesSince(from, REL90);
    std::vector<int> seen(16, 0); for (auto& r : rel) seen[r.ch]++;
    bool once = rel.size() == 6; for (int c = 0; c < 6; c++) once &= seen[c] == 1;
    CHECK(once, "T2 each of 6 sticks released exactly once");
    CHECK(!rel.empty() && rel[0].at - t0 >= 5000, "T2 first drop after start delay");
    bool gapsOk = true;
    for (size_t i = 1; i < rel.size(); i++) { unsigned long g = rel[i].at - rel[i - 1].at; gapsOk &= g >= 700 && g <= 2201; }
    CHECK(gapsOk, "T2 gaps = release time + 500..2000 ms");
    CHECK(has("ALL 6 STICKS HAVE FALLEN") && has("GAME READY"), "T2 round end messages");
    step(1000);
    allHold = true; for (int c = 0; c < 6; c++) allHold &= lastPulse(0x40, c) == HOLD0;
    CHECK(allHold, "T2 hooks back at HOLD");
    CHECK(!has("Round result"), "T2 no scoring without sensors");
  }

  // T3 settings survive a reboot; unchanged saves don't write flash
  {
    int w0 = g_flashWrites;
    CHECK(server.request("/sticks", {{"count", "10"}}).body == "10", "T3 set 10 sticks");
    server.request("/trim", {{"servo", "2"}, {"value", "12"}});
    server.request("/settings", {{"start", "2000"}, {"min", "100"}, {"max", "300"}, {"release", "100"}});
    server.request("/mode", {{"mode", "1"}});
    CHECK(g_flashWrites == w0 + 4, "T3 four saves = four flash writes");
    server.request("/trim", {{"servo", "2"}, {"value", "12"}});
    CHECK(g_flashWrites == w0 + 4, "T3 same value again: no flash write");
    json("config", server.request("/config"));
    reboot();
    CHECK(has("Settings loaded from flash."), "T3 loaded after reboot");
    CHECK(cfg.sticks == 10 && cfg.trim[2] == 12 && cfg.startDelay == 2000 && cfg.minDelay == 100 && cfg.mode == MODE_SPEEDUP, "T3 values kept");
    CHECK(lastPulse(0x40, 2) == pulse(12) && lastPulse(0x40, 9) == HOLD0, "T3 trim applied to hook 3 at HOLD");
    server.request("/trim", {{"servo", "2"}, {"value", "0"}});
    server.request("/mode", {{"mode", "0"}});
  }

  // T4 double drop, odd count
  {
    server.request("/mode", {{"mode", "2"}});
    server.request("/sticks", {{"count", "7"}});
    step(1000);
    auto rel = playGame();
    std::map<unsigned long, int> byTime; for (auto& r : rel) byTime[r.at]++;
    std::vector<int> groups; for (auto& kv : byTime) groups.push_back(kv.second);
    CHECK(rel.size() == 7 && groups == std::vector<int>({2, 2, 2, 1}), "T4 double drop pairs 2,2,2,1");
  }

  // T5 speed-up: gaps shrink each round, STOP resets
  {
    server.request("/mode", {{"mode", "1"}});
    server.request("/sticks", {{"count", "4"}});
    server.request("/settings", {{"start", "1000"}, {"min", "1000"}, {"max", "1000"}, {"release", "100"}});
    step(1000);
    auto r1 = playGame(); auto r2 = playGame();
    CHECK(r1.size() == 4 && r1[1].at - r1[0].at - 1100 <= 1, "T5 round 1 gap 100+1000");
    CHECK(r2.size() == 4 && r2[1].at - r2[0].at - 950 <= 1, "T5 round 2 gap 100+850");
    CHECK(server.request("/state").body.find("\"speed\":0.72") != std::string::npos, "T5 speed reported");
    server.request("/stop"); step(1000);
    CHECK(speedLevel == 0 && server.request("/state").body.find("\"speed\":1.00") != std::string::npos, "T5 STOP resets speed");
  }

  // T6 marathon: 2 rounds with a reload pause
  {
    server.request("/mode", {{"mode", "3"}, {"rounds", "2"}, {"pause", "3000"}});
    bool sawReload = false; unsigned long reloadStart = 0, reloadEnd = 0;
    size_t from = g_pwm.size();
    server.request("/start");
    runUntil([&] {
      if (gameState == RELOADING && !sawReload) { sawReload = true; reloadStart = g_millis;
        CHECK(server.request("/status").body.find("RELOAD - NEXT ROUND IN 3s") == 0, "T6 reload status"); }
      if (sawReload && !reloadEnd && gameState == COUNTDOWN) reloadEnd = g_millis;
      return sawReload && gameState == WAITING; }, 120000);
    CHECK(sawReload && reloadEnd - reloadStart == 3000, "T6 3 s reload pause");
    CHECK(releasesSince(from, REL90).size() == 8, "T6 4 sticks x 2 rounds released");
    CHECK(has("Marathon round 2 of 2") && has("MARATHON COMPLETE"), "T6 marathon messages");
    server.request("/mode", {{"mode", "0"}});
  }

  // T7 floor sensors score catches and misses
  {
    g_i2cPresent[0x20] = true; g_mcp[0x20];
    reboot();
    CHECK(has("Floor sensors: 16 lanes (scoring on)"), "T7 sensors found");
    server.request("/sticks", {{"count", "6"}}); step(1000);
    g_mcp[0x20].pins &= ~(1u << 2);        // lane 3 already blocked before the round: no edge
    std::map<int, unsigned long> dropAt;
    size_t from = g_pwm.size(); unsigned long lastDrop = 0, roundEndSeen = 0;
    server.request("/start");
    runUntil([&] {
      for (size_t i = from; i < g_pwm.size(); i++) if (g_pwm[i].off == REL90 && !dropAt.count(g_pwm[i].ch)) { dropAt[g_pwm[i].ch] = g_pwm[i].at; lastDrop = g_pwm[i].at; }
      for (int lane : {0, 1}) if (dropAt.count(lane)) {
        unsigned long dt = g_millis - dropAt[lane];
        if (dt == 300) g_mcp[0x20].pins &= ~(1u << lane);
        if (dt == 400) g_mcp[0x20].pins |= (1u << lane);
      }
      if (!roundEndSeen && gameState == RESETTING) roundEndSeen = g_millis;
      return gameState == WAITING && roundEndSeen; }, 60000);
    CHECK(has("Stick 1 MISSED") && has("Stick 2 MISSED") && !has("Stick 3 MISSED"), "T7 misses from floor hits only");
    CHECK(has("Round result: 4/6 caught"), "T7 round result");
    int lastLane = -1; for (auto& kv : dropAt) if (kv.second == lastDrop) lastLane = kv.first;
    unsigned long needed = lastLane <= 1 ? 500 : MISS_WINDOW_MS;   // a missed last stick is decided at once
    CHECK(roundEndSeen >= lastDrop + needed && roundEndSeen <= lastDrop + MISS_WINDOW_MS + 10, "T7 round waits for undecided sticks only");
    Reply st = server.request("/state"); json("state_sensors", st);
    CHECK(st.body.find("\"lanes\":\"mmcccc\"") != std::string::npos && st.body.find("\"best\":4") != std::string::npos, "T7 lanes + best in /state");
    g_i2cPresent[0x20] = false; g_mcp.erase(0x20);
  }

  // T8 second servo board: up to 32 sticks
  {
    g_i2cPresent[0x41] = true;
    reboot();
    CHECK(server.request("/config").body.find("\"maxSticks\":32") != std::string::npos, "T8 32 sticks available");
    CHECK(server.request("/sticks", {{"count", "20"}}).body == "20", "T8 set 20");
    step(2500);
    bool b2 = true; for (int c = 0; c < 4; c++) b2 &= lastPulse(0x41, c) == HOLD0;
    CHECK(b2 && lastPulse(0x41, 4) == 0, "T8 hooks 17-20 on board 2 channels 0-3");
    server.request("/test", {{"servo", "18"}});
    CHECK(lastPulse(0x41, 2) == REL90, "T8 test stick 19 fires board 2 CH2");
    step(500);
    auto rel = playGame();
    int onB2 = 0; for (auto& r : rel) onB2 += r.addr == 0x41;
    CHECK(rel.size() == 20 && onB2 == 4, "T8 20-stick round uses both boards");
    g_i2cPresent[0x41] = false;
    reboot();
    CHECK(has("WARNING: 20 sticks saved but board 2 (0x41) not found.") && cfg.sticks == 16, "T8 clamps to 16 without board 2");
  }

  // T9 test fire is non-blocking
  {
    unsigned long t = g_millis;
    CHECK(server.request("/test", {{"servo", "1"}}).body == "SERVO TESTED" && g_millis == t, "T9 /test returns at once");
    CHECK(lastPulse(0x40, 1) == pulse(cfg.releaseAngle), "T9 hook released");
    step(cfg.releaseTime + 1);
    CHECK(lastPulse(0x40, 1) == pulse(cfg.holdAngle + cfg.trim[1]), "T9 hook back at HOLD after release time");
    server.request("/start");
    CHECK(server.request("/test", {{"servo", "1"}}).body == "GAME RUNNING", "T9 test refused mid-game");
    server.request("/stop"); step(2000);
  }

  // T10 captive portal, page, update route
  {
    Reply r = server.request("/generate_204");
    CHECK(r.code == 302 && r.location == "http://192.168.4.1/", "T10 unknown URL redirects to panel");
    Reply p = server.request("/");
    CHECK(p.code == 200 && p.type == "text/html" && p.body.find("STICK CATCHER") != std::string::npos, "T10 panel served");
    CHECK(server.request("/update").code == 200, "T10 /update registered");
  }

  // T11 restore defaults
  {
    server.request("/trim", {{"servo", "0"}, {"value", "-20"}});
    CHECK(server.request("/defaults").body == "DEFAULTS RESTORED", "T11 defaults");
    CHECK(cfg.sticks == 6 && cfg.trim[0] == 0 && cfg.startDelay == 5000 && cfg.mode == 0, "T11 values reset");
  }

  // T12 START button debounce
  {
    g_pinIn[14] = LOW; step(20); g_pinIn[14] = HIGH; step(50);
    CHECK(gameState == WAITING, "T12 20 ms bounce ignored");
    g_pinIn[14] = LOW; step(40); g_pinIn[14] = HIGH; step(5);
    CHECK(gameState == COUNTDOWN, "T12 real press starts");
    server.request("/stop"); step(1000);
  }

  // T13 buzzer and LED
  {
    g_tones.clear();
    server.request("/settings", {{"start", "3000"}});
    server.request("/start");
    int ledOn = 0, ledOff = 0;
    runUntil([&] { if (gameState == COUNTDOWN) (g_pinOut[13] ? ledOn : ledOff)++; return gameState == BETWEEN_STICKS; }, 20000);
    CHECK(ledOn > 500 && ledOff > 500, "T13 LED blinks during countdown");
    CHECK(g_pinOut[13] == HIGH, "T13 LED on during round");
    int ticks = 0, go = 0; for (auto& t : g_tones) { ticks += t.freq == 660; go += t.freq == 1040; }
    CHECK(ticks == 3 && go == 1, "T13 three countdown beeps and GO");
    runUntil([] { return gameState == WAITING; }, 60000); step(10);
    CHECK(g_pinOut[13] == LOW, "T13 LED off when idle");
    server.request("/sound", {{"on", "0"}});
    g_tones.clear(); playGame();
    CHECK(g_tones.empty(), "T13 sound off = silent");
    server.request("/sound", {{"on", "1"}});
  }

  // T14 START right after changing the count waits for the servo reset
  {
    server.request("/settings", {{"start", "1000"}});
    server.request("/sticks", {{"count", "16"}});
    unsigned long t0 = g_millis;
    auto rel = playGame();
    CHECK(rel.size() == 16 && rel[0].at - t0 >= 16 * RESET_STEP_MS, "T14 first drop waits for all 16 hooks to reset");
  }

  // T15 settings from an older layout are ignored
  {
    g_flash.assign(g_flash.size(), 0xAB);
    reboot();
    CHECK(has("No saved settings, using defaults.") && cfg.sticks == 6, "T15 corrupt flash -> defaults");
  }

  printf("RESULT %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
