// SPDX-License-Identifier: MIT
// Unity suite: CommandSession lifecycle (submit -> execute -> reply -> confirm).
#include <unity.h>
#include <cstring>
#include "session_fixture.h"
using namespace cts;
struct Rig {
  Device d; fixture::Clock c; fixture::Executor e; CommandSession s;
  Rig() : s(d, c, e) {
    d.define(fixture::channel()); d.allow("s", Operation::Sample);
  }
};
void setUp() {}
void tearDown() {}
static void test_submit_before_begin_fails() {
  Rig r;
  TEST_ASSERT_TRUE(fixture::submit(r.s, fixture::command()).code == SessionCode::NotStarted);
}
static void test_success_reply_and_confirm() {
  Rig r;
  TEST_ASSERT_TRUE(r.s.begin(fixture::config()).code == SessionCode::Ok);
  TEST_ASSERT_TRUE(fixture::submit(r.s, fixture::command()).code == SessionCode::Ok);
  r.s.poll();
  TEST_ASSERT_EQUAL_INT(1, r.e.calls);
  TEST_ASSERT_TRUE(r.s.replySize() > 0);
  TEST_ASSERT_NOT_NULL(std::strstr(r.s.replyData(), "\"schema\":\"genesis_swarm_v2\""));
  TEST_ASSERT_NOT_NULL(std::strstr(r.s.replyData(), "\"command_id\":\"c1\""));
  TEST_ASSERT_TRUE(r.s.confirmReplyPublished(r.s.replySequence() + 1) == SessionCode::WrongSequence);
  TEST_ASSERT_TRUE(r.s.confirmReplyPublished(r.s.replySequence()) == SessionCode::Ok);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)r.s.replySize());
}
static void test_duplicate_retry_executes_once() {
  Rig r; r.s.begin(fixture::config());
  fixture::submit(r.s, fixture::command()); r.s.poll();
  r.s.confirmReplyPublished(r.s.replySequence());
  fixture::submit(r.s, fixture::command()); r.s.poll();
  TEST_ASSERT_EQUAL_INT(1, r.e.calls);
}
static void test_route_match() {
  Rig r; r.s.begin(fixture::config());
  TEST_ASSERT_TRUE(r.s.matchesRoute("lab", "node"));
  TEST_ASSERT_FALSE(r.s.matchesRoute("lab", "other"));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_submit_before_begin_fails);
  RUN_TEST(test_success_reply_and_confirm);
  RUN_TEST(test_duplicate_retry_executes_once);
  RUN_TEST(test_route_match);
  return UNITY_END();
}
