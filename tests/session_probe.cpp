// SYNTHETIC host round-trip harness. No physical sensor, MQTT or network.
#include "session_fixture.h"
#include <cstring>
#include <iostream>
using namespace cts;
using namespace fixture;
struct Sensor : CommandExecutor {
  Device &device;
  int calls = 0;
  bool fail = false;
  char reading[TxBytes] = {};
  explicit Sensor(Device &d) : device(d) {}
  bool execute(const Command &c) noexcept override {
    ++calls;
    reading[0] = 0;
    if (fail)
      return false;
    // Explicitly synthetic, not a measured temperature.
    return device.reading(c.channel, Sample::scalar(42.5, T), reading,
                          sizeof(reading)) == Error::Ok;
  }
};
int main(int argc, char **) {
  static Device device;
  Clock clock;
  Sensor executor(device);
  CommandSession session(device, clock, executor);
  static char hello[TxBytes];
  if (device.define(channel()) != Error::Ok ||
      device.allow("s", Operation::Sample) != Error::Ok ||
      session.begin(config()).code != SessionCode::Ok ||
      device.hello(hello, sizeof(hello)) != Error::Ok)
    return 1;
  std::cout << hello << std::endl;
  if (argc > 1)
    return 0;
  std::string line;
  bool expire = false;
  while (std::getline(std::cin, line)) {
    if (line == "@fail") {
      executor.fail = true;
      continue;
    }
    if (line == "@success") {
      executor.fail = false;
      continue;
    }
    if (line == "@expire") {
      expire = true;
      continue;
    }
    executor.reading[0] = 0;
    if (submit(session, line).dispatch.code != DispatchCode::Queued)
      return 2;
    if (submit(session, line).dispatch.code != DispatchCode::Duplicate)
      return 3;
    if (expire) {
      clock.time.monotonicMs += 5000;
      expire = false;
    }
    auto r = session.poll();
    if (r.code != SessionCode::ReplyPending)
      return 4;
    const std::string saved = session.replyData();
    const auto sequence = session.replySequence();
    if (session.poll().code != SessionCode::ReplyPending ||
        saved != session.replyData() || sequence != session.replySequence())
      return 5;
    if (submit(session, line).dispatch.code != DispatchCode::Duplicate)
      return 6;
    std::cout << "{\"reply\":" << saved << ",\"reading\":"
              << (executor.reading[0] ? executor.reading : "null")
              << ",\"calls\":" << executor.calls
              << ",\"retry_identical\":true,\"duplicate_suppressed\":true}"
              << std::endl;
    if (session.confirmReplyPublished(sequence) != SessionCode::Ok)
      return 7;
    if (submit(session, line).dispatch.code != DispatchCode::Duplicate ||
        session.replySize() != 0)
      return 8;
  }
}
