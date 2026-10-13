// SYNTHETIC desktop MQTT laboratory node; no physical sensor or board SDK.
#include "cts/NetworkInbox.h"
#include "session_fixture.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace cts;
struct Sensor : CommandExecutor {
  Device &device;
  int calls = 0;
  char reading[TxBytes] = {};
  bool fail = false;
  explicit Sensor(Device &d) : device(d) {}
  bool execute(const Command &c) noexcept override {
    ++calls;
    reading[0] = 0;
    return !fail && device.reading(c.channel, Sample::scalar(42.5, fixture::T),
                                   reading, sizeof(reading)) == Error::Ok;
  }
};
int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  static Device device;
  fixture::Clock clock;
  Sensor sensor(device);
  CommandSession session(device, clock, sensor);
  static NetworkInbox inbox;
  auto cfg = fixture::config();
  cfg.nodeId = argv[1];
  static char wire[TxBytes];
  if (device.define(fixture::channel()) != Error::Ok ||
      device.allow("s", Operation::Sample) != Error::Ok ||
      session.begin(cfg).code != SessionCode::Ok ||
      inbox.bind("lab", argv[1]) != InboxCode::Ok ||
      inbox.connected(1) != InboxCode::Ok)
    return 2;
  if (device.hello(wire, sizeof(wire)) != Error::Ok)
    return 3;
  std::cout << wire << std::endl;
  uint64_t token = 0, generation = 1;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "@hello") {
      if (inbox.connected(++generation) != InboxCode::Ok ||
          device.hello(wire, sizeof(wire)) != Error::Ok)
        return 4;
      std::cout << wire << std::endl;
      continue;
    }
    if (line == "@fail") {
      sensor.fail = true;
      std::cout << "{\"configured\":true}" << std::endl;
      continue;
    }
    if (line.compare(0, 11, "@published:") == 0) {
      auto seq = std::strtoull(line.c_str() + 11, nullptr, 10);
      std::cout << "{\"confirmed\":"
                << (session.confirmReplyPublished(seq) == SessionCode::Ok
                        ? "true"
                        : "false")
                << "}" << std::endl;
      continue;
    }
    const auto first = line.find('\t'),
               second =
                   line.find('\t', first == std::string::npos ? 0 : first + 1);
    if (first == std::string::npos || second == std::string::npos)
      return 5;
    const auto topic = line.substr(0, first),
               retain = line.substr(first + 1, second - first - 1),
               data = line.substr(second + 1);
    if (data.empty() || data.size() > CommandBytes)
      return 6;
    ++token;
    InboxCode code = InboxCode::Partial;
    for (size_t offset = 0; offset < data.size();) {
      const auto count =
          (data.size() - offset) > 17 ? 17 : data.size() - offset;
      ReceiveFragment f;
      f.connection = generation;
      f.token = token;
      f.data = data.data() + offset;
      f.length = count;
      f.total = data.size();
      f.offset = offset;
      f.authenticated = true;
      f.retainKnown = true;
      f.retained = retain == "1";
      if (!offset) {
        f.topic = topic.data();
        f.topicLength = topic.size();
      }
      code = inbox.push(f);
      offset += count;
      if (code != InboxCode::Partial && code != InboxCode::Enqueued)
        break;
    }
    if (code != InboxCode::Enqueued) {
      std::cout << "{\"ingress_rejected\":true,\"calls\":" << sensor.calls
                << "}" << std::endl;
      continue;
    }
    auto admitted = inbox.pollOne(session);
    if (admitted.code != InboxCode::Ok) {
      std::cout << "{\"admission_rejected\":true,\"calls\":" << sensor.calls
                << "}" << std::endl;
      continue;
    }
    if (admitted.session.dispatch.code == DispatchCode::Duplicate) {
      std::cout << "{\"duplicate\":true,\"calls\":" << sensor.calls << "}"
                << std::endl;
      continue;
    }
    sensor.reading[0] = 0;
    if (session.poll().code != SessionCode::ReplyPending)
      return 7;
    std::cout << "{\"reading\":"
              << (sensor.reading[0] ? sensor.reading : "null")
              << ",\"reply\":" << session.replyData()
              << ",\"calls\":" << sensor.calls << "}" << std::endl;
  }
}
