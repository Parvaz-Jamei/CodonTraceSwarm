// SYNTHETIC clock/acquisition fixture; never a real sensor measurement.
#include "cts/TimedTelemetry.h"
#include "session_fixture.h"
#include <iostream>
using namespace cts;
struct Source : TimeSource {
  TimeObservation v;
  Source() {
    v.utcMs = fixture::T;
    v.monotonicMs = 100;
    v.synchronized = v.ageKnown = v.uncertaintyKnown = true;
    v.uncertaintyMs = 5;
  }
  TimeObservation observe() noexcept override { return v; }
  void advance(uint64_t n) {
    v.utcMs += n;
    v.monotonicMs += n;
    v.syncAgeMs += n;
  }
};
int main() {
  static Device d;
  Source source;
  MonitoredClock clock(source);
  TimedTelemetry telemetry(d, clock, 1000);
  static char out[TxBytes];
  AcquisitionStamp stamp;
  if (d.define(fixture::channel()) != Error::Ok ||
      d.begin(fixture::config()) != Error::Ok ||
      d.hello(out, sizeof(out)) != Error::Ok)
    return 1;
  std::cout << out << "\n";
  if (telemetry.capture(stamp) != TelemetryCode::Ok)
    return 2;
  auto sample = Sample::scalar(0, stamp.utcMs());
  source.advance(100);
  if (telemetry.reading("s", sample, stamp, out, sizeof(out)).code !=
      TelemetryCode::Ok)
    return 3;
  std::cout << out << "\n";
  source.advance(1000);
  const auto seq = d.nextSequence();
  if (telemetry.reading("s", sample, stamp, out, sizeof(out)).code !=
          TelemetryCode::Stale ||
      d.nextSequence() != seq)
    return 4;
  if (telemetry.capture(stamp) != TelemetryCode::Ok)
    return 5;
  auto invalid = Sample::invalid("sensor_timeout", stamp.utcMs());
  if (telemetry.reading("s", invalid, stamp, out, sizeof(out)).code !=
      TelemetryCode::Ok)
    return 6;
  std::cout << out << "\n";
  std::cerr << "SYNTHETIC: acquisition timestamp preserved; stale rejected="
            << telemetry.health().stale
            << "; encoded=" << telemetry.health().encoded << "\n";
}
