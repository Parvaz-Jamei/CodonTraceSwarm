// SPDX-License-Identifier: MIT
#pragma once
#include "Dispatcher.h"
namespace cts {
// Reads the exact metadata and grants owned by Device; no parallel registry.
class RegistryPolicy final : public CommandPolicy {
public:
  explicit RegistryPolicy(const Device &device) : device_(device) {}
  bool permits(const Command &c) const noexcept override {
    return device_.commandAllowed(c.channel, c.operation, c.hz);
  }

private:
  const Device &device_;
};
enum class SessionCode {
  Ok,
  NotStarted,
  AlreadyStarted,
  SessionChanged,
  Busy,
  ReplyPending,
  NoReply,
  WrongSequence,
  DeviceError
};
struct SessionResult {
  SessionCode code = SessionCode::Ok;
  Error deviceError = Error::Ok;
  DispatchResult dispatch;
};
// One session owner per Device, one event loop. All referenced adapters outlive
// this object. No network calls. A pending reply blocks further serialization
// through Device, preserving its shared sequence until ordered publication.
class CommandSession {
public:
  static constexpr size_t ReplyBytes = 1024;
  CommandSession(Device &device, ClockProvider &clock,
                 CommandExecutor &executor)
      : device_(device), policy_(device), dispatcher_(clock, policy_),
        executor_(executor) {}
  ~CommandSession();
  CommandSession(const CommandSession &) = delete;
  CommandSession &operator=(const CommandSession &) = delete;
  SessionResult begin(const SessionConfig &);
  SessionResult submit(const char *, size_t, const DeliveryContext &);
  SessionResult poll();
  const char *replyData() const { return reply_; }
  size_t replySize() const { return replySize_; }
  uint64_t replySequence() const { return replySequence_; }
  // Invoke only after the matching bytes were accepted by the ordered
  // transport. This does NOT assert host acceptance; expired host commands can
  // reject replies.
  SessionCode confirmReplyPublished(uint64_t expectedSequence);
  size_t pendingCommands() const { return dispatcher_.pending(); }

private:
  Device &device_;
  RegistryPolicy policy_;
  Dispatcher dispatcher_;
  CommandExecutor &executor_;
  bool started_ = false, busy_ = false, pending_ = false;
  char swarm_[64] = {}, node_[64] = {}, boot_[64] = {};
  char reply_[ReplyBytes] = {};
  size_t replySize_ = 0;
  uint64_t replySequence_ = 0;
  DispatchResult completion_;
  SessionCode ready() const;
  SessionResult encode();
};
} // namespace cts
