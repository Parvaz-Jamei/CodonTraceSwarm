// SPDX-License-Identifier: MIT
#include "Dispatcher.h"
#include "Json.h"
#include <cstring>
#include <limits>
namespace cts {
namespace {
bool same(const Command &a, const Command &b) {
  return !std::strcmp(a.id, b.id) && !std::strcmp(a.channel, b.channel) &&
         !std::strcmp(a.sourceDigest, b.sourceDigest) &&
         a.operation == b.operation && a.createdMs == b.createdMs &&
         a.expiresMs == b.expiresMs && a.hz == b.hz && a.alpha == b.alpha &&
         a.clip == b.clip && a.eventThreshold == b.eventThreshold;
}
struct BusyGuard {
  bool &flag;
  explicit BusyGuard(bool &f) : flag(f) { flag = true; }
  ~BusyGuard() { flag = false; }
};
} // namespace
DispatchCode Dispatcher::begin(const char *s, const char *n, const char *b) {
  if (busy_)
    return DispatchCode::Busy;
  if (started_)
    return DispatchCode::AlreadyStarted;
  if (!detail::text(s, 63, true) || !detail::text(n, 63, true) ||
      !detail::text(b, 63, true))
    return DispatchCode::InvalidScope;
  std::strcpy(swarm_, s);
  std::strcpy(node_, n);
  std::strcpy(boot_, b);
  started_ = true;
  return DispatchCode::Ok;
}
DispatchResult Dispatcher::result(DispatchCode code, const Entry *e) const {
  DispatchResult r;
  r.code = code;
  if (e) {
    r.state = e->state;
    std::strcpy(r.commandId, e->command.id);
  }
  return r;
}
DispatchCode Dispatcher::now(ClockReading &r) {
  if (fault_)
    return DispatchCode::ClockFault;
  r = clock_.read();
  if (!r.synchronized || r.utcMs == 0 || r.utcMs > MaxWireInteger)
    return DispatchCode::ClockUnavailable;
  if (seenClock_ &&
      (r.utcMs < previous_.utcMs || r.monotonicMs < previous_.monotonicMs)) {
    fault_ = true;
    return DispatchCode::ClockFault;
  }
  previous_ = r;
  seenClock_ = true;
  return DispatchCode::Ok;
}
DispatchResult Dispatcher::submit(const char *wire, size_t length,
                                  const DeliveryContext &delivery) {
  if (busy_)
    return result(DispatchCode::Busy);
  BusyGuard guard(busy_);
  if (!started_)
    return result(DispatchCode::NotStarted);
  if (!delivery.authenticated)
    return result(DispatchCode::Untrusted);
  if (delivery.retained)
    return result(DispatchCode::Retained);
  ClockReading t;
  const auto clockResult = now(t);
  if (clockResult != DispatchCode::Ok)
    return result(clockResult);
  CommandContext context;
  context.swarmId = swarm_;
  context.nodeId = node_;
  context.bootId = boot_;
  context.nowMs = t.utcMs;
  Command command;
  const auto error = decoder_.decode(wire, length, context, command);
  if (error != CommandError::Ok) {
    auto r = result(DispatchCode::DecodeRejected);
    r.decodeError = error;
    return r;
  }
  size_t free = HistoryCapacity;
  for (size_t i = 0; i < HistoryCapacity; ++i) {
    auto &e = entries_[i];
    // Never evict live deduplication records just to admit more traffic.
    if (e.state != CommandState::None && e.state != CommandState::Queued &&
        e.state != CommandState::Running && t.utcMs >= e.command.expiresMs &&
        t.monotonicMs >= e.deadline)
      e = Entry{};
    if (e.state == CommandState::None) {
      if (free == HistoryCapacity)
        free = i;
    } else if (!std::strcmp(e.command.id, command.id))
      return result(same(e.command, command) ? DispatchCode::Duplicate
                                             : DispatchCode::Conflict,
                    &e);
  }
  if (!policy_.permits(command))
    return result(DispatchCode::Denied);
  if (queued_ == QueueCapacity)
    return result(DispatchCode::QueueFull);
  if (free == HistoryCapacity)
    return result(DispatchCode::HistoryFull);
  const uint64_t remaining = command.expiresMs - t.utcMs;
  if (t.monotonicMs > std::numeric_limits<uint64_t>::max() - remaining) {
    fault_ = true;
    return result(DispatchCode::ClockFault);
  }
  auto &e = entries_[free];
  e.command = command;
  e.deadline = t.monotonicMs + remaining;
  e.state = CommandState::Queued;
  queue_[(head_ + queued_) % QueueCapacity] = free;
  ++queued_;
  return result(DispatchCode::Queued, &e);
}
DispatchResult Dispatcher::runOne(CommandExecutor &executor) {
  if (busy_)
    return result(DispatchCode::Busy);
  BusyGuard guard(busy_);
  if (!started_)
    return result(DispatchCode::NotStarted);
  if (!queued_)
    return result(DispatchCode::Empty);
  ClockReading t;
  const auto clockResult = now(t);
  if (clockResult != DispatchCode::Ok)
    return result(clockResult);
  auto &e = entries_[queue_[head_]];
  const bool permitted = policy_.permits(e.command);
  const auto recheck = now(t);
  if (recheck != DispatchCode::Ok)
    return result(recheck);
  head_ = (head_ + 1) % QueueCapacity;
  --queued_;
  if (t.utcMs >= e.command.expiresMs || t.monotonicMs >= e.deadline)
    e.state = CommandState::Expired;
  else if (!permitted)
    e.state = CommandState::Denied;
  else {
    e.state = CommandState::Running;
    e.state = executor.execute(e.command) ? CommandState::Succeeded
                                          : CommandState::Failed;
  }
  return result(DispatchCode::Completed, &e);
}
} // namespace cts
