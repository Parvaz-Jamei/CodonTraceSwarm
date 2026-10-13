// SPDX-License-Identifier: MIT
// Independent adversarial checkpoint05 tests. Synthetic clock only.
#include "cts/TimedTelemetry.h"
#include "session_fixture.h"
#include <cstring>
#include <iostream>
#include <limits>
using namespace cts;
int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << __LINE__ << ": " << #x << "\n";                             \
    }                                                                          \
  } while (0)
struct Source : TimeSource {
  TimeObservation o;
  Source() {
    o.utcMs = fixture::T;
    o.monotonicMs = 100;
    o.synchronized = o.ageKnown = o.uncertaintyKnown = true;
  }
  TimeObservation observe() noexcept override { return o; }
  void advance(uint64_t n) {
    o.utcMs += n;
    o.monotonicMs += n;
  }
};
int main() {
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.o.synchronized = false;
    s.o.monotonicMs = 99;
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::MonotonicRollback);
    s.o.synchronized = true;
    s.advance(10);
    CHECK(!c.read().synchronized);
    CHECK(c.health().faults == 1);
    CHECK(c.health().observations == 3);
  }
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.o.ageKnown = false;
    s.advance(100);
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::UnknownAge);
    s.o.ageKnown = true;
    s.advance(100);
    CHECK(c.read().synchronized);
    CHECK(!c.health().latched);
  }
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.o.synchronized = false;
    s.advance(100);
    CHECK(!c.read().synchronized);
    s.o.synchronized = true;
    s.o.utcMs += 51;
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::Discontinuity);
  }
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.o.utcMs += 50;
    CHECK(c.read().synchronized);
    s.o.utcMs += 51;
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::Discontinuity);
  }
  {
    Source s;
    ClockLimits l;
    l.requireKnownUncertainty = false;
    MonitoredClock c(s, l);
    s.o.uncertaintyKnown = false;
    CHECK(c.read().synchronized);
    s.o.uncertaintyKnown = true;
    s.o.uncertaintyMs = 251;
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::Uncertain);
    s.o.uncertaintyMs = 250;
    CHECK(c.read().synchronized);
  }
  {
    Source s;
    s.o.monotonicMs = std::numeric_limits<uint64_t>::max() - 10;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.advance(10);
    CHECK(c.read().synchronized);
    s.o.monotonicMs = 0;
    CHECK(!c.read().synchronized);
    CHECK(c.health().latched);
  }
  {
    Source s;
    s.o.utcMs = MaxWireInteger;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    ++s.o.utcMs;
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::InvalidUtc);
  }
  {
    Device d;
    Source s;
    MonitoredClock c(s);
    TimedTelemetry t(d, c, 10), other(d, c, 10);
    AcquisitionStamp a;
    char out[TxBytes];
    CHECK(t.capture(a) == TelemetryCode::NotStarted);
    CHECK(d.define(fixture::channel()) == Error::Ok);
    CHECK(d.begin(fixture::config()) == Error::Ok);
    CHECK(t.capture(a) == TelemetryCode::Ok);
    auto sample = Sample::scalar(0, a.utcMs());
    auto seq = d.nextSequence();
    CHECK(other.reading("s", sample, a, out, sizeof out).code ==
          TelemetryCode::InvalidStamp);
    CHECK(d.nextSequence() == seq);
    sample.utcMs++;
    CHECK(t.reading("s", sample, a, out, sizeof out).code ==
          TelemetryCode::TimestampMismatch);
    sample.utcMs--;
    s.advance(10);
    CHECK(t.reading("s", sample, a, out, sizeof out).code == TelemetryCode::Ok);
    CHECK(std::strstr(out, "\"value\":0"));
    s.advance(1);
    seq = d.nextSequence();
    CHECK(t.reading("s", sample, a, out, sizeof out).code ==
          TelemetryCode::Stale);
    CHECK(d.nextSequence() == seq);
    CHECK(out[0] == 0);
    CHECK(t.capture(a) == TelemetryCode::Ok);
    sample = Sample::invalid("driver_timeout", a.utcMs());
    CHECK(t.reading("s", sample, a, out, sizeof out).code == TelemetryCode::Ok);
    CHECK(std::strstr(out, "\"value\":null"));
    d.end();
    auto cfg = fixture::config();
    cfg.bootId = "boot2";
    CHECK(d.begin(cfg) == Error::Ok);
    CHECK(t.reading("s", sample, a, out, sizeof out).code ==
          TelemetryCode::WrongSession);
    s.o.synchronized = false;
    CHECK(t.capture(a) == TelemetryCode::ClockUnavailable);
    CHECK(a.utcMs() == 0);
    s.o.synchronized = true;
    sample = Sample::scalar(1, s.o.utcMs);
    CHECK(t.reading("s", sample, a, out, sizeof out).code ==
          TelemetryCode::InvalidStamp);
  }
  std::cout << "independent_clock_checks=" << checks << " failures=" << failures
            << "\n";
  return failures ? 1 : 0;
}
