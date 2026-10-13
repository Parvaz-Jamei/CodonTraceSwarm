// SPDX-License-Identifier: MIT
#pragma once
#include "Command.h"
namespace cts {
struct ClockReading {
  uint64_t utcMs = 0, monotonicMs = 0;
  bool synchronized = false;
};
// Supply an atomic snapshot. monotonicMs must not wrap or reset within a boot.
class ClockProvider {
public:
  virtual ~ClockProvider() = default;
  virtual ClockReading read() noexcept = 0;
};
// Implement using the validated immutable channel registry AND an explicit
// supported-operation allowlist. Called both on admission and before execution.
class CommandPolicy {
public:
  virtual ~CommandPolicy() = default;
  virtual bool permits(const Command &) const noexcept = 0;
};
class CommandExecutor {
public:
  virtual ~CommandExecutor() = default;
  // Synchronous, bounded-duration, nonthrowing; false is terminal, never
  // retried.
  virtual bool execute(const Command &) noexcept = 0;
};
enum class DispatchCode {
  Ok,
  Queued,
  Duplicate,
  Conflict,
  Untrusted,
  Retained,
  NotStarted,
  AlreadyStarted,
  InvalidScope,
  ClockUnavailable,
  ClockFault,
  DecodeRejected,
  Denied,
  QueueFull,
  HistoryFull,
  Empty,
  Completed,
  Busy
};
enum class CommandState {
  None,
  Queued,
  Running,
  Succeeded,
  Failed,
  Expired,
  Denied
};
struct DispatchResult {
  DispatchCode code = DispatchCode::NotStarted;
  CommandState state = CommandState::None;
  CommandError decodeError = CommandError::Ok;
  char commandId[64] = {};
};
// Adapter assertions, NOT authentication performed by this library.
struct DeliveryContext {
  bool authenticated = false;
  bool retained = true;
};
class Dispatcher {
public:
  static constexpr size_t QueueCapacity = 8, HistoryCapacity = 16;
  Dispatcher(ClockProvider &clock, const CommandPolicy &policy)
      : clock_(clock), policy_(policy) {}
  Dispatcher(const Dispatcher &) = delete;
  Dispatcher &operator=(const Dispatcher &) = delete;
  DispatchCode begin(const char *swarm, const char *node, const char *boot);
  DispatchResult submit(const char *, size_t, const DeliveryContext &);
  DispatchResult runOne(CommandExecutor &);
  size_t pending() const { return queued_; }

private:
  struct Entry {
    Command command;
    CommandState state = CommandState::None;
    uint64_t deadline = 0;
  } entries_[HistoryCapacity];
  ClockProvider &clock_;
  const CommandPolicy &policy_;
  CommandDecoder decoder_;
  char swarm_[64] = {}, node_[64] = {}, boot_[64] = {};
  size_t queue_[QueueCapacity] = {}, head_ = 0, queued_ = 0;
  bool started_ = false, seenClock_ = false, fault_ = false, busy_ = false;
  ClockReading previous_;
  DispatchCode now(ClockReading &);
  DispatchResult result(DispatchCode, const Entry * = nullptr) const;
};
} // namespace cts
