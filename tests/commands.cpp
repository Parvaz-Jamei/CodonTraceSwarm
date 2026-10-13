#include "cts/Command.h"
#include <cstring>
#include <iostream>
#include <type_traits>
static_assert(!std::is_copy_constructible<cts::Device>::value,
              "Session codec must not copy");
#include <limits>
#include <string>
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
static CommandDecoder parser;
CommandContext context() {
  CommandContext c;
  c.swarmId = "synthetic";
  c.nodeId = "native";
  c.bootId = "boot_a";
  c.nowMs = 1800000000000ULL;
  return c;
}
std::string message(std::string op = "sample",
                    std::string params = "{\"channel\":\"s\"}") {
  return "{\"schema\":\"genesis_swarm_command_v1\",\"swarm_id\":\"synthetic\","
         "\"node_id\":\"native\",\"boot_id\":\"boot_a\",\"command_id\":\"cmd-"
         "1\",\"operation\":\"" +
         op + "\",\"params\":" + params +
         ",\"created_ms\":1800000000000,\"expires_ms\":1800000005000,"
         "\"status\":\"pending\"}";
}
std::string replace(std::string s, const std::string &a, const std::string &b) {
  auto p = s.find(a);
  if (p == std::string::npos)
    std::abort();
  s.replace(p, a.size(), b);
  return s;
}
CommandError decode(const std::string &s, Command &out) {
  return parser.decode(s.data(), s.size(), context(), out);
}
void reject(const std::string &s, CommandError expected) {
  Command out;
  std::strcpy(out.id, "stale_result");
  auto e = decode(s, out);
  CHECK(e == expected);
  CHECK(out.id[0] == 0);
}
int main() {
  Command c;
  auto base = message();
  CHECK(decode(base, c) == CommandError::Ok);
  CHECK(!std::strcmp(c.id, "cmd-1"));
  CHECK(c.createdMs == 1800000000000ULL);
  CHECK(c.expiresMs == 1800000005000ULL);
  Command owned = c;
  for (const char *op : {"sample", "raw", "features", "result"}) {
    CHECK(decode(message(op), c) == CommandError::Ok);
    CHECK(!std::strcmp(operationName(c.operation), op));
  }
  CHECK(decode(message("set_rate", "{\"channel\":\"s\",\"hz\":2.5}"), c) ==
        CommandError::Ok);
  CHECK(c.hz == 2.5);
  for (const char *op : {"pipeline", "capsule"}) {
    CHECK(decode(
              message(op,
                      "{\"channel\":\"s\",\"pipeline\":{\"alpha\":0.5,\"clip\":"
                      "2,\"event_threshold\":4},\"source_digest\":\"" +
                          std::string(64, 'a') + "\"}"),
              c) == CommandError::Ok);
    CHECK(c.alpha == .5 && c.clip == 2 && c.eventThreshold == 4 &&
          std::strlen(c.sourceDigest) == 64);
  }
  CHECK(!std::strcmp(owned.id, "cmd-1"));
  CHECK(decode(replace(base, "\"command_id\"", "\"command_\\u0069d\""), c) ==
        CommandError::Ok);
  CHECK(decode(replace(base, "cmd-1", "cmd-\\u0031"), c) == CommandError::Ok);
  CHECK(!std::strcmp(c.id, "cmd-1"));
  reject(replace(base, "\"params\":", "\"command_id\":\"cmd-2\",\"params\":"),
         CommandError::DuplicateKey);
  reject(replace(base,
                 "\"params\":", "\"command_\\u0069d\":\"cmd-2\",\"params\":"),
         CommandError::DuplicateKey);
  reject(message("sample", "{\"channel\":\"s\",\"chann\\u0065l\":\"s\"}"),
         CommandError::DuplicateKey);
  reject(message("capsule", "{\"channel\":\"s\",\"pipeline\":{\"alpha\":0.5,"
                            "\"alpha\":0.2,\"clip\":2,\"event_threshold\":4}}"),
         CommandError::DuplicateKey);
  reject(replace(base, "\"params\":", "\"extra\":0,\"params\":"),
         CommandError::Schema);
  reject(message("sample", "{\"channel\":\"s\",\"extra\":0}"),
         CommandError::InvalidParameters);
  reject(replace(base, "boot_a", "boot_b"), CommandError::Scope);
  reject(replace(base, "synthetic", "other"), CommandError::Scope);
  reject(replace(base, "native", "other"), CommandError::Scope);
  reject(replace(base, "pending", "acknowledged"), CommandError::Schema);
  reject(
      replace(base, "created_ms\":1800000000000", "created_ms\":1800000000001"),
      CommandError::Future);
  reject(replace(replace(base, "created_ms\":1800000000000",
                         "created_ms\":1799999999999"),
                 "expires_ms\":1800000005000", "expires_ms\":1800000000000"),
         CommandError::Expired);
  reject(
      replace(base, "expires_ms\":1800000005000", "expires_ms\":1800000060001"),
      CommandError::InvalidParameters);
  reject(replace(base, "created_ms\":1800000000000", "created_ms\":true"),
         CommandError::Schema);
  reject(replace(base, "created_ms\":1800000000000",
                 "created_ms\":1800000000000.0"),
         CommandError::Schema);
  reject(replace(base, "expires_ms\":1800000005000",
                 "expires_ms\":9007199254740992"),
         CommandError::Schema);
  reject(message("execute_code"), CommandError::Unsupported);
  for (const char *v : {"0", "10001", "true", "null", "\"1\"", "1e999"})
    reject(message("set_rate",
                   std::string("{\"channel\":\"s\",\"hz\":") + v + "}"),
           CommandError::InvalidParameters);
  for (const char *v : {"01", "+1", "1.", "1e", "NaN", "Infinity"})
    reject(message("set_rate",
                   std::string("{\"channel\":\"s\",\"hz\":") + v + "}"),
           CommandError::MalformedJson);
  for (const char *bad :
       {"", "{", "[]x", "{\"a\":1,}", "[1,]", "{\"a\":\"\\uD800\"}",
        "{\"a\":\"\\uDC00\"}", "{\"a\":\"\\u0000\"}", "{\"a\":\"\\z\"}",
        "{\"a\":\"\xc0\xaf\"}", "{\"a\":\"\xed\xa0\x80\"}"})
    reject(bad, CommandError::MalformedJson);
  // Valid paired surrogate/raw Unicode decode, but IDs must still be ASCII
  // identifiers.
  reject(replace(base, "cmd-1", "\\uD83D\\uDE00"), CommandError::Schema);
  reject(replace(base, "cmd-1", "µ"), CommandError::Schema);
  reject(base + "x", CommandError::MalformedJson);
  reject(std::string(4097, ' '), CommandError::TooLarge);
  auto nul = base;
  nul[2] = 0;
  reject(nul, CommandError::MalformedJson);
  reject("[[[[[[[[[[0]]]]]]]]]]", CommandError::Capacity);
  std::string many = "[";
  for (int i = 0; i < 100; ++i) {
    if (i)
      many += ",";
    many += "0";
  }
  many += "]";
  reject(many, CommandError::Capacity);
  reject(replace(base, "\"params\":", "\"µ\":0,\"\\u00b5\":0,\"params\":"),
         CommandError::DuplicateKey);
  for (size_t prefix = 0; prefix < base.size(); ++prefix)
    CHECK(decode(base.substr(0, prefix), c) != CommandError::Ok);
  CHECK(decode(base + std::string(CommandBytes - base.size(), ' '), c) ==
        CommandError::Ok);
  reject(message("capsule",
                 "{\"channel\":\"s\",\"pipeline\":{\"alpha\":0.5,\"clip\":2,"
                 "\"event_threshold\":4},\"source_digest\":\"" +
                     std::string(64, 'A') + "\"}"),
         CommandError::InvalidParameters);
  CHECK(decode(base, c) == CommandError::Ok);
  auto badctx = context();
  badctx.nowMs = 0;
  CHECK(parser.decode(base.data(), base.size(), badctx, c) ==
        CommandError::InvalidContext);
  CHECK(c.id[0] == 0);
  CHECK(parser.decode(nullptr, 10, context(), c) ==
        CommandError::MalformedJson);
  ChannelSpec channel;
  channel.name = "s";
  channel.descriptor.hasRate = true;
  channel.descriptor.minimumHz = 1;
  channel.descriptor.maximumHz = 10;
  CHECK(decode(message("set_rate", "{\"channel\":\"s\",\"hz\":2}"), c) ==
        CommandError::Ok);
  CHECK(validateChannel(c, channel) == CommandError::Ok);
  channel.descriptor.maximumHz = 1;
  CHECK(validateChannel(c, channel) == CommandError::InvalidParameters);
  CHECK(decode(message("pipeline", "{\"channel\":\"s\",\"pipeline\":{\"alpha\":"
                                   "1,\"clip\":2,\"event_threshold\":3}}"),
               c) == CommandError::Ok);
  channel.descriptor.type = ValueType::Vector;
  CHECK(validateChannel(c, channel) == CommandError::InvalidParameters);
  // Deterministic fuzz tests memory/parser behavior only, not simulation
  // results.
  uint32_t state = 0x43545332;
  char bytes[512];
  for (int i = 0; i < 20000; ++i) {
    state = state * 1664525u + 1013904223u;
    size_t n = state % sizeof(bytes);
    for (size_t j = 0; j < n; ++j) {
      state = state * 1664525u + 1013904223u;
      bytes[j] = char(state >> 24);
    }
    auto e = parser.decode(bytes, n, context(), c);
    if (e != CommandError::Ok && c.id[0]) {
      ++failures;
      break;
    }
  }
  for (size_t i = 0; i < base.size(); ++i) {
    auto mutated = base;
    mutated[i] = '\0';
    CHECK(decode(mutated, c) != CommandError::Ok);
  }
  CHECK(decode(base, c) == CommandError::Ok);
  std::cout << "command_checks=" << checks << " failures=" << failures
            << " fuzz_inputs=20000 sizeof_decoder=" << sizeof(CommandDecoder)
            << "\n";
  return failures ? 1 : 0;
}
