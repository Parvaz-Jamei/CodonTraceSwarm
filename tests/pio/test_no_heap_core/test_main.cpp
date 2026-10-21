// SPDX-License-Identifier: MIT
// Unity suite [env:native_noheap]: zero heap calls on the codec, command,
// inbox, session and ArduinoMqttAdapter hot paths. Counters: tests/heap_counter.h
// (link-time --wrap of the malloc family plus every operator new/delete form).
#include <unity.h>
#include <cstring>
#include <string>
#include "heap_counter.h"
#include "cts/ArduinoMqttAdapter.h"
#include "session_fixture.h"
using namespace cts;

void setUp() {}
void tearDown() { heap::counting = false; }

static void *volatile sink;
static void test_counters_see_every_heap_entry_point() {
  unsigned long n = 0;
  { heap::Window w; sink = std::malloc(8); std::free(sink); n = heap::allocs; }
  TEST_ASSERT_EQUAL_UINT32(1, n);
  { heap::Window w; sink = std::calloc(2, 8); sink = std::realloc(sink, 32); std::free(sink); n = heap::allocs; }
  TEST_ASSERT_EQUAL_UINT32(2, n);
  { heap::Window w; void *p = nullptr; if (posix_memalign(&p, 64, 64) == 0) std::free(p); sink = aligned_alloc(64, 64); std::free(sink); n = heap::allocs; }
  TEST_ASSERT_EQUAL_UINT32(2, n);
  { heap::Window w; int *p = new int(1); delete p; int *a = new int[4]; delete[] a; n = heap::allocs; }
  TEST_ASSERT_EQUAL_UINT32(2, n);
  { heap::Window w; int *p = new (std::nothrow) int(1); delete p; int *a = new (std::nothrow) int[4]; delete[] a; n = heap::allocs; }
  TEST_ASSERT_EQUAL_UINT32(2, n);
  struct alignas(64) Wide { char b[64]; };
  { heap::Window w; Wide *p = new Wide; delete p; Wide *a = new Wide[2]; delete[] a; Wide *q = new (std::nothrow) Wide; delete q; n = heap::allocs; }
  TEST_ASSERT_EQUAL_UINT32(3, n);
  { heap::Window w; std::string s(100, 'x'); sink = const_cast<char *>(s.data()); n = heap::allocs; sink = nullptr; }
  TEST_ASSERT_TRUE(n >= 1);
}

static ChannelSpec chan(const char *name, ValueType t) {
  ChannelSpec c = fixture::channel(name);
  c.descriptor.type = t;
  c.descriptor.hasRate = false;
  if (t == ValueType::Vector) { c.descriptor.axes[0] = "x"; c.descriptor.axes[1] = "y"; c.descriptor.axisCount = 2; }
  if (t == ValueType::Waveform) { c.descriptor.length = 3; c.descriptor.samplePeriodUs = 1000; }
  return c;
}
static char out[TxBytes];
static void test_telemetry_hot_path() {
  static Device d;
  TEST_ASSERT_TRUE(d.define(chan("s", ValueType::Scalar)) == Error::Ok);
  TEST_ASSERT_TRUE(d.define(chan("v", ValueType::Vector)) == Error::Ok);
  TEST_ASSERT_TRUE(d.define(chan("w", ValueType::Waveform)) == Error::Ok);
  TEST_ASSERT_TRUE(d.define(chan("b", ValueType::Boolean)) == Error::Ok);
  TEST_ASSERT_TRUE(d.define(chan("c", ValueType::Category)) == Error::Ok);
  TEST_ASSERT_TRUE(d.begin(fixture::config()) == Error::Ok);
  Sample vec = Sample::scalar(0, fixture::T); vec.values[0] = 1; vec.values[1] = 2; vec.count = 2;
  Sample wav = Sample::scalar(0, fixture::T); wav.values[0] = 1; wav.values[1] = 2; wav.values[2] = 3; wav.count = 3;
  Sample boo = Sample::scalar(0, fixture::T); boo.boolean = true;
  Sample cat = Sample::scalar(0, fixture::T); cat.category = "idle";
  Error e[8];
  unsigned long n;
  {
    heap::Window w;
    e[0] = d.hello(out, sizeof out);
    for (int i = 0; i < 50; ++i) e[1] = d.reading("s", Sample::scalar(20.0 + i, fixture::T + i), out, sizeof out);
    e[2] = d.reading("v", vec, out, sizeof out);
    e[3] = d.reading("w", wav, out, sizeof out);
    e[4] = d.reading("b", boo, out, sizeof out);
    e[5] = d.reading("c", cat, out, sizeof out);
    e[6] = d.reading("s", Sample::invalid("read_failed", fixture::T), out, sizeof out);
    e[7] = d.heartbeat(out, sizeof out);
    n = heap::allocs + heap::frees;
  }
  for (int i = 0; i < 8; ++i) TEST_ASSERT_TRUE_MESSAGE(e[i] == Error::Ok, "encode failed");
  TEST_ASSERT_EQUAL_UINT32(0, n);
}

struct FixedString {
  const char *p;
  size_t length() const { return std::strlen(p); }
  const char *c_str() const { return p; }
};
struct FakeMqtt { // fixed buffers only, so the fake itself never allocates
  bool online = true; void (*cb)(int) = nullptr;
  const char *inTopic = ""; const char *inBody = ""; size_t inLen = 0, readPos = 0;
  bool pending = false; char outTopic[160] = {}; char outBody[1100] = {}; size_t outLen = 0;
  bool connected() const { return online; }
  void onMessage(void (*f)(int)) { cb = f; }
  void stop() { online = false; }
  void poll() { if (pending && cb) { pending = false; readPos = 0; cb((int)inLen); } }
  int messageRetain() const { return 0; }
  int messageQoS() const { return 1; }
  FixedString messageTopic() const { return FixedString{inTopic}; }
  int read(uint8_t *b, size_t n) { if (readPos + n > inLen) return -1; std::memcpy(b, inBody + readPos, n); readPos += n; return (int)n; }
  int beginMessage(const char *t, unsigned long, bool, uint8_t, bool) { std::strncpy(outTopic, t, sizeof outTopic - 1); outLen = 0; return 1; }
  size_t write(const uint8_t *b, size_t n) { if (outLen + n > sizeof outBody) return 0; std::memcpy(outBody + outLen, b, n); outLen += n; return n; }
  int endMessage() { return 1; }
};
static void test_command_and_mqtt_adapter_hot_path() {
  static Device d; static fixture::Clock c; static fixture::Executor e;
  static CommandSession s(d, c, e); static NetworkInbox in; static FakeMqtt m;
  d.define(fixture::channel()); d.allow("s", Operation::Sample);
  TEST_ASSERT_TRUE(s.begin(fixture::config()).code == SessionCode::Ok);
  in.bind("lab", "node");
  static ArduinoMqttAdapter<FakeMqtt, 7> a(m, in);
  std::string wires[6];  // built before the window: the fixture uses std::string
  for (int i = 0; i < 6; ++i) wires[i] = fixture::command(i + 1);
  bool attached; int published = 0, enq = 0;
  unsigned long n;
  {
    heap::Window w;
    attached = a.attach(1, true);
    for (int i = 0; i < 6; ++i) {
      m.inTopic = "genesis/swarm/lab/node/down"; m.inBody = wires[i].data(); m.inLen = wires[i].size(); m.pending = true;
      if (a.receive((uint64_t)i + 1) == ArduinoMqttCode::Enqueued) ++enq;
      in.pollOne(s);
      s.poll();
      if (a.publishReply(s) == ArduinoMqttCode::Published) ++published;
    }
    a.close();
    n = heap::allocs + heap::frees;
  }
  TEST_ASSERT_TRUE(attached);
  TEST_ASSERT_EQUAL_INT(6, enq);
  TEST_ASSERT_EQUAL_INT(6, published);
  TEST_ASSERT_EQUAL_INT(6, e.calls);
  TEST_ASSERT_EQUAL_STRING("genesis/swarm/lab/node/up", m.outTopic);
  TEST_ASSERT_EQUAL_UINT32(0, n);
}

static void test_command_decoder_hot_path() {
  static CommandDecoder dec;
  std::string wire = fixture::command(9);
  CommandContext ctx; ctx.swarmId = "lab"; ctx.nodeId = "node"; ctx.bootId = "boot"; ctx.nowMs = fixture::T;
  CommandError r = CommandError::Ok, bad = CommandError::Ok;
  unsigned long n;
  {
    heap::Window w;
    Command cmd;
    for (int i = 0; i < 20; ++i) r = dec.decode(wire.data(), wire.size(), ctx, cmd);
    bad = dec.decode("{\"x\":[[[[[[[[[[1]]]]]]]]]]}", 27, ctx, cmd);
    n = heap::allocs + heap::frees;
  }
  TEST_ASSERT_TRUE(r == CommandError::Ok);
  TEST_ASSERT_TRUE(bad != CommandError::Ok);
  TEST_ASSERT_EQUAL_UINT32(0, n);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_counters_see_every_heap_entry_point);
  RUN_TEST(test_telemetry_hot_path);
  RUN_TEST(test_command_decoder_hot_path);
  RUN_TEST(test_command_and_mqtt_adapter_hot_path);
  return UNITY_END();
}
