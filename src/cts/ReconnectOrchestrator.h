// SPDX-License-Identifier: MIT
#pragma once
#include "NetworkInbox.h"
#include "NonblockingAttempt.h"
#include "ReconnectPolicy.h"
namespace cts {
// QoS1 publisher driven one step at a time. Not implemented by ArduinoMqttAdapter.
// step must not call confirmReplyPublished. False means the attempt failed.
// qos1Complete is ignored unless writeComplete is true and every byte was accepted.
class QosPublisher {
public:
  virtual ~QosPublisher() = default;
  virtual bool step(uint64_t generation, const char *data, size_t size,
                    size_t &accepted, bool &writeComplete,
                    bool &qos1Complete) noexcept = 0;
  virtual void cancel(uint64_t generation) noexcept = 0;
};
enum class OrchCode {
  Ok,
  Refused,
  Busy,
  Failed,
  Lost,
  NotConnected,
  HelloRequired,
  InvalidArgument,
  Stale,
  Fault
};
struct OrchHealth {
  uint64_t losses = 0, publishFailures = 0, hellos = 0, readings = 0,
           repliesConfirmed = 0, connectRefusals = 0;
};
// Single-threaded reconnect owner. No threads, String, or dynamic allocation.
// On each new connection generation a fresh Device hello is serialized before
// any reading. repliesConfirmed / confirmReplyPublished means the publisher
// reported a full write and QoS1 completion (transport ack), not that the
// Genesis process accepted the payload.
// PUBACK is not Genesis application acceptance.
// A disconnect during an in-flight publish counts as one loss and does not count as delivery. Dropping a staged
// Hello or Reading retry counts as one loss, and does not count a second time
// if aborting the in-flight publish already counted it. clockReady false never
// starts a connect attempt.
// A handshake the inbox or policy refuses is abandoned: that generation's
// client is cancelled before policy.lost, including after Succeeded.
// Once Device::reading has serialized, those exact bytes are retried. The
// caller is told Busy, not Failed, and readingsAllowed() is false while that
// retry is queued. Failed means those bytes will not be published later.
// messageStartedMs_ is the first attempt of that Hello or Reading. A retry
// does not move it. Before restarting the retry, if
// now - messageStartedMs_ >= 2 * attemptTimeoutMs, startKind is not called,
// the retry is cleared, and exactly one loss is counted. A later poll does
// not count a second loss for that dropped message. A dropped Reading leaves
// the link up; readingsAllowed() is then true when hello is already complete.
// A dropped Hello closes this connection via onDisconnected(generation, now, 0)
// after that loss and after retrySame_ is cleared, so the close does not count
// a second loss. readingsAllowed() stays false until a later connection
// completes a new hello. Policy backoff runs first; the same poll does not
// reconnect. The same serialized hello is not started again on this connection.
// A pending command reply is not a reading and is not dropped by this budget.
// accepted from the publisher is cumulative. A publish that makes no forward
// progress for attemptTimeoutMs is cancelled once, counts one loss, and is
// left inactive. A publish is also cancelled once, with one loss, when it has
// stayed active for 2 * attemptTimeoutMs since publishStartedMs_. That stamp
// does not move when accepted increases. Neither cancel is host acceptance.
// ArduinoMqttAdapter is a different, synchronous backend and is not used here.
class ReconnectOrchestrator {
public:
  ReconnectOrchestrator(Device &device, CommandSession &session,
                        NetworkInbox &inbox, ReconnectPolicy &policy,
                        ConnectAttemptClient &connect, QosPublisher &publisher,
                        uint32_t attemptTimeoutMs = 10000)
      : device_(device), session_(session), inbox_(inbox), policy_(policy),
        attempt_(connect), publisher_(publisher),
        attemptTimeoutMs_(attemptTimeoutMs) {}
  ReconnectOrchestrator(const ReconnectOrchestrator &) = delete;
  ReconnectOrchestrator &operator=(const ReconnectOrchestrator &) = delete;
  bool start(uint64_t now) noexcept;
  OrchCode poll(uint64_t now, bool clockReady, uint32_t jitter) noexcept;
  OrchCode publishReading(const char *channel, const Sample &, char *scratch,
                          size_t capacity) noexcept;
  OrchCode onDisconnected(uint64_t token, uint64_t now, uint32_t jitter) noexcept;
  bool readingsAllowed() const noexcept {
    return online_ && helloComplete_ && !session_.replySize() && !publishActive_ &&
           !retrySame_;
  }
  bool helloComplete() const noexcept { return helloComplete_; }
  bool publishActive() const noexcept { return publishActive_; }
  uint64_t generation() const noexcept { return online_ ? generation_ : 0; }
  uint64_t losses() const noexcept { return health_.losses; }
  uint64_t connectRefusals() const noexcept { return health_.connectRefusals; }
  const OrchHealth &health() const noexcept { return health_; }

private:
  enum class OutKind { None, Reply, Hello, Reading };
  Device &device_;
  CommandSession &session_;
  NetworkInbox &inbox_;
  ReconnectPolicy &policy_;
  NonblockingAttempt attempt_;
  QosPublisher &publisher_;
  uint32_t attemptTimeoutMs_;
  OrchHealth health_;
  char wire_[TxBytes] = {};
  size_t size_ = 0, accepted_ = 0;
  uint64_t generation_ = 0, replySeq_ = 0, progressMs_ = 0, publishStartedMs_ = 0,
           messageStartedMs_ = 0;
  OutKind kind_ = OutKind::None;
  OutKind retryKind_ = OutKind::None;
  bool online_ = false, helloSerialized_ = false, helloComplete_ = false,
       publishActive_ = false, replyFault_ = false, retrySame_ = false,
       helloAbandoned_ = false;
  static void bump(uint64_t &) noexcept;
  void clearOnline() noexcept;
  void abortPublish(bool loss) noexcept;
  bool stepPublish() noexcept;
  void finishPublish() noexcept;
  bool startKind(OutKind, uint64_t now) noexcept;
  void startReply(uint64_t now) noexcept;
  void service(uint64_t now) noexcept;
  void handleClose() noexcept;
  void driveAttempt(uint64_t now, bool clockReady, uint32_t jitter) noexcept;
};
} // namespace cts
