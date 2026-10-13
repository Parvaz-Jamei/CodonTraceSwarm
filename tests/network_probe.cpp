// SYNTHETIC host round-trip harness. No physical sensor, MQTT or network.
#include "cts/NetworkInbox.h"
#include "session_fixture.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
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
SessionResult deliver(NetworkInbox &inbox, CommandSession &session,
                      const std::string &line) {
  static uint64_t token = 0;
  ++token;
  for (size_t offset = 0; offset < line.size();) {
    const size_t length = (line.size() - offset) > 7 ? 7 : line.size() - offset;
    ReceiveFragment f;
    f.data = line.data() + offset;
    f.length = length;
    f.total = line.size();
    f.offset = offset;
    f.connection = 1;
    f.token = token;
    f.authenticated = true;
    f.retainKnown = true;
    f.retained = false;
    if (!offset) {
      f.topic = inbox.topic();
      f.topicLength = std::strlen(f.topic);
    }
    const auto code = inbox.push(f);
    offset += length;
    if (code !=
        (offset == line.size() ? InboxCode::Enqueued : InboxCode::Partial))
      std::exit(20);
  }
  const auto r = inbox.pollOne(session);
  if (r.code != InboxCode::Ok)
    std::exit(21);
  return r.session;
}
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
  static NetworkInbox inbox;
  if (inbox.bind("lab", "node") != InboxCode::Ok ||
      inbox.connected(1) != InboxCode::Ok)
    return 22;
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
    if (deliver(inbox, session, line).dispatch.code != DispatchCode::Queued)
      return 2;
    if (deliver(inbox, session, line).dispatch.code != DispatchCode::Duplicate)
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
    if (deliver(inbox, session, line).dispatch.code != DispatchCode::Duplicate)
      return 6;
    std::cout << "{\"reply\":" << saved << ",\"reading\":"
              << (executor.reading[0] ? executor.reading : "null")
              << ",\"calls\":" << executor.calls
              << ",\"retry_identical\":true,\"duplicate_suppressed\":true}"
              << std::endl;
    if (session.confirmReplyPublished(sequence) != SessionCode::Ok)
      return 7;
    if (deliver(inbox, session, line).dispatch.code !=
            DispatchCode::Duplicate ||
        session.replySize() != 0)
      return 8;
  }
}
