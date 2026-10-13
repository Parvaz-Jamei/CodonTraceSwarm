// Explicitly SYNTHETIC native fixture; not a board adapter or measured sensor.
#pragma once
#include "cts/CommandSession.h"
#include <string>
namespace fixture {
constexpr uint64_t T = 1800000000000ULL;
struct Clock : cts::ClockProvider {
  cts::ClockReading time;
  Clock() {
    time.utcMs = T;
    time.monotonicMs = 100;
    time.synchronized = true;
  }
  cts::ClockReading read() noexcept override { return time; }
};
inline cts::ChannelSpec channel(const char *name = "s") {
  cts::ChannelSpec s;
  s.name = name;
  s.quantity = "temperature";
  s.unit = "degC";
  s.descriptor.sourceUnit = "degC";
  s.descriptor.featureId = "synthetic_probe";
  s.descriptor.configId = "config1";
  s.descriptor.hasRate = true;
  s.descriptor.minimumHz = 1;
  s.descriptor.maximumHz = 10;
  return s;
}
inline cts::SessionConfig config() {
  cts::SessionConfig c;
  c.swarmId = "lab";
  c.nodeId = "node";
  c.bootId = "boot";
  return c;
}
inline cts::DeliveryContext delivery() {
  cts::DeliveryContext d;
  d.authenticated = true;
  d.retained = false;
  return d;
}
inline std::string command(int id = 1,
                           const char *params = "{\"channel\":\"s\"}",
                           const char *op = "sample") {
  return "{\"schema\":\"genesis_swarm_command_v1\",\"swarm_id\":\"lab\",\"node_"
         "id\":\"node\",\"boot_id\":\"boot\",\"command_id\":\"c" +
         std::to_string(id) + "\",\"operation\":\"" + op +
         "\",\"params\":" + params +
         ",\"created_ms\":1800000000000,\"expires_ms\":1800000005000,"
         "\"status\":\"pending\"}";
}
struct Executor : cts::CommandExecutor {
  int calls = 0;
  bool succeed = true;
  bool execute(const cts::Command &) noexcept override {
    ++calls;
    return succeed;
  }
};
inline cts::SessionResult submit(cts::CommandSession &s,
                                 const std::string &wire) {
  return s.submit(wire.data(), wire.size(), delivery());
}
} // namespace fixture
