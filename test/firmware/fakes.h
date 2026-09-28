// Fake I2C bus (PCA9685 + MCP23017 devices), EEPROM, web server, Wi-Fi, OTA.
#pragma once
#include "Arduino.h"

// ---------- I2C ----------
struct FakeMcp { uint8_t reg[32] = {0}; uint16_t pins = 0xFFFF; };   // pins: 1 = HIGH (idle)
extern std::map<uint8_t, bool> g_i2cPresent;
extern std::map<uint8_t, FakeMcp> g_mcp;

class TwoWire {
public:
  uint8_t addr = 0; std::vector<uint8_t> tx; std::vector<uint8_t> rx; size_t rxPos = 0;
  void begin(int, int) {}
  void beginTransmission(uint8_t a) { addr = a; tx.clear(); }
  size_t write(uint8_t v) { tx.push_back(v); return 1; }
  uint8_t endTransmission() {
    if (!g_i2cPresent[addr]) return 2;
    if (g_mcp.count(addr)) {
      FakeMcp& m = g_mcp[addr];
      if (tx.size() >= 2) m.reg[tx[0]] = tx[1];
      if (!tx.empty()) m.reg[31] = tx[0];            // remember register pointer
    }
    return 0;
  }
  int requestFrom(int a, int n) {
    rx.clear(); rxPos = 0;
    if (!g_i2cPresent[a] || !g_mcp.count(a)) return 0;
    FakeMcp& m = g_mcp[a];
    if (m.reg[31] == 0x12) { rx.push_back(m.pins & 0xFF); rx.push_back(m.pins >> 8); }
    return (int)min<size_t>(rx.size(), (size_t)n);
  }
  int read() { return rxPos < rx.size() ? rx[rxPos++] : -1; }
};
extern TwoWire Wire;

// ---------- PCA9685 ----------
struct PwmEvent { unsigned long at; uint8_t addr; uint8_t ch; uint16_t off; };
extern std::vector<PwmEvent> g_pwm;
class Adafruit_PWMServoDriver {
public:
  uint8_t a;
  Adafruit_PWMServoDriver(uint8_t addr = 0x40) : a(addr) {}
  void begin() {}
  void setOscillatorFrequency(uint32_t) {}
  void setPWMFreq(float) {}
  void setPWM(uint8_t ch, uint16_t, uint16_t off) { g_pwm.push_back({g_millis, a, ch, off}); }
};

// ---------- EEPROM (keeps its bytes across a simulated reboot) ----------
extern std::vector<uint8_t> g_flash;
extern int g_flashWrites;
class EEPROMClass {
public:
  std::vector<uint8_t> data; bool dirty = false;
  void begin(size_t n) { if (g_flash.size() < n) g_flash.resize(n, 0xFF); data = g_flash; data.resize(n); dirty = false; }
  template <class T> T& get(int a, T& t) { memcpy(&t, data.data() + a, sizeof(T)); return t; }
  template <class T> const T& put(int a, const T& t) {
    if (memcmp(data.data() + a, &t, sizeof(T)) != 0) { dirty = true; memcpy(data.data() + a, &t, sizeof(T)); }
    return t;
  }
  bool commit() { if (dirty) { g_flash = data; g_flashWrites++; dirty = false; } return true; }
};
extern EEPROMClass EEPROM;

// ---------- Wi-Fi / DNS / OTA ----------
#define WIFI_AP 2
struct FakeWiFi {
  void mode(int) {}
  void softAPConfig(IPAddress, IPAddress, IPAddress) {}
  void softAP(const char*, const char*) {}
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
};
extern FakeWiFi WiFi;

class DNSServer { public: void start(int, const char*, IPAddress) {} void processNextRequest() {} };

struct FakeOTA {
  std::function<void()> startCb;
  void setHostname(const char*) {}
  void setPassword(const char*) {}
  void onStart(std::function<void()> f) { startCb = f; }
  void begin() {}
  void handle() {}
};
extern FakeOTA ArduinoOTA;

// ---------- Web server ----------
struct Reply { int code = 0; std::string type, body, location; };
class ESP8266WebServer {
public:
  std::map<std::string, std::function<void()>> routes;
  std::function<void()> notFound;
  std::map<std::string, std::string> args;
  Reply last; std::string pendingLocation;
  ESP8266WebServer(int) {}
  void on(const char* p, std::function<void()> f) { routes[p] = f; }
  void onNotFound(std::function<void()> f) { notFound = f; }
  void begin() {}
  void handleClient() {}
  bool hasArg(const char* k) { return args.count(k) > 0; }
  String arg(const char* k) { return String(args[k]); }
  void sendHeader(const char* k, const char* v, bool) { if (std::string(k) == "Location") pendingLocation = v; }
  void send(int c, const char* t, const String& b) { last.code = c; last.type = t; last.body = b.s; last.location = pendingLocation; pendingLocation.clear(); }
  void send_P(int c, PGM_P t, PGM_P b) { send(c, t, String(b)); }
  // test helper
  Reply request(const std::string& path, std::map<std::string, std::string> a = {}) {
    args = a; last = Reply();
    if (routes.count(path)) routes[path](); else if (notFound) notFound();
    return last;
  }
};
class ESP8266HTTPUpdateServer {
public:
  template <class S> void setup(S* s, const char* path, const char*, const char*) { s->on(path, [s]() { s->send(200, "text/html", "update page"); }); }
};
