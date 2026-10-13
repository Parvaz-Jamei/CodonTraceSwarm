// Independent re-review of D1-D4. Does not modify the library.
// Synthetic inputs only. Not a bench, flash, soak, or scientific result.
#include "cts/ReconnectOrchestrator.h"
#include "session_fixture.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
namespace cts {
struct AttemptDeadlineProbe {
  static void arm(NonblockingAttempt &attempt, uint64_t generation,
                  uint64_t began, uint64_t timeoutMs) {
    attempt.phase_ = AttemptPhase::Running;
    attempt.generation_ = generation;
    attempt.began_ = began;
    attempt.timeoutMs_ = timeoutMs;
  }
};
} // namespace cts
struct Link : cts::ConnectAttemptClient {
  cts::NonblockingAttempt *attempt = nullptr;
  int starts = 0, polls = 0, cancels = 0, live = 0, maxLive = 0;
  int cancelsBeforeSecond = -1, liveBeforeSecond = -1;
  bool progressed = true, done = false, ok = true, failPoll = false,
       stacked = false;
  uint64_t gen = 0, cancelledGen = 0;
  cts::AttemptPhase phaseAtCancel = cts::AttemptPhase::Idle;
  cts::AttemptIo start(uint64_t g) noexcept override {
    if (starts == 1) {
      cancelsBeforeSecond = cancels;
      liveBeforeSecond = live;
    }
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
    if (failPoll || g != gen) {
      p = d = o = false;
      return cts::AttemptIo::Fail;
    }
    p = progressed;
    d = done;
    o = ok;
    return cts::AttemptIo::Ok;
  }
  void cancel(uint64_t g) noexcept override {
    if (cancels == 0 && attempt)
      phaseAtCancel = attempt->phase();
    ++cancels;
    cancelledGen = g;
    if (live > 0)
      --live;
  }
};
struct Pub : cts::QosPublisher {
  enum class Mode { Complete, Fail } mode = Mode::Complete;
  int steps = 0, cancels = 0, completed = 0;
  char shots[6][8192] = {};
  size_t shotN[6] = {};
  int shotCount = 0;
  bool step(uint64_t, const char *data, size_t size, size_t &accepted,
            bool &writeComplete, bool &qos1Complete) noexcept override {
    ++steps;
    if (shotCount < 6 && data && size < sizeof(shots[0])) {
      std::memcpy(shots[shotCount], data, size);
      shots[shotCount][size] = 0;
      shotN[shotCount] = size;
      ++shotCount;
    }
    if (mode == Mode::Fail || !data || !size || size >= sizeof(shots[0])) {
      accepted = 0;
      writeComplete = false;
      qos1Complete = false;
      return false;
    }
    accepted = size;
    writeComplete = true;
    qos1Complete = true;
    ++completed;
    return true;
  }
  void cancel(uint64_t) noexcept override { ++cancels; }
  const char *lastDone() const {
    return completed > 0 && shotCount > 0 ? shots[shotCount - 1] : "";
  }
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
  explicit Env(bool bindInbox)
      : session(device, clock, exec),
        orch(device, session, inbox, policy, link, pub) {
    CHECK(device.define(fixture::channel()) == cts::Error::Ok);
    CHECK(device.allow("s", cts::Operation::Sample) == cts::Error::Ok);
    CHECK(session.begin(fixture::config()).code == cts::SessionCode::Ok);
    if (bindInbox)
      CHECK(inbox.bind("lab", "node") == cts::InboxCode::Ok);
    CHECK(orch.start(0));
  }
};
static bool kind(const char *bytes, const char *name) {
  char needle[40];
  std::snprintf(needle, sizeof(needle), "\"kind\":\"%s\"", name);
  return bytes && std::strstr(bytes, needle);
}
static bool sameBytes(const char *a, const char *b) {
  return a && b && std::strcmp(a, b) == 0;
}
static void d1Attempt(const char *name, bool rollback, bool failPoll,
                      bool badHandshake) {
  using namespace cts;
  Link link;
  NonblockingAttempt attempt(link);
  link.attempt = &attempt;
  CHECK(attempt.start(4, rollback ? 100 : 0, 1000) == AttemptCode::Started);
  CHECK(link.starts == 1);
  CHECK(link.live == 1);
  CHECK(attempt.phase() == AttemptPhase::Running);
  if (failPoll)
    link.failPoll = true;
  if (badHandshake) {
    link.done = true;
    link.ok = false;
  }
  const uint64_t now = rollback ? 50 : 1;
  const AttemptCode code = attempt.poll(4, now);
  if (rollback)
    CHECK(code == AttemptCode::Fault);
  else
    CHECK(code == AttemptCode::Failed);
  CHECK(attempt.phase() == AttemptPhase::Failed);
  CHECK(link.phaseAtCancel == AttemptPhase::Running);
  CHECK(link.cancels == 1);
  CHECK(link.cancelledGen == 4);
  CHECK(link.live == 0);
  CHECK(link.polls == (rollback ? 0 : 1));
  CHECK(attempt.cancel(4) == AttemptCode::Stale);
  CHECK(link.cancels == 1);
  CHECK(link.live == 0);
  CHECK(attempt.phase() == AttemptPhase::Failed);
  CHECK(attempt.start(5, now, 1000) == AttemptCode::Started);
  CHECK(link.starts == 2);
  CHECK(link.live == 1);
  CHECK(link.maxLive == 1);
  CHECK(!link.stacked);
  std::cout << "D1 " << name << " cancels=" << link.cancels
            << " live=" << link.live << " stacked=" << link.stacked << "\n";
}
static void d1Orchestrated(const char *name, bool failPoll, bool badHandshake) {
  using namespace cts;
  Env env(true);
  if (failPoll)
    env.link.failPoll = true;
  if (badHandshake) {
    env.link.done = true;
    env.link.ok = false;
  }
  CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
  CHECK(env.link.starts == 1);
  CHECK(env.link.cancels >= 1);
  CHECK(env.link.live == 0);
  CHECK(!env.link.stacked);
  CHECK(env.policy.state() == LinkState::Waiting);
  CHECK(env.orch.generation() == 0);
  env.link.failPoll = false;
  env.link.done = true;
  env.link.ok = true;
  const uint64_t due = env.policy.nextAttemptMs();
  CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
  CHECK(env.link.starts == 2);
  CHECK(env.link.cancelsBeforeSecond >= 1);
  CHECK(env.link.liveBeforeSecond == 0);
  CHECK(!env.link.stacked);
  CHECK(env.link.maxLive == 1);
  std::cout << "D1 orch " << name
            << " cancels_before_second=" << env.link.cancelsBeforeSecond
            << " live_before_second=" << env.link.liveBeforeSecond
            << " maxLive=" << env.link.maxLive << "\n";
}
static void d2(const char *name, bool preconnect) {
  using namespace cts;
  Env env(true);
  if (!preconnect) {
    // Rebuild is unnecessary: bind already happened. Use a fresh unbound env.
  }
  env.link.done = true;
  env.link.ok = true;
  if (preconnect)
    CHECK(env.inbox.connected(50) == InboxCode::Ok);
  CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
  CHECK(env.link.starts == 1);
  CHECK(env.link.cancels >= 1);
  CHECK(env.link.cancelledGen == env.link.gen || env.link.cancels >= 1);
  CHECK(env.link.live == 0);
  CHECK(!env.link.stacked);
  CHECK(env.link.maxLive == 1);
  CHECK(env.orch.generation() == 0);
  CHECK(!env.orch.helloComplete());
  CHECK(env.pub.steps == 0);
  CHECK(env.pub.completed == 0);
  const uint64_t due = env.policy.nextAttemptMs();
  CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
  CHECK(env.link.starts >= 2);
  CHECK(env.link.cancelsBeforeSecond >= 1);
  CHECK(env.link.liveBeforeSecond == 0);
  CHECK(!env.link.stacked);
  CHECK(env.link.maxLive == 1);
  CHECK(env.pub.steps == 0);
  std::cout << "D2 " << name << " cancels_before_second="
            << env.link.cancelsBeforeSecond
            << " live_before_second=" << env.link.liveBeforeSecond
            << " maxLive=" << env.link.maxLive
            << " steps=" << env.pub.steps << "\n";
}
int main() {
  using namespace cts;
  const uint64_t kMax = std::numeric_limits<uint64_t>::max();
  d1Attempt("rollback", true, false, false);
  d1Attempt("io_fail", false, true, false);
  d1Attempt("done_not_ok", false, false, true);
  d1Orchestrated("io_fail", true, false);
  d1Orchestrated("done_not_ok", false, true);
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
    link.ok = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1);
    CHECK(link.cancels >= 1);
    CHECK(link.live == 0);
    CHECK(!link.stacked);
    CHECK(orch.generation() == 0);
    CHECK(pub.steps == 0);
    const uint64_t due = policy.nextAttemptMs();
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(link.cancelsBeforeSecond >= 1);
    CHECK(link.liveBeforeSecond == 0);
    CHECK(!link.stacked);
    CHECK(link.maxLive == 1);
    CHECK(pub.steps == 0);
    std::cout << "D2 unbound cancels_before_second=" << link.cancelsBeforeSecond
              << " live_before_second=" << link.liveBeforeSecond << "\n";
  }
  d2("wrong_connection", true);
  {
    Env env(true);
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    CHECK(env.orch.readingsAllowed());
    const uint64_t seq = env.device.nextSequence();
    const int helloCompleted = env.pub.completed;
    char scratch[TxBytes];
    const Sample sample = Sample::scalar(1.25, fixture::T);
    env.pub.mode = Pub::Mode::Fail;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    CHECK(env.orch.health().readings == 0);
    CHECK(!env.orch.readingsAllowed());
    CHECK(env.device.nextSequence() == seq + 1);
    CHECK(env.pub.shotCount == helloCompleted + 1);
    CHECK(kind(scratch, "reading"));
    CHECK(sameBytes(env.pub.shots[env.pub.shotCount - 1], scratch));
    const int queuedShots = env.pub.shotCount;
    char frozen[8192];
    std::snprintf(frozen, sizeof(frozen), "%s", scratch);
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    CHECK(env.device.nextSequence() == seq + 1);
    CHECK(env.pub.shotCount == queuedShots);
    CHECK(!env.orch.readingsAllowed());
    env.pub.mode = Pub::Mode::Complete;
    const int before = env.pub.completed;
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == before + 1);
    CHECK(env.orch.health().readings == 1);
    CHECK(env.orch.readingsAllowed());
    CHECK(sameBytes(env.pub.lastDone(), frozen));
    CHECK(kind(env.pub.lastDone(), "reading"));
    CHECK(env.device.nextSequence() == seq + 1);
    std::cout << "D3 retry bytes_equal=1 readings=" << env.orch.health().readings
              << " allowed_after=" << env.orch.readingsAllowed() << "\n";
  }
  {
    Env env(true);
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    const uint64_t seq = env.device.nextSequence();
    const int completed = env.pub.completed;
    const int steps = env.pub.steps;
    char scratch[TxBytes];
    std::memset(scratch, 'X', sizeof(scratch) - 1);
    scratch[sizeof(scratch) - 1] = 0;
    const Sample sample = Sample::scalar(1.25, fixture::T);
    CHECK(env.orch.publishReading("missing", sample, scratch, sizeof(scratch)) ==
          OrchCode::Failed);
    CHECK(env.device.nextSequence() == seq);
    CHECK(env.orch.readingsAllowed());
    CHECK(env.orch.health().readings == 0);
    CHECK(env.orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == completed);
    CHECK(env.pub.steps == steps);
    char tiny[4];
    std::memset(tiny, 'Y', sizeof(tiny));
    CHECK(env.orch.publishReading("s", sample, tiny, sizeof(tiny)) ==
          OrchCode::Failed);
    CHECK(env.device.nextSequence() == seq);
    CHECK(env.orch.readingsAllowed());
    CHECK(env.orch.poll(2, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == completed);
    CHECK(env.pub.steps == steps);
    const Sample bad = Sample::invalid("ok", fixture::T);
    CHECK(env.orch.publishReading("s", bad, scratch, sizeof(scratch)) ==
          OrchCode::Failed);
    CHECK(env.device.nextSequence() == seq);
    CHECK(env.orch.poll(3, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == completed);
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Ok);
    CHECK(env.pub.completed == completed + 1);
    CHECK(kind(env.pub.lastDone(), "reading"));
    CHECK(std::strstr(env.pub.lastDone(), "\"channel\":\"s\"") != nullptr);
    CHECK(env.orch.health().readings == 1);
    std::cout << "D3 true_failed stayed_unsent completed_delta="
              << (env.pub.completed - completed) << "\n";
  }
  {
    Env env(true);
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    char scratch[TxBytes];
    const Sample sample = Sample::scalar(9.5, fixture::T);
    env.pub.mode = Pub::Mode::Fail;
    CHECK(env.orch.publishReading("s", sample, scratch, sizeof(scratch)) ==
          OrchCode::Busy);
    char frozen[8192];
    std::snprintf(frozen, sizeof(frozen), "%s", scratch);
    const uint64_t gen = env.orch.generation();
    CHECK(env.orch.onDisconnected(gen, 2, 0) == OrchCode::Ok);
    CHECK(!env.orch.readingsAllowed());
    env.pub.mode = Pub::Mode::Complete;
    env.link.done = true;
    const int before = env.pub.completed;
    const uint64_t due = env.policy.nextAttemptMs();
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(env.pub.completed == before + 1);
    CHECK(kind(env.pub.lastDone(), "hello"));
    CHECK(!sameBytes(env.pub.lastDone(), frozen));
    // Immediate-complete publisher finishes hello inside this poll, so a
    // reading is no longer HelloRequired. The queued reading was not replayed.
    CHECK(env.orch.helloComplete());
    std::cout << "D3 disconnect_clears_retry first_after=hello\n";
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(9, kMax - 5, 1000) == AttemptCode::Fault);
    CHECK(attempt.phase() == AttemptPhase::Idle);
    CHECK(link.starts == 0);
    CHECK(link.cancels == 0);
    CHECK(link.live == 0);
    CHECK(attempt.poll(9, kMax) == AttemptCode::Stale);
    CHECK(link.live == 0);
    CHECK(attempt.start(9, 0, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(!link.stacked);
    std::cout << "D4 start_fault starts=" << (link.starts - 1)
              << " live_after_fault_was_0\n";
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    link.attempt = &attempt;
    AttemptDeadlineProbe::arm(attempt, 9, kMax - 5, 1000);
    link.live = 1;
    link.gen = 9;
    link.progressed = true;
    link.done = false;
    CHECK(attempt.poll(9, kMax) == AttemptCode::TimedOut);
    CHECK(attempt.phase() == AttemptPhase::TimedOut);
    CHECK(link.phaseAtCancel == AttemptPhase::Running);
    CHECK(link.cancels == 1);
    CHECK(link.polls == 0);
    CHECK(link.live == 0);
    CHECK(attempt.poll(9, kMax) == AttemptCode::Stale);
    CHECK(link.cancels == 1);
    CHECK(attempt.phase() == AttemptPhase::TimedOut);
    CHECK(attempt.start(10, 0, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(link.maxLive == 1);
    CHECK(!link.stacked);
    std::cout << "D4 armed_deadline code=TimedOut polls=" << link.polls
              << " cancels=" << link.cancels << "\n";
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(9, kMax - 1000, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(attempt.poll(9, kMax - 1) == AttemptCode::Progress);
    CHECK(attempt.phase() == AttemptPhase::Running);
    CHECK(link.cancels == 0);
    CHECK(link.polls == 1);
    CHECK(attempt.poll(9, kMax) == AttemptCode::TimedOut);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(link.polls == 1);
    CHECK(attempt.start(11, kMax - 999, 1000) == AttemptCode::Fault);
    CHECK(link.starts == 1);
    CHECK(link.live == 0);
    std::cout << "D4 representable_boundary progress_then_timeout\n";
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
    CHECK(orch.start(kMax - 5));
    link.done = true;
    CHECK(orch.poll(kMax - 5, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 0);
    CHECK(link.cancels == 0);
    CHECK(link.live == 0);
    // Retry delay cannot be added to UINT64_MAX-5, so the policy latches Fault
    // after the attempt refuses to open. That still leaves no live client.
    CHECK(policy.state() == LinkState::Fault);
    CHECK(orch.generation() == 0);
    CHECK(pub.steps == 0);
    std::cout << "D4 orch_unrepresentable starts=0 live=0 policy=Fault\n";
  }
  {
    Env env(true);
    env.link.done = false;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.link.starts == 1);
    CHECK(env.link.cancels == 0);
    CHECK(env.link.live == 1);
    CHECK(env.orch.poll(1, false, 0) == OrchCode::Refused);
    CHECK(env.link.cancels == 1);
    CHECK(env.link.live == 0);
    CHECK(env.orch.generation() == 0);
    std::cout << "regression clock_drop_while_running cancels="
              << env.link.cancels << "\n";
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
    CHECK(orch.start(100));
    link.done = false;
    CHECK(orch.poll(100, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1);
    CHECK(link.live == 1);
    const auto rolled = orch.poll(50, true, 0);
    // Fault is latched inside this poll; the return is still Ok. Cancel of the
    // Running client happens here. The following poll is what returns Fault.
    CHECK(rolled == OrchCode::Ok);
    CHECK(policy.state() == LinkState::Fault);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(orch.poll(50, true, 0) == OrchCode::Fault);
    CHECK(link.cancels == 1);
    std::cout << "regression rollback_while_running code="
              << static_cast<int>(rolled) << " cancels=" << link.cancels
              << "\n";
  }
  {
    Env env(true);
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    const int cancelsAfterAccept = env.link.cancels;
    const int liveAfterAccept = env.link.live;
    const uint64_t gen = env.orch.generation();
    CHECK(env.orch.onDisconnected(gen, 5, 0) == OrchCode::Ok);
    const int cancelsAfterDisc = env.link.cancels;
    const int liveAfterDisc = env.link.live;
    env.link.done = true;
    const uint64_t due = env.policy.nextAttemptMs();
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    // Accepted generation must be cancelled before the next start.
    CHECK(cancelsAfterDisc > cancelsAfterAccept);
    CHECK(liveAfterDisc == 0);
    CHECK(env.link.liveBeforeSecond == 0);
    CHECK(!env.link.stacked);
    CHECK(env.link.maxLive == 1);
    std::cout << "accepted_disconnect cancels_after_accept=" << cancelsAfterAccept
              << " live_after_accept=" << liveAfterAccept
              << " cancels_after_disc=" << cancelsAfterDisc
              << " live_after_disc=" << liveAfterDisc
              << " stacked=" << env.link.stacked
              << " maxLive=" << env.link.maxLive
              << " live_before_second=" << env.link.liveBeforeSecond << "\n";
  }
  {
    Env env(true);
    env.link.done = true;
    CHECK(env.orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(env.orch.helloComplete());
    const int cancelsOnline = env.link.cancels;
    CHECK(env.orch.poll(1, false, 7) == OrchCode::Refused);
    const int cancelsAfterClose = env.link.cancels;
    const int liveAfterClose = env.link.live;
    env.link.done = true;
    const uint64_t due = env.policy.nextAttemptMs();
    CHECK(env.orch.poll(due, true, 0) == OrchCode::Ok);
    // Clock-loss Close must cancel the accepted client before the next start.
    CHECK(cancelsAfterClose > cancelsOnline);
    CHECK(liveAfterClose == 0);
    CHECK(env.link.liveBeforeSecond == 0);
    CHECK(!env.link.stacked);
    CHECK(env.link.maxLive == 1);
    std::cout << "clock_loss_while_online cancels_online=" << cancelsOnline
              << " cancels_after_close=" << cancelsAfterClose
              << " live_after_close=" << liveAfterClose
              << " stacked=" << env.link.stacked
              << " maxLive=" << env.link.maxLive
              << " live_before_second=" << env.link.liveBeforeSecond << "\n";
  }
  if (failures) {
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
    return 1;
  }
  std::cout << "PASS rereview checks=" << checks << " failures=0\n";
  return 0;
}
