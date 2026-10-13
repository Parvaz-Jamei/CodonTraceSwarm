#include "cts/TimedTelemetry.h"
#include "session_fixture.h"
#include <cstring>
#include <iostream>
#include <limits>
using namespace cts;
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
struct Source : TimeSource {
  TimeObservation value;
  Source() {
    value.utcMs = fixture::T;
    value.monotonicMs = 100;
    value.synchronized = true;
    value.ageKnown = true;
    value.uncertaintyKnown = true;
    value.uncertaintyMs = 5;
  }
  TimeObservation observe() noexcept override { return value; }
  void advance(uint64_t ms) {
    value.utcMs += ms;
    value.monotonicMs += ms;
    value.syncAgeMs += ms;
  }
};
int main() {
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.health().state == ClockState::Uninitialized);
    CHECK(c.read().synchronized);
    s.advance(100);
    CHECK(c.read().utcMs == fixture::T + 100);
    CHECK(c.health().observations == 2 && c.health().ready == 2 &&
          c.health().rejected == 0);
    CHECK(c.health().uncertaintyKnown && c.health().uncertaintyMs == 5);
  }
  for (int type = 0; type < 6; ++type) {
    Source s;
    MonitoredClock c(s);
    switch (type) {
    case 0:
      s.value.synchronized = false;
      break;
    case 1:
      s.value.utcMs = 0;
      break;
    case 2:
      s.value.ageKnown = false;
      break;
    case 3:
      s.value.syncAgeMs = 300001;
      break;
    case 4:
      s.value.uncertaintyKnown = false;
      break;
    case 5:
      s.value.uncertaintyMs = 251;
      break;
    }
    const ClockState expected[] = {
        ClockState::Unsynchronized,     ClockState::InvalidUtc,
        ClockState::UnknownAge,         ClockState::Stale,
        ClockState::UnknownUncertainty, ClockState::Uncertain};
    auto r = c.read();
    CHECK(!r.synchronized && r.utcMs == 0);
    CHECK(c.health().state == expected[type] && !c.health().latched);
    Source valid;
    s.value = valid.value;
    CHECK(c.read().synchronized);
    CHECK(c.health().rejected == 1 && c.health().ready == 1);
  }
  {
    Source s;
    s.value.utcMs = MaxWireInteger + 1;
    MonitoredClock c(s);
    CHECK(!c.read().synchronized);
    CHECK(c.health().state == ClockState::InvalidUtc);
  }
  {
    Source s;
    ClockLimits l;
    l.requireKnownUncertainty = false;
    MonitoredClock c(s, l);
    s.value.uncertaintyKnown = false;
    CHECK(c.read().synchronized);
    CHECK(!c.health().uncertaintyKnown);
    s.value.uncertaintyKnown = true;
    s.value.uncertaintyMs = 251;
    CHECK(!c.read().synchronized);
  }
  {
    Source s;
    s.value.syncAgeMs = 300000;
    s.value.uncertaintyMs = 250;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
  }
  for (int type = 0; type < 4; ++type) {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    if (type == 0)
      --s.value.monotonicMs;
    if (type == 1)
      --s.value.utcMs;
    if (type == 2)
      s.value.utcMs += 51;
    if (type == 3) {
      --s.value.monotonicMs;
      s.value.synchronized = false;
    }
    CHECK(!c.read().synchronized && c.health().latched);
    Source restored;
    s.value = restored.value;
    s.advance(1000);
    CHECK(!c.read().synchronized);
    CHECK(c.health().faults == 1);
    CHECK(c.health().ready == 1 && c.health().rejected == 2);
  }
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.value.utcMs += 50;
    CHECK(c.read().synchronized);
  }
  {
    Source s;
    MonitoredClock c(s);
    CHECK(c.read().synchronized);
    s.value.synchronized = false;
    s.advance(100);
    CHECK(!c.read().synchronized);
    s.value.synchronized = true;
    s.advance(100);
    s.value.utcMs += 1000;
    CHECK(!c.read().synchronized &&
          c.health().state == ClockState::Discontinuity);
  }
  // Counter/state progression under many consistent readings (not a hardware
  // soak).
  {
    Source s;
    MonitoredClock c(s);
    for (int i = 0; i < 1000; ++i) {
      s.advance(1);
      CHECK(c.read().synchronized);
    }
    CHECK(c.health().ready == 1000 && c.health().faults == 0);
  }
  {
    Source s;
    MonitoredClock c(s);
    Device d;
    TimedTelemetry t(d, c, 100);
    AcquisitionStamp stamp;
    char out[TxBytes];
    CHECK(t.capture(stamp) == TelemetryCode::NotStarted);
    CHECK(d.define(fixture::channel()) == Error::Ok);
    CHECK(d.begin(fixture::config()) == Error::Ok);
    CHECK(d.activeSessionNumber() == 1);
    CHECK(t.capture(stamp) == TelemetryCode::Ok);
    CHECK(stamp.utcMs() == fixture::T && stamp.monotonicMs() == 100);
    auto sample = Sample::scalar(0, stamp.utcMs());
    s.advance(80);
    CHECK(t.reading("s", sample, stamp, out, sizeof(out)).code ==
          TelemetryCode::Ok);
    CHECK(std::strstr(out, "\"sample_ms\":1800000000000"));
    CHECK(std::strstr(out, "\"value\":0"));
    const auto seq = d.nextSequence();
    sample.utcMs = s.value.utcMs;
    CHECK(t.reading("s", sample, stamp, out, sizeof(out)).code ==
          TelemetryCode::TimestampMismatch);
    CHECK(d.nextSequence() == seq && out[0] == 0);
    sample.utcMs = stamp.utcMs();
    s.advance(20);
    CHECK(t.reading("s", sample, stamp, out, sizeof(out)).code ==
          TelemetryCode::Ok);
    s.advance(1);
    CHECK(t.reading("s", sample, stamp, out, sizeof(out)).code ==
          TelemetryCode::Stale);
    CHECK(t.health().stale == 1 && t.health().encoded == 2 &&
          t.health().invalid == 2);
    CHECK(t.capture(stamp) == TelemetryCode::Ok);
    auto invalid = Sample::invalid("sensor_timeout", stamp.utcMs());
    CHECK(t.reading("s", invalid, stamp, out, sizeof(out)).code ==
          TelemetryCode::Ok);
    CHECK(std::strstr(out, "\"value\":null"));
    s.value.synchronized = false;
    CHECK(t.reading("s", invalid, stamp, out, sizeof(out)).code ==
          TelemetryCode::ClockUnavailable);
    CHECK(t.capture(stamp) == TelemetryCode::ClockUnavailable &&
          stamp.utcMs() == 0);
    CHECK(t.health().clockRejected == 2);
  }
  {
    Source s;
    MonitoredClock c(s);
    Device d;
    CHECK(d.define(fixture::channel()) == Error::Ok);
    CHECK(d.begin(fixture::config()) == Error::Ok);
    TimedTelemetry t(d, c), other(d, c);
    AcquisitionStamp stamp;
    CHECK(t.capture(stamp) == TelemetryCode::Ok);
    char out[TxBytes];
    auto sample = Sample::scalar(1, stamp.utcMs());
    CHECK(other.reading("s", sample, stamp, out, sizeof(out)).code ==
          TelemetryCode::InvalidStamp);
    CHECK(t.reading("s", sample, AcquisitionStamp{}, out, sizeof(out)).code ==
          TelemetryCode::InvalidStamp);
    CHECK(t.reading("s", sample, stamp, out, 1).code ==
          TelemetryCode::CodecError);
    CHECK(t.health().codecRejected == 1);
    d.end();
    CHECK(d.activeSessionNumber() == 0);
    auto cfg = fixture::config();
    cfg.bootId = "boot2";
    CHECK(d.begin(cfg) == Error::Ok && d.activeSessionNumber() == 2);
    CHECK(t.reading("s", sample, stamp, out, sizeof(out)).code ==
          TelemetryCode::WrongSession);
  }
  // Same monitored clock gates actual command sessions; no mock dispatcher.
  {
    Source s;
    MonitoredClock clock(s);
    Device d;
    fixture::Executor exec;
    CommandSession session(d, clock, exec);
    CHECK(d.define(fixture::channel()) == Error::Ok);
    CHECK(d.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(fixture::submit(session, fixture::command()).dispatch.code ==
          DispatchCode::Queued);
    s.value.syncAgeMs = 300001;
    CHECK(session.poll().dispatch.code == DispatchCode::ClockUnavailable &&
          exec.calls == 0);
    s.value.syncAgeMs = 0;
    CHECK(session.poll().dispatch.state == CommandState::Succeeded &&
          exec.calls == 1);
    AcquisitionStamp stamp;
    TimedTelemetry t(d, clock);
    CHECK(t.capture(stamp) == TelemetryCode::Ok);
    char out[TxBytes];
    auto r = t.reading("s", Sample::scalar(1, stamp.utcMs()), stamp, out,
                       sizeof(out));
    CHECK(r.code == TelemetryCode::CodecError && r.codecError == Error::Busy);
    CHECK(session.confirmReplyPublished(session.replySequence()) ==
          SessionCode::Ok);
    CHECK(t.reading("s", Sample::scalar(1, stamp.utcMs()), stamp, out,
                    sizeof(out))
              .code == TelemetryCode::Ok);
  }
  std::cout << "clock checks=" << checks << " failures=" << failures
            << " sizeof(MonitoredClock)=" << sizeof(MonitoredClock)
            << " sizeof(TimedTelemetry)=" << sizeof(TimedTelemetry) << "\n";
  return failures ? 1 : 0;
}
