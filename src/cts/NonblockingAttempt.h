// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace cts {
// Injected connect client. Implementations must not block inside these calls.
// ArduinoMqttAdapter does not implement this interface.
enum class AttemptIo { Ok, Fail };
class ConnectAttemptClient {
public:
  virtual ~ConnectAttemptClient() = default;
  virtual AttemptIo start(uint64_t generation) noexcept = 0;
  // progressed: this call moved the handshake forward but may still be partial.
  // done/ok are meaningful together: done&&ok is success, done&&!ok is failure.
  virtual AttemptIo poll(uint64_t generation, bool &progressed, bool &done,
                         bool &ok) noexcept = 0;
  virtual void cancel(uint64_t generation) noexcept = 0;
};
enum class AttemptPhase {
  Idle,
  Running,
  Succeeded,
  Failed,
  Cancelled,
  TimedOut
};
enum class AttemptCode {
  Started,
  Progress,
  Succeeded,
  Failed,
  Cancelled,
  TimedOut,
  Stale,
  Busy,
  Fault
};
// Single-threaded connect attempt. No allocation, threads, or String.
// A poll/cancel whose generation is not the running attempt is ignored.
// Every failure that leaves Running cancels the client before the phase changes.
// start returns Fault, and does not open a client, when now + timeoutMs cannot
// be represented. poll of an already unrepresentable deadline cancels and
// returns TimedOut. abandon cancels a Running or Succeeded generation and
// leaves that attempt dead; cancel itself still ignores every other phase.
class NonblockingAttempt {
public:
  explicit NonblockingAttempt(ConnectAttemptClient &client) : client_(client) {}
  NonblockingAttempt(const NonblockingAttempt &) = delete;
  NonblockingAttempt &operator=(const NonblockingAttempt &) = delete;
  AttemptCode start(uint64_t generation, uint64_t now, uint32_t timeoutMs) noexcept;
  AttemptCode poll(uint64_t generation, uint64_t now) noexcept;
  AttemptCode cancel(uint64_t generation) noexcept;
  // Cancels the client for this Running or Succeeded generation, then marks
  // the attempt dead so a later start cannot stack a second live client.
  AttemptCode abandon(uint64_t generation) noexcept;
  AttemptPhase phase() const noexcept { return phase_; }
  uint64_t generation() const noexcept { return generation_; }
  uint64_t staleIgnored() const noexcept { return stale_; }
  uint64_t progressSteps() const noexcept { return progress_; }

private:
  friend struct AttemptDeadlineProbe;
  ConnectAttemptClient &client_;
  AttemptPhase phase_ = AttemptPhase::Idle;
  uint64_t generation_ = 0, began_ = 0, timeoutMs_ = 0, stale_ = 0, progress_ = 0;
};
} // namespace cts
