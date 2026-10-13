// SPDX-License-Identifier: MIT
#include "Clock.h"
#include <limits>
namespace cts {
namespace {
void bump(uint64_t &v) {
  if (v < std::numeric_limits<uint64_t>::max())
    ++v;
}
} // namespace
ClockReading MonitoredClock::read() noexcept {
  const auto o = source_.observe();
  bump(health_.observations);
  health_.syncAgeMs = o.syncAgeMs;
  health_.uncertaintyMs = o.uncertaintyMs;
  health_.ageKnown = o.ageKnown;
  health_.uncertaintyKnown = o.uncertaintyKnown;
  ClockReading r;
  r.monotonicMs = o.monotonicMs;
  auto state = health_.state;
  if (!health_.latched) {
    state = ClockState::Ready;
    if (seenMono_ && o.monotonicMs < lastMono_)
      state = ClockState::MonotonicRollback;
    else if (!o.synchronized)
      state = ClockState::Unsynchronized;
    else if (!o.utcMs || o.utcMs > MaxWireInteger)
      state = ClockState::InvalidUtc;
    else if (!o.ageKnown)
      state = ClockState::UnknownAge;
    else if (o.syncAgeMs > limits_.maxSyncAgeMs)
      state = ClockState::Stale;
    else if (!o.uncertaintyKnown && limits_.requireKnownUncertainty)
      state = ClockState::UnknownUncertainty;
    else if (o.uncertaintyKnown && o.uncertaintyMs > limits_.maxUncertaintyMs)
      state = ClockState::Uncertain;
    // Compare against last trusted mapping, including across sync outages.
    if (state == ClockState::Ready && seenTrusted_) {
      if (o.utcMs < trustedUtc_)
        state = ClockState::UtcRollback;
      else {
        const auto du = o.utcMs - trustedUtc_,
                   dm = o.monotonicMs - trustedMono_;
        const auto discrepancy = du > dm ? du - dm : dm - du;
        if (discrepancy > limits_.maxStepMs)
          state = ClockState::Discontinuity;
      }
    }
    lastMono_ = o.monotonicMs;
    seenMono_ = true;
    if (state == ClockState::MonotonicRollback ||
        state == ClockState::UtcRollback ||
        state == ClockState::Discontinuity) {
      health_.latched = true;
      bump(health_.faults);
    }
    health_.state = state;
  }
  if (state == ClockState::Ready) {
    r.utcMs = o.utcMs;
    r.synchronized = true;
    trustedUtc_ = o.utcMs;
    trustedMono_ = o.monotonicMs;
    seenTrusted_ = true;
    bump(health_.ready);
  } else
    bump(health_.rejected);
  return r;
}
} // namespace cts
