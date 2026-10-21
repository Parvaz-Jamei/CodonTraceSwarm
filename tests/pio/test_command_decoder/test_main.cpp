// SPDX-License-Identifier: MIT
// Unity suite: CommandDecoder (genesis_swarm_command_v1 decode only).
#include <unity.h>
#include <cstring>
#include <string>
#include "cts/Command.h"
#include "session_fixture.h"
using namespace cts;
static CommandDecoder dec;
static CommandContext ctx() {
  CommandContext c; c.swarmId = "lab"; c.nodeId = "node"; c.bootId = "boot";
  c.nowMs = fixture::T; return c;
}
void setUp() {}
void tearDown() {}
static CommandError run(const std::string &w, Command &c) {
  return dec.decode(w.data(), w.size(), ctx(), c);
}
static void test_valid_sample_command() {
  Command c;
  TEST_ASSERT_TRUE(run(fixture::command(7), c) == CommandError::Ok);
  TEST_ASSERT_EQUAL_STRING("c7", c.id);
  TEST_ASSERT_EQUAL_STRING("s", c.channel);
  TEST_ASSERT_TRUE(c.operation == Operation::Sample);
  TEST_ASSERT_TRUE(c.createdMs == fixture::T);
}
static void test_wrong_schema_rejected() {
  Command c; std::string w = fixture::command();
  w.replace(w.find("command_v1"), 10, "command_v9");
  TEST_ASSERT_TRUE(run(w, c) == CommandError::Schema);
}
static void test_other_node_is_scope_error() {
  Command c; std::string w = fixture::command();
  w.replace(w.find("\"node\""), 6, "\"nodx\"");
  TEST_ASSERT_TRUE(run(w, c) == CommandError::Scope);
}
static void test_duplicate_key_rejected() {
  Command c; std::string w = fixture::command();
  w.insert(1, "\"status\":\"pending\",");
  TEST_ASSERT_TRUE(run(w, c) == CommandError::DuplicateKey);
}
static void test_malformed_and_oversize() {
  Command c;
  TEST_ASSERT_TRUE(run("{\"schema\":", c) == CommandError::MalformedJson);
  std::string big(CommandBytes + 1, ' ');
  TEST_ASSERT_TRUE(run(big, c) == CommandError::TooLarge);
}
static void test_expired_rejected() {
  Command c; CommandContext x = ctx(); x.nowMs = fixture::T + 10000;
  const std::string w = fixture::command();
  TEST_ASSERT_TRUE(dec.decode(w.data(), w.size(), x, c) == CommandError::Expired);
}
// Same depth-64 case that tools/verify_golden.py sends to the Genesis host.
static void test_nesting_depth_64_is_capacity() {
  Command c;
  std::string deep;
  for (int i = 0; i < 63; ++i) deep += "[";
  deep += "0";
  for (int i = 0; i < 63; ++i) deep += "]";
  const std::string w = fixture::command(1, ("{\"channel\":\"s\",\"x\":" + deep + "}").c_str());
  TEST_ASSERT_TRUE(run(w, c) == CommandError::Capacity);
  TEST_ASSERT_EQUAL_STRING("", c.id);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_sample_command);
  RUN_TEST(test_wrong_schema_rejected);
  RUN_TEST(test_other_node_is_scope_error);
  RUN_TEST(test_duplicate_key_rejected);
  RUN_TEST(test_malformed_and_oversize);
  RUN_TEST(test_expired_rejected);
  RUN_TEST(test_nesting_depth_64_is_capacity);
  return UNITY_END();
}
