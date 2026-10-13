// SPDX-License-Identifier: MIT
// Synthetic reconnect orchestration. Transport events are not a live broker.
#include "cts/ReconnectOrchestrator.h"
#include "session_fixture.h"
#include <cstdio>
#include <cstring>
#include <iostream>
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
struct Link : cts::ConnectAttemptClient {
  int starts = 0, polls = 0, cancels = 0, live = 0, maxLive = 0;
  int secondStartCancels = -1;
  bool progressed = true, done = false, ok = true, stacked = false;
  uint64_t gen = 0;
  cts::AttemptIo start(uint64_t g) noexcept override {
    if (starts == 1)
      secondStartCancels = cancels;
    if (live != 0)
      stacked = true;
    ++starts;
    ++live;
    if (live > maxLive)
      maxLive = live;
    gen = g;
    return cts::AttemptIo::Ok;
  }
  cts::AttemptIo poll(uint64_t g, bool &p, bool &d, bool &o) noexcept override {
    ++polls;
    p = progressed;
    d = done && g == gen;
    o = ok;
    return cts::AttemptIo::Ok;
  }
  void cancel(uint64_t) noexcept override {
    ++cancels;
    if (live > 0)
      --live;
  }
};
struct Pub : cts::QosPublisher {
  enum class Mode { Complete, Partial, Fail, WriteOnly, Regress, Grow } mode =
      Mode::Complete;
  int steps = 0, cancels = 0, completed = 0, script = 0;
  char last[8192] = {};
  char log[8][8192] = {};
  bool step(uint64_t, const char *data, size_t size, size_t &accepted,
            bool &writeComplete, bool &qos1Complete) noexcept override {
    ++steps;
    last[0] = 0;
    if (!data || !size || size >= sizeof(last))
      return false;
    std::memcpy(last, data, size);
    last[size] = 0;
    if (mode == Mode::Fail) {
      accepted = size > 3 ? 3 : size;
      writeComplete = false;
      qos1Complete = false;
      return false;
    }
    if (mode == Mode::Regress) {
      accepted = script == 0 ? (size > 4 ? 4 : size) : (size > 1 ? 1 : 0);
      ++script;
      writeComplete = false;
      qos1Complete = false;
      return true;
    }
    if (mode == Mode::Grow) {
      accepted = script == 0 ? (size > 4 ? 4 : size) : (size > 8 ? 8 : size);
      if (script < 8)
        ++script;
      writeComplete = false;
      qos1Complete = false;
      return true;
    }
    if (mode == Mode::Partial) {
      accepted = size > 4 ? 4 : size;
      writeComplete = false;
      qos1Complete = false;
      return true;
    }
    if (mode == Mode::WriteOnly) {
      accepted = size;
      writeComplete = true;
      qos1Complete = false;
      mode = Mode::Complete;
      return true;
    }
    accepted = size;
    writeComplete = true;
    qos1Complete = true;
    if (completed < 8)
      std::memcpy(log[completed], last, size + 1);
    ++completed;
    return true;
  }
  void cancel(uint64_t) noexcept override { ++cancels; }
};
struct Env {
  cts::Device device;
  fixture::Clock clock;
  fixture::Executor exec;
  cts::CommandSession session;
  cts::NetworkInbox inbox;
  cts::ReconnectPolicy policy;
  Link link;
  Pub pub;
  cts::ReconnectOrchestrator orch;
  Env()
      : session(device, clock, exec),
        orch(device, session, inbox, policy, link, pub) {
    CHECK(device.define(fixture::channel()) == cts::Error::Ok);
    CHECK(device.allow("s", cts::Operation::Sample) == cts::Error::Ok);
    CHECK(session.begin(fixture::config()).code == cts::SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == cts::InboxCode::Ok);
    CHECK(orch.start(0));
  }
};
static bool kind(const char *bytes, const char *name) {
  char needle[32];
  std::snprintf(needle, sizeof(needle), "\"kind\":\"%s\"", name);
  return bytes && std::strstr(bytes, needle);
}
int main() {
  using namespace cts;
  char scratch[TxBytes];
  const Sample sample = Sample::scalar(1.5, fixture::T);
  {
    Env env;
    CHECK(env.orch.poll(0, false, 0) == OrchCode::Refused);
    CHECK(env.link.starts == 0);
    CHECK(env.orch.connectRefusals() >= 1);
    CHECK(!env.orch.readingsAllowed());
    env.link.done = false;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.link.starts == 1);
    CHECK(env.orch.generation() == 0);
    CHECK(env.link.polls >= 1);
    env.link.done = true;
    env.pub.mode = Pub::Mode::Partial;
    const auto seq = env.device.nextSequence();
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(env.orch.generation() != 0);
    CHECK(!env.orch.helloComplete());
    CHECK(env.device.nextSequence() == seq + 1);
    CHECK(kind(env.pub.last, "hello"));
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::HelloRequired);
    CHECK(env.pub.completed == 0);
    env.pub.mode = Pub::Mode::Complete;
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.readingsAllowed());
    CHECK(kind(env.pub.log[0], "hello"));
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(kind(env.pub.log[1], "reading"));
    const auto gen = env.orch.generation();
    const auto losses = env.orch.losses();
    CHECK(env.orch.onDisconnected(gen, 2, 0) == OrchCode::Ok);
    CHECK(env.orch.losses() == losses);
    CHECK(!env.orch.readingsAllowed());
    env.link.done = true;
    env.pub.mode = Pub::Mode::Partial;
    const auto due = env.policy.nextAttemptMs();
    const auto before = env.device.nextSequence();
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::HelloRequired);
    CHECK(env.device.nextSequence() == before + 1);
    CHECK(kind(env.pub.last, "hello"));
    CHECK(!kind(env.pub.last, "reading"));
    env.pub.mode = Pub::Mode::Complete;
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(kind(env.pub.log[env.pub.completed - 1], "reading"));
    bool helloBefore = false;
    for (int i = 0; i < env.pub.completed; ++i) {
      if (kind(env.pub.log[i], "hello"))
        helloBefore = true;
      if (kind(env.pub.log[i], "reading"))
        CHECK(helloBefore);
    }
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, false, 0) == OrchCode::Refused);
    CHECK(env.link.starts == 0);
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(fixture::submit(env.session, fixture::command()).dispatch.code ==
          DispatchCode::Queued);
    CHECK(env.session.poll().code == SessionCode::ReplyPending);
    const auto seq = env.session.replySequence();
    const auto size = env.session.replySize();
    CHECK(size > 0);
    env.pub.mode = Pub::Mode::Fail;
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(env.session.replySize() == size);
    CHECK(env.session.replySequence() == seq);
    CHECK(env.orch.health().repliesConfirmed == 0);
    env.pub.mode = Pub::Mode::WriteOnly;
    CHECK(env.orch.poll(2, true, 0) == OrchCode::Ok);
    CHECK(env.session.replySize() == size);
    CHECK(env.orch.health().repliesConfirmed == 0);
    CHECK(env.orch.publishActive());
    CHECK(env.orch.poll(3, true, 0) == OrchCode::Ok);
    CHECK(env.session.replySize() == 0);
    CHECK(env.orch.health().repliesConfirmed == 1);
    CHECK(fixture::submit(env.session, fixture::command(2)).dispatch.code ==
          DispatchCode::Queued);
    CHECK(env.session.poll().code == SessionCode::ReplyPending);
    const auto seq2 = env.session.replySequence();
    const auto size2 = env.session.replySize();
    env.pub.mode = Pub::Mode::Partial;
    CHECK(env.orch.poll(4, true, 0) == OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.session.replySize() == size2);
    const auto gen = env.orch.generation();
    CHECK(env.orch.onDisconnected(gen, 5, 0) == OrchCode::Lost);
    CHECK(env.orch.losses() == 1);
    CHECK(env.session.replySize() == size2);
    CHECK(env.session.replySequence() == seq2);
    CHECK(env.orch.onDisconnected(gen, 6, 0) == OrchCode::Stale);
    CHECK(env.orch.losses() == 1);
    const auto due = env.policy.nextAttemptMs();
    env.link.done = true;
    env.pub.mode = Pub::Mode::Complete;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::NotConnected);
    const int before = env.pub.completed;
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::HelloRequired);
    CHECK(env.session.replySize() == 0);
    CHECK(kind(env.pub.log[before], "reply"));
    CHECK(env.orch.poll(due + 1, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(kind(env.pub.log[before + 1], "hello"));
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(kind(env.pub.log[before + 2], "reading"));
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(orch.start(0));
    link.done = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1);
    CHECK(link.cancels >= 1);
    CHECK(link.live == 0);
    CHECK(!link.stacked);
    CHECK(orch.generation() == 0);
    CHECK(!orch.helloComplete());
    CHECK(pub.steps == 0);
    const auto due = policy.nextAttemptMs();
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 2);
    CHECK(link.secondStartCancels >= 1);
    CHECK(!link.stacked);
    CHECK(link.maxLive == 1);
    CHECK(link.cancels >= 1);
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(inbox.connected(50) == InboxCode::Ok);
    CHECK(orch.start(0));
    link.done = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1);
    CHECK(link.cancels >= 1);
    CHECK(link.live == 0);
    CHECK(!link.stacked);
    CHECK(orch.generation() == 0);
    CHECK(pub.steps == 0);
    const auto due = policy.nextAttemptMs();
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 2);
    CHECK(link.secondStartCancels >= 1);
    CHECK(!link.stacked);
    CHECK(link.maxLive == 1);
    CHECK(link.live == 0);
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    const auto seq = env.device.nextSequence();
    const int completed = env.pub.completed;
    CHECK(env.orch.publishReading("missing", sample, scratch, sizeof(scratch)) ==
          OrchCode::Failed);
    CHECK(env.device.nextSequence() == seq);
    CHECK(env.orch.readingsAllowed());
    char tiny[4];
    CHECK(env.orch.publishReading("s", sample, tiny, sizeof(tiny)) ==
          OrchCode::Failed);
    CHECK(env.device.nextSequence() == seq);
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == completed);
    CHECK(env.orch.readingsAllowed());
    env.pub.mode = Pub::Mode::Fail;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    CHECK(env.orch.health().readings == 0);
    CHECK(!env.orch.readingsAllowed());
    CHECK(kind(env.pub.last, "reading"));
    char frozen[8192];
    std::snprintf(frozen, sizeof(frozen), "%s", env.pub.last);
    const auto queued = env.device.nextSequence();
    CHECK(queued == seq + 1);
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    CHECK(env.device.nextSequence() == queued);
    CHECK(std::strcmp(env.pub.last, frozen) == 0);
    CHECK(!env.orch.readingsAllowed());
    env.pub.mode = Pub::Mode::Complete;
    const int before = env.pub.completed;
    CHECK(env.orch.poll(2, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == before + 1);
    CHECK(std::strcmp(env.pub.log[env.pub.completed - 1], frozen) == 0);
    CHECK(kind(env.pub.log[env.pub.completed - 1], "reading"));
    CHECK(env.orch.health().readings == 1);
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.readingsAllowed());
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(env.link.live == 1);
    CHECK(env.link.cancels == 0);
    const int cancels = env.link.cancels;
    CHECK(env.orch.poll(1, false, 7) == OrchCode::Refused);
    CHECK(env.link.cancels == cancels + 1);
    CHECK(env.link.live == 0);
    CHECK(!env.link.stacked);
    const auto due = env.policy.nextAttemptMs();
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(!env.link.stacked);
    CHECK(env.link.maxLive == 1);
    CHECK(env.link.live == 1);
    const auto gen = env.orch.generation();
    CHECK(env.orch.onDisconnected(gen, due + 1, 0) == OrchCode::Ok);
    CHECK(env.link.live == 0);
    CHECK(env.link.cancels >= 2);
    const auto again = env.policy.nextAttemptMs();
    CHECK(env.orch.poll(again, true, 0) == OrchCode::Ok);
    CHECK(!env.link.stacked);
    CHECK(env.link.maxLive == 1);
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    env.pub.mode = Pub::Mode::Fail;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    CHECK(env.orch.losses() == 0);
    CHECK(!env.orch.publishActive());
    CHECK(!env.orch.readingsAllowed());
    const auto gen = env.orch.generation();
    CHECK(env.orch.onDisconnected(gen, 2, 0) == OrchCode::Ok);
    CHECK(env.orch.losses() == 1);
    CHECK(env.orch.onDisconnected(gen, 3, 0) == OrchCode::Stale);
    CHECK(env.orch.losses() == 1);
    env.pub.mode = Pub::Mode::Complete;
    env.link.done = true;
    const int before = env.pub.completed;
    const auto due = env.policy.nextAttemptMs();
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == before + 1);
    CHECK(kind(env.pub.log[env.pub.completed - 1], "hello"));
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.losses() == 1);
    std::cout << "probe staged_retry_loss before=0 after=" << env.orch.losses()
              << " stale=1\n";
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    env.pub.mode = Pub::Mode::Fail;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.poll(1, false, 0) == OrchCode::Refused);
    CHECK(env.orch.losses() == 1);
    CHECK(env.orch.poll(1, false, 0) == OrchCode::Refused);
    CHECK(env.orch.losses() == 1);
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.readingsAllowed());
    env.pub.mode = Pub::Mode::Partial;
    const int cancels = env.pub.cancels;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.pub.cancels == cancels);
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.poll(9999, true, 0) == OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.pub.cancels == cancels);
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.poll(10000, true, 0) == OrchCode::Ok);
    CHECK(!env.orch.publishActive());
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.readingsAllowed());
    CHECK(env.orch.losses() == 1);
    CHECK(env.pub.cancels == cancels + 1);
    CHECK(env.orch.health().publishFailures == 0);
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    env.pub.mode = Pub::Mode::Partial;
    const int cancels = env.pub.cancels;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.pub.cancels == cancels);
    CHECK(env.orch.poll(1000000000ull, true, 0) == OrchCode::Ok);
    CHECK(!env.orch.publishActive());
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.readingsAllowed());
    CHECK(env.orch.losses() == 1);
    CHECK(env.pub.cancels == cancels + 1);
    std::cout << "probe partial_stall active=" << env.orch.publishActive()
              << " cancels=" << env.pub.cancels
              << " allowed=" << env.orch.readingsAllowed()
              << " losses=" << env.orch.losses()
              << " hello=" << env.orch.helloComplete() << "\n";
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    env.pub.mode = Pub::Mode::Grow;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.orch.poll(9000, true, 0) == OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.poll(18999, true, 0) == OrchCode::Ok);
    CHECK(env.orch.publishActive());
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.poll(19000, true, 0) == OrchCode::Ok);
    CHECK(!env.orch.publishActive());
    CHECK(env.orch.losses() == 1);
    CHECK(env.orch.readingsAllowed());
    CHECK(env.orch.helloComplete());
  }
  {
    Env env;
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    env.pub.mode = Pub::Mode::Regress;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(env.orch.publishActive());
    char frozen[8192];
    std::snprintf(frozen, sizeof(frozen), "%s", env.pub.last);
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(!env.orch.publishActive());
    CHECK(!env.orch.readingsAllowed());
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.health().publishFailures == 1);
    CHECK(env.pub.cancels == 1);
    env.pub.mode = Pub::Mode::Complete;
    const int before = env.pub.completed;
    CHECK(env.orch.poll(2, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == before + 1);
    CHECK(std::strcmp(env.pub.log[env.pub.completed - 1], frozen) == 0);
    CHECK(env.orch.health().readings == 1);
    CHECK(env.orch.losses() == 0);
    CHECK(env.orch.readingsAllowed());
  }
  {
    Env env;
    CHECK(env.orch.poll(100, true, 0) == OrchCode::Ok);
    CHECK(env.policy.state() != LinkState::Fault);
    const int starts = env.link.starts;
    const int cancels = env.link.cancels;
    const auto rolled = env.orch.poll(99, true, 0);
    CHECK(rolled == OrchCode::Fault);
    CHECK(env.policy.state() == LinkState::Fault);
    CHECK(env.link.starts == starts);
    CHECK(env.link.cancels == cancels + 1);
    CHECK(env.orch.poll(99, true, 0) == OrchCode::Fault);
    CHECK(env.link.starts == starts);
    std::cout << "probe rollback="
              << (rolled == OrchCode::Fault &&
                          env.policy.state() == LinkState::Fault
                      ? "Fault"
                      : "not-fault")
              << " starts=" << env.link.starts << "\n";
  }
  if (failures)
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
  else
    std::cout << "PASS: orchestrator hello/reply/loss/clock checks=" << checks
              << " failures=0\n";
  return failures ? 1 : 0;
}
