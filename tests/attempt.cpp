// SPDX-License-Identifier: MIT
// Synthetic connect-attempt seam. Not an Arduino MQTT adapter test.
#include "cts/NonblockingAttempt.h"
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
  int starts = 0, polls = 0, cancels = 0, live = 0;
  bool progressed = true, done = false, ok = true, failStart = false,
       failPoll = false, stacked = false;
  uint64_t gen = 0;
  cts::AttemptIo start(uint64_t g) noexcept override {
    ++starts;
    gen = g;
    if (failStart)
      return cts::AttemptIo::Fail;
    if (live != 0)
      stacked = true;
    ++live;
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
  void cancel(uint64_t) noexcept override {
    ++cancels;
    if (live > 0)
      --live;
  }
};
int main() {
  using namespace cts;
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(0, 0, 10) == AttemptCode::Fault);
    CHECK(attempt.start(1, 0, 0) == AttemptCode::Fault);
    CHECK(attempt.start(1, 0, 10) == AttemptCode::Started);
    CHECK(attempt.start(2, 0, 10) == AttemptCode::Busy);
    CHECK(attempt.phase() == AttemptPhase::Running);
    link.done = false;
    CHECK(attempt.poll(1, 1) == AttemptCode::Progress);
    CHECK(attempt.progressSteps() == 1);
    CHECK(attempt.poll(1, 2) == AttemptCode::Progress);
    CHECK(attempt.progressSteps() == 2);
    CHECK(attempt.phase() == AttemptPhase::Running);
    link.done = true;
    CHECK(attempt.poll(99, 3) == AttemptCode::Stale);
    CHECK(attempt.phase() == AttemptPhase::Running);
    CHECK(attempt.staleIgnored() == 1);
    CHECK(attempt.cancel(98) == AttemptCode::Stale);
    CHECK(link.cancels == 0);
    CHECK(attempt.phase() == AttemptPhase::Running);
    CHECK(attempt.cancel(1) == AttemptCode::Cancelled);
    CHECK(link.cancels == 1);
    CHECK(attempt.phase() == AttemptPhase::Cancelled);
    CHECK(attempt.poll(1, 4) == AttemptCode::Stale);
    CHECK(attempt.phase() == AttemptPhase::Cancelled);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(7, 100, 10) == AttemptCode::Started);
    CHECK(attempt.poll(7, 109) == AttemptCode::Progress);
    CHECK(attempt.phase() == AttemptPhase::Running);
    link.done = true;
    CHECK(attempt.poll(7, 110) == AttemptCode::TimedOut);
    CHECK(attempt.phase() == AttemptPhase::TimedOut);
    CHECK(link.cancels == 1);
    CHECK(link.polls == 1);
    CHECK(attempt.poll(7, 111) == AttemptCode::Stale);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(3, 0, 50) == AttemptCode::Started);
    CHECK(attempt.poll(3, 1) == AttemptCode::Progress);
    link.done = true;
    link.ok = true;
    CHECK(attempt.poll(3, 2) == AttemptCode::Succeeded);
    CHECK(attempt.phase() == AttemptPhase::Succeeded);
    CHECK(link.cancels == 0);
  }
  {
    Link link;
    link.failStart = true;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(4, 0, 5) == AttemptCode::Failed);
    CHECK(attempt.phase() == AttemptPhase::Failed);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(4, 100, 1000) == AttemptCode::Started);
    CHECK(link.starts == 1);
    CHECK(link.live == 1);
    CHECK(attempt.poll(4, 50) == AttemptCode::Fault);
    CHECK(attempt.phase() == AttemptPhase::Failed);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.cancel(4) == AttemptCode::Stale);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.start(5, 50, 1000) == AttemptCode::Started);
    CHECK(link.starts == 2);
    CHECK(link.live == 1);
    CHECK(!link.stacked);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(4, 0, 1000) == AttemptCode::Started);
    link.failPoll = true;
    CHECK(attempt.poll(4, 1) == AttemptCode::Failed);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.cancel(4) == AttemptCode::Stale);
    CHECK(link.cancels == 1);
    CHECK(attempt.start(5, 1, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(!link.stacked);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(3, 0, 1000) == AttemptCode::Started);
    link.done = true;
    link.ok = false;
    CHECK(attempt.poll(3, 1) == AttemptCode::Failed);
    CHECK(attempt.phase() == AttemptPhase::Failed);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.cancel(3) == AttemptCode::Stale);
    CHECK(link.cancels == 1);
    CHECK(attempt.start(6, 2, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(!link.stacked);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(8, 0, 1000) == AttemptCode::Started);
    link.done = true;
    CHECK(attempt.poll(8, 1) == AttemptCode::Succeeded);
    CHECK(attempt.cancel(8) == AttemptCode::Stale);
    CHECK(link.cancels == 0);
    CHECK(link.live == 1);
    CHECK(attempt.abandon(8) == AttemptCode::Cancelled);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.phase() == AttemptPhase::Cancelled);
    CHECK(attempt.abandon(8) == AttemptCode::Stale);
    CHECK(link.cancels == 1);
    CHECK(attempt.start(9, 2, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(!link.stacked);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(1, 0, 1000) == AttemptCode::Started);
    CHECK(attempt.abandon(1) == AttemptCode::Cancelled);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.phase() == AttemptPhase::Cancelled);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(9, UINT64_MAX - 5, 1000) == AttemptCode::Fault);
    CHECK(attempt.phase() == AttemptPhase::Idle);
    CHECK(link.starts == 0);
    CHECK(link.cancels == 0);
    CHECK(link.live == 0);
    CHECK(attempt.poll(9, UINT64_MAX) == AttemptCode::Stale);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(9, UINT64_MAX - 1000, 1000) == AttemptCode::Started);
    CHECK(link.live == 1);
    CHECK(attempt.poll(9, UINT64_MAX) == AttemptCode::TimedOut);
    CHECK(link.cancels == 1);
    CHECK(link.live == 0);
    CHECK(attempt.phase() == AttemptPhase::TimedOut);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    cts::AttemptDeadlineProbe::arm(attempt, 9, UINT64_MAX - 5, 1000);
    link.live = 1;
    CHECK(attempt.poll(9, UINT64_MAX) == AttemptCode::TimedOut);
    CHECK(link.cancels == 1);
    CHECK(link.polls == 0);
    CHECK(link.live == 0);
    CHECK(attempt.phase() == AttemptPhase::TimedOut);
    CHECK(attempt.poll(9, UINT64_MAX) == AttemptCode::Stale);
    CHECK(link.cancels == 1);
    CHECK(attempt.phase() == AttemptPhase::TimedOut);
  }
  if (failures)
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
  else
    std::cout << "PASS: nonblocking attempt start/partial/cancel/timeout/stale "
                 "checks="
              << checks << " failures=0\n";
  return failures ? 1 : 0;
}
