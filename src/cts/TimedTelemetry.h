// SPDX-License-Identifier: MIT
#pragma once
#include "Clock.h"
namespace cts {
class TimedTelemetry;
class AcquisitionStamp {
public:
  uint64_t utcMs() const { return utc_; }
  uint64_t monotonicMs() const { return mono_; }

private:
  friend class TimedTelemetry;
  const TimedTelemetry *owner_ = nullptr;
  uint64_t session_ = 0, utc_ = 0, mono_ = 0;
};
enum class TelemetryCode {
  Ok,
  NotStarted,
  ClockUnavailable,
  InvalidStamp,
  WrongSession,
  Stale,
  TimestampMismatch,
  CodecError
};
struct TelemetryResult {
  TelemetryCode code = TelemetryCode::Ok;
  Error codecError = Error::Ok;
};
struct TelemetryHealth {
  uint64_t captured = 0, encoded = 0, clockRejected = 0, stale = 0, invalid = 0,
           codecRejected = 0;
};
// Use one event loop. Capture at acquisition (window START for waveforms), not
// when draining a queue. Does not read sensors, schedule acquisition or
// publish.
class TimedTelemetry {
public:
  TimedTelemetry(Device &device, MonitoredClock &clock,
                 uint64_t maxAgeMs = 3000)
      : device_(device), clock_(clock), maxAgeMs_(maxAgeMs) {}
  TimedTelemetry(const TimedTelemetry &) = delete;
  TimedTelemetry &operator=(const TimedTelemetry &) = delete;
  TelemetryCode capture(AcquisitionStamp &);
  TelemetryResult reading(const char *channel, const Sample &,
                          const AcquisitionStamp &, char *, size_t);
  const TelemetryHealth &health() const { return health_; }

private:
  Device &device_;
  MonitoredClock &clock_;
  uint64_t maxAgeMs_;
  TelemetryHealth health_;
};
} // namespace cts
