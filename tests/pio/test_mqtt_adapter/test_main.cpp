// SPDX-License-Identifier: MIT
// Unity suite: ArduinoMqttAdapter topic/payload framing with a fake client.
// The fake models only the ArduinoMqttClient calls the adapter uses.
#include <unity.h>
#include <cstring>
#include <string>
#include "cts/ArduinoMqttAdapter.h"
#include "session_fixture.h"
using namespace cts;
struct FakeString {
  std::string s;
  size_t length() const { return s.size(); }
  const char *c_str() const { return s.c_str(); }
};
struct FakeMqtt {
  bool online = true; void (*cb)(int) = nullptr;
  std::string inTopic, inBody, outTopic, outBody; size_t readPos = 0;
  int inQos = 1, inRetain = 0; bool pending = false; bool outRetain = true; int outQos = -1;
  bool connected() const { return online; }
  void onMessage(void (*f)(int)) { cb = f; }
  void stop() { online = false; }
  void poll() { if (pending && cb) { pending = false; readPos = 0; cb((int)inBody.size()); } }
  int messageRetain() const { return inRetain; }
  int messageQoS() const { return inQos; }
  FakeString messageTopic() const { return FakeString{inTopic}; }
  int read(uint8_t *b, size_t n) {
    if (readPos + n > inBody.size()) return -1;
    std::memcpy(b, inBody.data() + readPos, n); readPos += n; return (int)n;
  }
  int beginMessage(const char *t, unsigned long, bool retain, uint8_t qos, bool) {
    outTopic = t; outRetain = retain; outQos = qos; outBody.clear(); return 1;
  }
  size_t write(const uint8_t *b, size_t n) { outBody.append((const char *)b, n); return n; }
  int endMessage() { return 1; }
};
struct Rig {
  Device d; fixture::Clock c; fixture::Executor e; CommandSession s; NetworkInbox in; FakeMqtt m;
  Rig() : s(d, c, e) {
    d.define(fixture::channel()); d.allow("s", Operation::Sample);
    s.begin(fixture::config()); in.bind("lab", "node");
  }
};
void setUp() {}
void tearDown() {}
static void test_attach_requires_attestation() {
  Rig r; ArduinoMqttAdapter<FakeMqtt, 1> a(r.m, r.in);
  TEST_ASSERT_FALSE(a.attach(1, false));
}
static void test_command_in_reply_out_on_up_topic_qos1_not_retained() {
  Rig r; ArduinoMqttAdapter<FakeMqtt, 2> a(r.m, r.in);
  TEST_ASSERT_TRUE(a.attach(1, true));
  r.m.inTopic = "genesis/swarm/lab/node/down"; r.m.inBody = fixture::command(); r.m.pending = true;
  TEST_ASSERT_TRUE(a.receive(1) == ArduinoMqttCode::Enqueued);
  TEST_ASSERT_TRUE(r.in.pollOne(r.s).code == InboxCode::Ok);
  r.s.poll();
  TEST_ASSERT_TRUE(a.publishReply(r.s) == ArduinoMqttCode::Published);
  TEST_ASSERT_EQUAL_STRING("genesis/swarm/lab/node/up", r.m.outTopic.c_str());
  TEST_ASSERT_EQUAL_INT(1, r.m.outQos);
  TEST_ASSERT_FALSE(r.m.outRetain);
  TEST_ASSERT_NOT_NULL(std::strstr(r.m.outBody.c_str(), "\"command_id\":\"c1\""));
}
static void test_retained_or_qos0_or_wrong_topic_closes() {
  for (int k = 0; k < 3; ++k) {
    Rig r; ArduinoMqttAdapter<FakeMqtt, 3> a(r.m, r.in);
    TEST_ASSERT_TRUE(a.attach(1, true));
    r.m.inTopic = k == 2 ? "genesis/swarm/lab/other/down" : "genesis/swarm/lab/node/down";
    r.m.inRetain = k == 0; r.m.inQos = k == 1 ? 0 : 1;
    r.m.inBody = fixture::command(); r.m.pending = true;
    TEST_ASSERT_TRUE(a.receive(1) == ArduinoMqttCode::Rejected);
    TEST_ASSERT_FALSE(r.m.online);
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_attach_requires_attestation);
  RUN_TEST(test_command_in_reply_out_on_up_topic_qos1_not_retained);
  RUN_TEST(test_retained_or_qos0_or_wrong_topic_closes);
  return UNITY_END();
}
