// SPDX-License-Identifier: MIT
#pragma once
#include "Dispatcher.h"
namespace cts {
// Board adapter must provide a coherent snapshot, not unrelated racy reads.
struct TimeObservation {
  uint64_t utcMs = 0, monotonicMs = 0, syncAgeMs = 0, uncertaintyMs = 0;
  bool synchronized = false, ageKnown = false, uncertaintyKnown = false;
};
class TimeSource {
public:
  virtual ~TimeSource() = default;
  virtual TimeObservation observe() noexcept = 0;
};
struct ClockLimits {
  uint64_t maxSyncAgeMs = 300000, maxUncertaintyMs = 250, maxStepMs = 50;
  bool requireKnownUncertainty = true;
};
enum class ClockState {
  Uninitialized,
  Ready,
  Unsynchronized,
  InvalidUtc,
  UnknownAge,
  Stale,
  UnknownUncertainty,
  Uncertain,
  MonotonicRollback,
  UtcRollback,
  Discontinuity
};
struct ClockHealth {
  ClockState state = ClockState::Uninitialized;
  uint64_t observations = 0, ready = 0, rejected = 0, faults = 0;
  uint64_t syncAgeMs = 0, uncertaintyMs = 0;
  bool ageKnown = false, uncertaintyKnown = false, latched = false;
};
// Conservative observer, not an NTP client or estimator. Fault recovery
// requires a new monitor AND a new measurement/command boot session; no silent
// clamp.
class MonitoredClock final : public ClockProvider {
public:
  explicit MonitoredClock(TimeSource &source,
                          const ClockLimits &limits = ClockLimits())
      : source_(source), limits_(limits) {}
  MonitoredClock(const MonitoredClock &) = delete;
  MonitoredClock &operator=(const MonitoredClock &) = delete;
  ClockReading read() noexcept override;
  const ClockHealth &health() const { return health_; }

private:
  TimeSource &source_;
  ClockLimits limits_;
  ClockHealth health_;
  uint64_t lastMono_ = 0, trustedMono_ = 0, trustedUtc_ = 0;
  bool seenMono_ = false, seenTrusted_ = false;
};
} // namespace cts
