// SPDX-License-Identifier: MIT
// Unity tests for the host-native codec core. Synthetic inputs only; no
// sensor, board, broker or Genesis host is involved.
#include <unity.h>

#include <cmath>
#include <cstring>
#include <limits>

#include "CodonTraceSwarm.h"

using namespace cts;

static char out[TxBytes];
static const uint64_t utc = 1800000000000ULL;

static ChannelSpec scalarChannel(const char* name) {
  ChannelSpec c;
  c.name = name;
  c.quantity = "synthetic_signal";
  c.unit = "count";
  c.descriptor.type = ValueType::Scalar;
  c.descriptor.featureId = "fixture";
  c.descriptor.configId = "fixture_v1";
  c.descriptor.sourceUnit = "count";
  return c;
}

static SessionConfig session(const char* boot) {
  SessionConfig s;
  s.swarmId = "synthetic";
  s.nodeId = "native";
  s.bootId = boot;
  return s;
}

void setUp() { std::memset(out, 0, sizeof out); }
void tearDown() {}

static void test_hello_requires_begin() {
  Device d;
  TEST_ASSERT_TRUE(d.hello(out, sizeof out) == Error::NotStarted);
}

static void test_hello_is_genesis_swarm_v2() {
  Device d;
  TEST_ASSERT_TRUE(d.define(scalarChannel("s")) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(session("boot_a")) == Error::Ok);
  TEST_ASSERT_TRUE(d.hello(out, sizeof out) == Error::Ok);
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"schema\":\"genesis_swarm_v2\""));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"kind\":\"hello\""));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"wifi\":true"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"operations\":[]"));
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(d.nextSequence()));
}

static void test_define_after_begin_is_frozen() {
  Device d;
  TEST_ASSERT_TRUE(d.define(scalarChannel("s")) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(session("boot_a")) == Error::Ok);
  TEST_ASSERT_TRUE(d.define(scalarChannel("late")) == Error::Frozen);
}

static void test_zero_and_invalid_readings() {
  Device d;
  TEST_ASSERT_TRUE(d.define(scalarChannel("s")) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(session("boot_a")) == Error::Ok);
  TEST_ASSERT_TRUE(d.hello(out, sizeof out) == Error::Ok);
  TEST_ASSERT_TRUE(d.reading("s", Sample::scalar(0, utc), out, sizeof out) == Error::Ok);
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"value\":0"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"valid\":true"));
  Sample bad = Sample::invalid("sensor_timeout", utc);
  TEST_ASSERT_TRUE(d.reading("s", bad, out, sizeof out) == Error::Ok);
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"value\":null"));
  TEST_ASSERT_NOT_NULL(std::strstr(out, "\"valid\":false"));
}

static void test_non_finite_values_are_rejected() {
  Device d;
  TEST_ASSERT_TRUE(d.define(scalarChannel("s")) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(session("boot_a")) == Error::Ok);
  TEST_ASSERT_TRUE(d.hello(out, sizeof out) == Error::Ok);
  Sample s = Sample::scalar(std::numeric_limits<double>::quiet_NaN(), utc);
  TEST_ASSERT_TRUE(d.reading("s", s, out, sizeof out) == Error::InvalidSample);
  s.value = std::numeric_limits<double>::infinity();
  TEST_ASSERT_TRUE(d.reading("s", s, out, sizeof out) == Error::InvalidSample);
}

static void test_small_buffer_fails_without_consuming_sequence() {
  Device d;
  TEST_ASSERT_TRUE(d.define(scalarChannel("s")) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(session("boot_a")) == Error::Ok);
  const auto before = d.nextSequence();
  char tiny[8];
  std::memset(tiny, 'x', sizeof tiny);
  TEST_ASSERT_TRUE(d.hello(tiny, sizeof tiny) == Error::Capacity);
  TEST_ASSERT_EQUAL_CHAR(0, tiny[0]);
  TEST_ASSERT_TRUE(d.nextSequence() == before);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hello_requires_begin);
  RUN_TEST(test_hello_is_genesis_swarm_v2);
  RUN_TEST(test_define_after_begin_is_frozen);
  RUN_TEST(test_zero_and_invalid_readings);
  RUN_TEST(test_non_finite_values_are_rejected);
  RUN_TEST(test_small_buffer_fails_without_consuming_sequence);
  return UNITY_END();
}
