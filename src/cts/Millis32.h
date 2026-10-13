// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace cts {
// Single-owner adapter for an unsigned 32-bit millisecond counter.
// Call at intervals strictly below 2^31 ms. It cannot detect a full missed wrap
// or distinguish a reboot from wraparound; construct anew for every boot.
// This provides elapsed monotonic time only, NEVER UTC or synchronization.
class Millis32 {
public:
  bool update(uint32_t tick) noexcept {
    if (fault_) return false;
    if (!started_) { last_ = tick; started_ = true; return true; }
    const uint32_t delta = static_cast<uint32_t>(tick - last_);
    if (delta >= UINT32_C(0x80000000) || UINT64_MAX - elapsed_ < delta) {
      fault_ = true;
      return false;
    }
    elapsed_ += delta;
    last_ = tick;
    return true;
  }
  uint64_t elapsedMs() const noexcept { return elapsed_; }
  bool ready() const noexcept { return started_ && !fault_; }
  bool faulted() const noexcept { return fault_; }
private:
  uint64_t elapsed_ = 0;
  uint32_t last_ = 0;
  bool started_ = false, fault_ = false;
};
}
