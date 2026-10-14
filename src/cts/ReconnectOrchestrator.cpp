// SPDX-License-Identifier: MIT
#include "ReconnectOrchestrator.h"
#include <cstring>
namespace cts {
void ReconnectOrchestrator::bump(uint64_t &n) noexcept {
  if (n != UINT64_MAX)
    ++n;
}
bool ReconnectOrchestrator::start(uint64_t now) noexcept {
  if (!attemptTimeoutMs_)
    return false;
  return policy_.start(now);
}
void ReconnectOrchestrator::clearOnline() noexcept {
  // Staged Hello/Reading bytes are already owned. Dropping them is one loss.
  // abortPublish clears retrySame_ first when it has counted the in-flight copy.
  if (retrySame_)
    bump(health_.losses);
  online_ = false;
  helloComplete_ = false;
  helloSerialized_ = false;
  helloAbandoned_ = false;
  generation_ = 0;
  kind_ = OutKind::None;
  retryKind_ = OutKind::None;
  publishActive_ = false;
  retrySame_ = false;
  messageStartedMs_ = 0;
  size_ = 0;
  accepted_ = 0;
}
void ReconnectOrchestrator::abortPublish(bool loss) noexcept {
  if (!publishActive_)
    return;
  publisher_.cancel(generation_);
  publishActive_ = false;
  kind_ = OutKind::None;
  retryKind_ = OutKind::None;
  retrySame_ = false;
  if (loss)
    bump(health_.losses);
}
void ReconnectOrchestrator::finishPublish() noexcept {
  if (kind_ == OutKind::Reply) {
    if (session_.confirmReplyPublished(replySeq_) == SessionCode::Ok)
      bump(health_.repliesConfirmed);
    else
      replyFault_ = true;
  } else if (kind_ == OutKind::Hello) {
    helloComplete_ = true;
    bump(health_.hellos);
  } else if (kind_ == OutKind::Reading)
    bump(health_.readings);
  publishActive_ = false;
  retrySame_ = false;
  kind_ = OutKind::None;
}
bool ReconnectOrchestrator::stepPublish() noexcept {
  size_t accepted = 0;
  bool writeComplete = false, qos1 = false;
  const bool ok = publisher_.step(generation_, wire_, size_, accepted,
                                  writeComplete, qos1);
  if (!ok || accepted > size_ || accepted < accepted_ ||
      (writeComplete && accepted != size_) || (qos1 && !writeComplete)) {
    if (kind_ == OutKind::Hello || kind_ == OutKind::Reading) {
      retryKind_ = kind_;
      retrySame_ = true;
    }
    publisher_.cancel(generation_);
    publishActive_ = false;
    kind_ = OutKind::None;
    bump(health_.publishFailures);
    return false;
  }
  accepted_ = accepted;
  if (writeComplete && qos1)
    finishPublish();
  return true;
}
bool ReconnectOrchestrator::startKind(OutKind kind, uint64_t now) noexcept {
  // First Hello or Reading attempt only. A retry must not move this stamp.
  if ((kind == OutKind::Hello || kind == OutKind::Reading) && !retrySame_)
    messageStartedMs_ = now;
  retrySame_ = false;
  kind_ = kind;
  publishActive_ = true;
  accepted_ = 0;
  progressMs_ = now;
  publishStartedMs_ = now;
  return stepPublish();
}
void ReconnectOrchestrator::startReply(uint64_t now) noexcept {
  const size_t n = session_.replySize();
  if (!n || n >= sizeof(wire_))
    return;
  std::memcpy(wire_, session_.replyData(), n);
  wire_[n] = 0;
  size_ = n;
  replySeq_ = session_.replySequence();
  startKind(OutKind::Reply, now);
}
void ReconnectOrchestrator::service(uint64_t now) noexcept {
  if (!online_)
    return;
  if (publishActive_) {
    const size_t before = accepted_;
    stepPublish();
    if (!publishActive_)
      return;
    if (accepted_ > before)
      progressMs_ = now;
    // publishStartedMs_ stays at activation. Forward progress does not extend it.
    const uint64_t totalMs = static_cast<uint64_t>(attemptTimeoutMs_) * 2u;
    const bool totalDue = now >= publishStartedMs_ &&
                          now - publishStartedMs_ >= totalMs;
    const bool quietDue =
        accepted_ == before && now >= progressMs_ &&
        now - progressMs_ >= static_cast<uint64_t>(attemptTimeoutMs_);
    if (totalDue || quietDue)
      abortPublish(true);
    return;
  }
  if (retrySame_) {
    // Message lifetime is not the lifetime of one attempt. Replies never set
    // retrySame_, so this budget does not drop a pending command reply.
    const uint64_t budget = static_cast<uint64_t>(attemptTimeoutMs_) * 2u;
    if (now >= messageStartedMs_ && now - messageStartedMs_ >= budget) {
      const OutKind dropped = retryKind_;
      retrySame_ = false;
      retryKind_ = OutKind::None;
      bump(health_.losses);
      // Hello only. retrySame_ is already false and publish is inactive, so
      // onDisconnected's clearOnline must not count a second loss. A Reading
      // drop does not close. Jitter 0 lets policy_.lost schedule backoff;
      // this poll already passed Connect, so it does not reconnect.
      if (dropped == OutKind::Hello && !retrySame_ && !publishActive_)
        onDisconnected(generation_, now, 0);
      return;
    }
    startKind(retryKind_, now);
    return;
  }
  if (!helloComplete_) {
    // Budget already counted this hello once. Do not restart it and count again.
    // A pending reply is not part of that budget and is still published.
    if (helloAbandoned_) {
      if (session_.replySize() && !replyFault_)
        startReply(now);
      return;
    }
    if (!helloSerialized_) {
      if (session_.replySize() && !replyFault_) {
        startReply(now);
        return;
      }
      if (device_.hello(wire_, sizeof(wire_)) != Error::Ok)
        return;
      helloSerialized_ = true;
      size_ = std::strlen(wire_);
    }
    startKind(OutKind::Hello, now);
    return;
  }
  if (session_.replySize() && !replyFault_)
    startReply(now);
}
void ReconnectOrchestrator::handleClose() noexcept {
  abortPublish(true);
  const auto phase = attempt_.phase();
  if (phase == AttemptPhase::Running || phase == AttemptPhase::Succeeded)
    attempt_.abandon(attempt_.generation());
  inbox_.disconnected();
  clearOnline();
}
void ReconnectOrchestrator::driveAttempt(uint64_t now, bool clockReady,
                                        uint32_t jitter) noexcept {
  if (attempt_.phase() != AttemptPhase::Running)
    return;
  const uint64_t gen = attempt_.generation();
  const auto code = attempt_.poll(gen, now);
  if (code == AttemptCode::Stale || code == AttemptCode::Progress ||
      code == AttemptCode::Started)
    return;
  if (code == AttemptCode::Succeeded) {
    if (!clockReady || !policy_.connected(gen, now, clockReady, jitter) ||
        inbox_.connected(gen) != InboxCode::Ok) {
      attempt_.abandon(gen);
      if (policy_.state() == LinkState::Online ||
          policy_.state() == LinkState::Connecting)
        policy_.lost(gen, now, jitter);
      inbox_.disconnected();
      clearOnline();
      return;
    }
    online_ = true;
    generation_ = gen;
    helloComplete_ = false;
    helloSerialized_ = false;
    helloAbandoned_ = false;
    replyFault_ = false;
    return;
  }
  if (policy_.state() == LinkState::Connecting)
    policy_.lost(gen, now, jitter);
}
OrchCode ReconnectOrchestrator::poll(uint64_t now, bool clockReady,
                                    uint32_t jitter) noexcept {
  if (!attemptTimeoutMs_ || policy_.state() == LinkState::Fault)
    return OrchCode::Fault;
  if (!clockReady)
    bump(health_.connectRefusals);
  const LinkStep step = policy_.poll(now, clockReady, jitter);
  if (step.action == LinkAction::Close)
    handleClose();
  else if (step.action == LinkAction::Connect &&
           policy_.state() != LinkState::Fault) {
    if (!clockReady)
      bump(health_.connectRefusals);
    else {
      const auto started = attempt_.start(step.token, now, attemptTimeoutMs_);
      if (started == AttemptCode::Failed || started == AttemptCode::Fault)
        policy_.lost(step.token, now, jitter);
    }
  }
  if (policy_.state() != LinkState::Fault) {
    driveAttempt(now, clockReady, jitter);
    if (online_)
      service(now);
    if (!publishActive_)
      progressMs_ = now;
  }
  if (policy_.state() == LinkState::Fault)
    return OrchCode::Fault;
  return clockReady ? OrchCode::Ok : OrchCode::Refused;
}
OrchCode ReconnectOrchestrator::publishReading(const char *channel,
                                              const Sample &sample, char *scratch,
                                              size_t capacity) noexcept {
  if (!channel || !scratch)
    return OrchCode::InvalidArgument;
  if (!online_)
    return OrchCode::NotConnected;
  if (!helloComplete_)
    return OrchCode::HelloRequired;
  if (retrySame_ || session_.replySize() || publishActive_)
    return OrchCode::Busy;
  const auto error = device_.reading(channel, sample, scratch, capacity);
  if (error != Error::Ok)
    return OrchCode::Failed;
  const size_t n = std::strlen(scratch);
  if (!n || n >= sizeof(wire_))
    return OrchCode::Failed;
  std::memcpy(wire_, scratch, n + 1);
  size_ = n;
  if (startKind(OutKind::Reading, progressMs_))
    return OrchCode::Ok;
  // Bytes are already on the retry queue. Failed would let the caller believe
  // they will not be published.
  return retrySame_ ? OrchCode::Busy : OrchCode::Failed;
}
OrchCode ReconnectOrchestrator::onDisconnected(uint64_t token, uint64_t now,
                                              uint32_t jitter) noexcept {
  const bool livePublish = publishActive_ && token == generation_ && online_;
  const auto phase = attempt_.phase();
  const bool liveAttempt =
      (phase == AttemptPhase::Running || phase == AttemptPhase::Succeeded) &&
      token == attempt_.generation();
  const bool liveOnline = online_ && token == generation_;
  if (!livePublish && !liveAttempt && !liveOnline) {
    policy_.lost(token, now, jitter);
    return OrchCode::Stale;
  }
  if (livePublish)
    abortPublish(true);
  if (liveAttempt)
    attempt_.abandon(token);
  policy_.lost(token, now, jitter);
  inbox_.disconnected();
  clearOnline();
  return livePublish ? OrchCode::Lost : OrchCode::Ok;
}
} // namespace cts
