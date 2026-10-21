// SPDX-License-Identifier: MIT
// Unity suite: NetworkInbox binding, topic bounds and fragment checks.
#include <unity.h>
#include <cstring>
#include <string>
#include "cts/NetworkInbox.h"
#include "session_fixture.h"
using namespace cts;
static NetworkInbox *inbox;
void setUp() { inbox = new NetworkInbox(); }
void tearDown() { delete inbox; }
static ReceiveFragment frag(const std::string &s) {
  ReceiveFragment f; f.topic = inbox->topic(); f.topicLength = std::strlen(f.topic);
  f.data = s.data(); f.length = s.size(); f.total = s.size(); f.connection = 1;
  f.token = 1; f.authenticated = true; f.retainKnown = true; f.retained = false;
  return f;
}
static void test_topic_is_down_route() {
  TEST_ASSERT_TRUE(inbox->bind("lab", "node") == InboxCode::Ok);
  TEST_ASSERT_EQUAL_STRING("genesis/swarm/lab/node/down", inbox->topic());
}
static void test_max_length_ids_fit_topic_buffer() {
  const std::string id63(63, 'a');
  TEST_ASSERT_TRUE(inbox->bind(id63.c_str(), id63.c_str()) == InboxCode::Ok);
  const std::string want = "genesis/swarm/" + id63 + "/" + id63 + "/down";
  TEST_ASSERT_EQUAL_STRING(want.c_str(), inbox->topic());
}
static void test_overlong_or_empty_ids_rejected() {
  const std::string id64(64, 'a');
  TEST_ASSERT_TRUE(inbox->bind(id64.c_str(), "node") == InboxCode::InvalidScope);
  TEST_ASSERT_TRUE(inbox->bind("lab", id64.c_str()) == InboxCode::InvalidScope);
  TEST_ASSERT_TRUE(inbox->bind("", "node") == InboxCode::InvalidScope);
  TEST_ASSERT_TRUE(inbox->bind("lab", nullptr) == InboxCode::InvalidScope);
}
static void test_untrusted_retained_wrongtopic() {
  inbox->bind("lab", "node"); inbox->connected(1);
  const std::string w = fixture::command();
  ReceiveFragment f = frag(w); f.authenticated = false;
  TEST_ASSERT_TRUE(inbox->push(f) == InboxCode::Untrusted);
  inbox->connected(2);
  f = frag(w); f.connection = 2; f.retained = true;
  TEST_ASSERT_TRUE(inbox->push(f) == InboxCode::Retained);
}
static void test_enqueue_until_full() {
  inbox->bind("lab", "node"); inbox->connected(1);
  const std::string w = fixture::command();
  for (size_t i = 0; i < NetworkInbox::Capacity; ++i) {
    ReceiveFragment f = frag(w); f.token = i + 1;
    TEST_ASSERT_TRUE(inbox->push(f) == InboxCode::Enqueued);
  }
  ReceiveFragment f = frag(w); f.token = 99;
  TEST_ASSERT_TRUE(inbox->push(f) == InboxCode::Full);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_topic_is_down_route);
  RUN_TEST(test_max_length_ids_fit_topic_buffer);
  RUN_TEST(test_overlong_or_empty_ids_rejected);
  RUN_TEST(test_untrusted_retained_wrongtopic);
  RUN_TEST(test_enqueue_until_full);
  return UNITY_END();
}
