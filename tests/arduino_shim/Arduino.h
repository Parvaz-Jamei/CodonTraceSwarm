// SPDX-License-Identifier: MIT
// TEST ONLY: minimal native facade, not an Arduino core implementation.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
using byte = uint8_t;
class String {
  std::string value_;
public:
  String() = default;
  String(const char* s): value_(s ? s : "") {}
  size_t length() const { return value_.size(); }
  const char* c_str() const { return value_.c_str(); }
  bool reserve(size_t n) { value_.reserve(n); return true; }
  String& operator+=(char c) { value_ += c; return *this; }
};
extern unsigned long testArduinoMillis;
extern void (*testArduinoYieldHook)();
inline unsigned long millis() { return testArduinoMillis++; }
inline void yield() { ++testArduinoMillis; if(testArduinoYieldHook) testArduinoYieldHook(); }
