// SPDX-License-Identifier: MIT
// Unity suite: backs the README "Heap Allocations Zero (Hot Path)" claim.
// Replaces global operator new/delete with counting versions, then drives the
// hot path (hello, readings, command decode, NetworkInbox push/poll,
// CommandSession submit/poll/reply confirm) and asserts zero allocations.
// tools/check_no_heap.py separately checks that no library object file
// references malloc/calloc/realloc/free or operator new/delete at all.
#include <unity.h>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include "cts/NetworkInbox.h"
#include "session_fixture.h"
using namespace cts;

static volatile bool counting = false;
static volatile unsigned long allocations = 0;
void *operator new(std::size_t n) {
  if (counting) ++allocations;
  if (void *p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void *operator new[](std::size_t n) { return operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

static char out[TxBytes];
void setUp() {}
void tearDown() { counting = false; }

static void test_counter_detects_allocation() {
  allocations = 0; counting = true;
  int *p = new int(1);
  counting = false;
  delete p;
  TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)allocations);
}

static void test_telemetry_hot_path_allocates_nothing() {
  static Device d;
  TEST_ASSERT_TRUE(d.define(fixture::channel()) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(fixture::config()) == Error::Ok);
  allocations = 0; counting = true;
  Error h = d.hello(out, sizeof out);
  Error r = Error::Ok;
  for (int i = 0; i < 100 && r == Error::Ok; ++i)
    r = d.reading("s", Sample::scalar(20.0 + i, fixture::T + i), out, sizeof out);
  counting = false;
  TEST_ASSERT_TRUE(h == Error::Ok);
  TEST_ASSERT_TRUE(r == Error::Ok);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)allocations);
}

static void test_command_hot_path_allocates_nothing() {
  static Device d; static fixture::Clock c; static fixture::Executor e;
  static CommandSession s(d, c, e); static NetworkInbox in;
  d.define(fixture::channel()); d.allow("s", Operation::Sample);
  TEST_ASSERT_TRUE(s.begin(fixture::config()).code == SessionCode::Ok);
  in.bind("lab", "node"); in.connected(1);
  // Wire strings are built before counting: the fixture uses std::string.
  std::string wires[5];
  for (int i = 0; i < 5; ++i) wires[i] = fixture::command(i + 1);
  allocations = 0; counting = true;
  for (int i = 0; i < 5; ++i) {
    ReceiveFragment f;
    f.topic = in.topic(); f.topicLength = std::strlen(f.topic);
    f.data = wires[i].data(); f.length = f.total = wires[i].size();
    f.connection = 1; f.token = (uint64_t)i + 1;
    f.authenticated = true; f.retainKnown = true; f.retained = false;
    in.push(f);
    in.pollOne(s);
    s.poll();
    s.confirmReplyPublished(s.replySequence());
  }
  counting = false;
  TEST_ASSERT_EQUAL_INT(5, e.calls);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)allocations);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_counter_detects_allocation);
  RUN_TEST(test_telemetry_hot_path_allocates_nothing);
  RUN_TEST(test_command_hot_path_allocates_nothing);
  return UNITY_END();
}
