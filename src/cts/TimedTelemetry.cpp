// SPDX-License-Identifier: MIT
#include "TimedTelemetry.h"
#include <limits>
namespace cts {
namespace {
void bump(uint64_t &v) {
  if (v < std::numeric_limits<uint64_t>::max())
    ++v;
}
} // namespace
TelemetryCode TimedTelemetry::capture(AcquisitionStamp &stamp) {
  stamp = AcquisitionStamp{};
  if (!device_.activeSessionNumber()) {
    bump(health_.invalid);
    return TelemetryCode::NotStarted;
  }
  const auto now = clock_.read();
  if (!now.synchronized) {
    bump(health_.clockRejected);
    return TelemetryCode::ClockUnavailable;
  }
  stamp.owner_ = this;
  stamp.session_ = device_.activeSessionNumber();
  stamp.utc_ = now.utcMs;
  stamp.mono_ = now.monotonicMs;
  bump(health_.captured);
  return TelemetryCode::Ok;
}
TelemetryResult TimedTelemetry::reading(const char *channel,
                                        const Sample &sample,
                                        const AcquisitionStamp &stamp,
                                        char *out, size_t cap) {
  if (out && cap)
    out[0] = 0;
  TelemetryResult r;
  if (!device_.activeSessionNumber())
    r.code = TelemetryCode::NotStarted;
  else if (stamp.owner_ != this || !stamp.session_ || !stamp.utc_)
    r.code = TelemetryCode::InvalidStamp;
  else if (stamp.session_ != device_.activeSessionNumber())
    r.code = TelemetryCode::WrongSession;
  else if (sample.utcMs != stamp.utc_)
    r.code = TelemetryCode::TimestampMismatch;
  if (r.code != TelemetryCode::Ok) {
    bump(health_.invalid);
    return r;
  }
  const auto now = clock_.read();
  if (!now.synchronized) {
    r.code = TelemetryCode::ClockUnavailable;
    bump(health_.clockRejected);
    return r;
  }
  if (now.monotonicMs < stamp.mono_ || now.utcMs < stamp.utc_) {
    r.code = TelemetryCode::InvalidStamp;
    bump(health_.invalid);
    return r;
  }
  if (now.monotonicMs - stamp.mono_ > maxAgeMs_ ||
      now.utcMs - stamp.utc_ > maxAgeMs_) {
    r.code = TelemetryCode::Stale;
    bump(health_.stale);
    return r;
  }
  r.codecError = device_.reading(channel, sample, out, cap);
  if (r.codecError != Error::Ok) {
    r.code = TelemetryCode::CodecError;
    bump(health_.codecRejected);
  } else
    bump(health_.encoded);
  return r;
}
} // namespace cts
