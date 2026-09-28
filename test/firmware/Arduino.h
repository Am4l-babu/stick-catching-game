// Minimal host-side stand-ins for the Arduino / ESP8266 APIs the sketch uses.
// Just enough to run the game logic on a PC with a fake clock.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <random>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define PROGMEM
typedef const char* PGM_P;
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

template <class T> T min(T a, T b) { return a < b ? a : b; }
template <class T> T max(T a, T b) { return a > b ? a : b; }

// ---------- fake time ----------
extern unsigned long g_millis;
inline unsigned long millis() { return g_millis; }
inline unsigned long micros() { return g_millis * 1000 + 17; }
inline void delay(unsigned long ms) { g_millis += ms; }

// ---------- random ----------
extern std::mt19937 g_rng;
inline void randomSeed(unsigned long s) { g_rng.seed(s); }
inline long random(long lo, long hi) {
  if (hi <= lo) return lo;
  return lo + (long)(g_rng() % (unsigned long)(hi - lo));
}
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }

// ---------- GPIO ----------
extern int g_pinMode[64];
extern int g_pinOut[64];
extern int g_pinIn[64];
struct ToneEvent { unsigned long at; int pin; unsigned freq; unsigned long ms; };
extern std::vector<ToneEvent> g_tones;
inline void pinMode(int p, int m) { g_pinMode[p] = m; }
inline void digitalWrite(int p, int v) { g_pinOut[p] = v; }
inline int digitalRead(int p) { return g_pinIn[p]; }
inline void tone(int pin, unsigned f, unsigned long ms = 0) { g_tones.push_back({g_millis, pin, f, ms}); }
inline void noTone(int) {}

// ---------- String ----------
class String {
public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  String(char c) : s(1, c) {}
  String(unsigned char v) : s(std::to_string(v)) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(double v, int decimals = 2) { char b[64]; snprintf(b, sizeof b, "%.*f", decimals, v); s = b; }
  String(float v, int decimals = 2) : String((double)v, decimals) {}
  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  long toInt() const { return atol(s.c_str()); }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  bool operator==(const char* o) const { return s == o; }
  friend String operator+(const String& a, const String& b) { return String(a.s + b.s); }
  friend String operator+(const String& a, const char* b) { return String(a.s + b); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
};

// ---------- IPAddress ----------
struct IPAddress {
  uint8_t b[4];
  IPAddress(int a = 0, int c = 0, int d = 0, int e = 0) { b[0] = a; b[1] = c; b[2] = d; b[3] = e; }
  String toString() const { char t[20]; snprintf(t, sizeof t, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]); return String(t); }
};

// ---------- Serial ----------
#define HEX 16
class FakeSerial {
public:
  std::string out;
  bool echo = false;
  void begin(long) {}
  void w(const std::string& t) { out += t; if (echo) fputs(t.c_str(), stdout); }
  void print(const char* t) { w(t); }
  void print(const String& t) { w(t.s); }
  void print(char c) { w(std::string(1, c)); }
  void print(int v, int base = 10) { w(base == 16 ? fmtHex(v) : std::to_string(v)); }
  void print(unsigned v) { w(std::to_string(v)); }
  void print(long v) { w(std::to_string(v)); }
  void print(unsigned long v) { w(std::to_string(v)); }
  void print(unsigned char v, int base = 10) { print((int)v, base); }
  void print(double v, int d = 2) { w(String(v, d).s); }
  void print(const IPAddress& ip) { w(ip.toString().s); }
  template <class T> void println(const T& v) { print(v); w("\n"); }
  void println(double v, int d) { print(v, d); w("\n"); }
  void println(int v, int base) { print(v, base); w("\n"); }
  void println() { w("\n"); }
  int available() { return 0; }
  int read() { return -1; }
  static std::string fmtHex(int v) { char b[16]; snprintf(b, sizeof b, "%X", v); return b; }
};
extern FakeSerial Serial;
