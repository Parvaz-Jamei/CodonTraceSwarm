// Decode-only synthetic host interoperability fixture. No command execution.
#include "cts/Command.h"
#include <iomanip>
#include <iostream>
#include <string>
int main() {
  static cts::CommandDecoder decoder;
  cts::CommandContext context;
  context.swarmId = "synthetic";
  context.nodeId = "native";
  context.bootId = "boot_a";
  context.nowMs = 1800000000000ULL;
  std::string line;
  while (std::getline(std::cin, line)) {
    cts::Command c;
    auto e = decoder.decode(line.data(), line.size(), context, c);
    std::cout << std::setprecision(17) << "{\"error\":\""
              << cts::commandErrorName(e) << "\",\"id\":\"" << c.id
              << "\",\"operation\":\"" << cts::operationName(c.operation)
              << "\",\"channel\":\"" << c.channel << "\",\"hz\":" << c.hz
              << ",\"alpha\":" << c.alpha << ",\"clip\":" << c.clip
              << ",\"event_threshold\":" << c.eventThreshold
              << ",\"source_digest\":\"" << c.sourceDigest << "\"}\n";
  }
}
