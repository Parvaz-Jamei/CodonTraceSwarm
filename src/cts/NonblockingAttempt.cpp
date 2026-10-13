// SPDX-License-Identifier: MIT
#include "NonblockingAttempt.h"
namespace cts {
namespace {
void bump(uint64_t &n) noexcept {
  if (n != UINT64_MAX)
    ++n;
}
} // namespace
AttemptCode NonblockingAttempt::start(uint64_t generation, uint64_t now,
                                      uint32_t timeoutMs) noexcept {
  if (!generation || !timeoutMs)
    return AttemptCode::Fault;
  if (phase_ == AttemptPhase::Running)
    return AttemptCode::Busy;
  if (now > UINT64_MAX - static_cast<uint64_t>(timeoutMs))
    return AttemptCode::Fault;
  if (client_.start(generation) != AttemptIo::Ok) {
    phase_ = AttemptPhase::Failed;
    generation_ = generation;
    return AttemptCode::Failed;
  }
  phase_ = AttemptPhase::Running;
  generation_ = generation;
  began_ = now;
  timeoutMs_ = timeoutMs;
  return AttemptCode::Started;
}
AttemptCode NonblockingAttempt::poll(uint64_t generation, uint64_t now) noexcept {
  if (generation != generation_ || phase_ != AttemptPhase::Running) {
    bump(stale_);
    return AttemptCode::Stale;
  }
  if (now < began_) {
    client_.cancel(generation_);
    phase_ = AttemptPhase::Failed;
    return AttemptCode::Fault;
  }
  if (began_ > UINT64_MAX - timeoutMs_) {
    client_.cancel(generation_);
    phase_ = AttemptPhase::TimedOut;
    return AttemptCode::TimedOut;
  }
  if (now - began_ >= timeoutMs_) {
    client_.cancel(generation_);
    phase_ = AttemptPhase::TimedOut;
    return AttemptCode::TimedOut;
  }
  bool progressed = false, done = false, ok = false;
  if (client_.poll(generation_, progressed, done, ok) != AttemptIo::Ok) {
    client_.cancel(generation_);
    phase_ = AttemptPhase::Failed;
    return AttemptCode::Failed;
  }
  if (progressed)
    bump(progress_);
  if (!done)
    return AttemptCode::Progress;
  if (!ok) {
    client_.cancel(generation_);
    phase_ = AttemptPhase::Failed;
    return AttemptCode::Failed;
  }
  phase_ = AttemptPhase::Succeeded;
  return AttemptCode::Succeeded;
}
AttemptCode NonblockingAttempt::cancel(uint64_t generation) noexcept {
  if (generation != generation_ || phase_ != AttemptPhase::Running) {
    bump(stale_);
    return AttemptCode::Stale;
  }
  client_.cancel(generation_);
  phase_ = AttemptPhase::Cancelled;
  return AttemptCode::Cancelled;
}
AttemptCode NonblockingAttempt::abandon(uint64_t generation) noexcept {
  if (generation != generation_ ||
      (phase_ != AttemptPhase::Running && phase_ != AttemptPhase::Succeeded)) {
    bump(stale_);
    return AttemptCode::Stale;
  }
  client_.cancel(generation_);
  phase_ = AttemptPhase::Cancelled;
  return AttemptCode::Cancelled;
}
} // namespace cts
