// SPDX-License-Identifier: MIT
#include "CommandSession.h"
#include <cstring>
namespace cts {
namespace {
struct Guard {
  bool &flag;
  explicit Guard(bool &f) : flag(f) { flag = true; }
  ~Guard() { flag = false; }
};
SessionResult status(SessionCode c) {
  SessionResult r;
  r.code = c;
  return r;
}
const char *message(CommandState s) {
  switch (s) {
  case CommandState::Succeeded:
    return "succeeded";
  case CommandState::Failed:
    return "execution_failed";
  case CommandState::Expired:
    return "expired_before_execution";
  case CommandState::Denied:
    return "denied_before_execution";
  default:
    return nullptr;
  }
}
} // namespace
CommandSession::~CommandSession() {
  // Destruction abandons any unsent reply; never do this during active
  // delivery.
  if (pending_)
    device_.txHeld_ = false;
}
SessionCode CommandSession::ready() const {
  if (busy_)
    return SessionCode::Busy;
  if (!started_)
    return SessionCode::NotStarted;
  if (!device_.matchesSession(swarm_, node_, boot_))
    return SessionCode::SessionChanged;
  return SessionCode::Ok;
}
SessionResult CommandSession::begin(const SessionConfig &config) {
  if (busy_)
    return status(SessionCode::Busy);
  if (started_)
    return status(SessionCode::AlreadyStarted);
  Guard guard(busy_);
  const auto error = device_.begin(config);
  if (error != Error::Ok) {
    auto r = status(SessionCode::DeviceError);
    r.deviceError = error;
    return r;
  }
  const auto d =
      dispatcher_.begin(config.swarmId, config.nodeId, config.bootId);
  if (d != DispatchCode::Ok) {
    device_.end();
    return status(SessionCode::DeviceError);
  }
  std::strcpy(swarm_, config.swarmId);
  std::strcpy(node_, config.nodeId);
  std::strcpy(boot_, config.bootId);
  started_ = true;
  return status(SessionCode::Ok);
}
SessionResult CommandSession::submit(const char *wire, size_t size,
                                     const DeliveryContext &delivery) {
  const auto check = ready();
  if (check != SessionCode::Ok)
    return status(check);
  Guard guard(busy_);
  auto r = status(SessionCode::Ok);
  r.dispatch = dispatcher_.submit(wire, size, delivery);
  // Duplicate delivery never re-executes and does not mint a fresh reply
  // sequence.
  return r;
}
SessionResult CommandSession::encode() {
  auto r = status(SessionCode::ReplyPending);
  r.dispatch = completion_;
  if (replySize_)
    return r;
  const char *why = message(completion_.state);
  if (!why) {
    r.code = SessionCode::DeviceError;
    r.deviceError = Error::InvalidArgument;
    return r;
  }
  // Private single-threaded serialization; no external callbacks in this
  // section.
  device_.txHeld_ = false;
  replySequence_ = device_.nextSequence();
  const auto error = device_.reply(completion_.commandId,
                                   completion_.state == CommandState::Succeeded,
                                   why, reply_, sizeof(reply_));
  device_.txHeld_ = true;
  if (error != Error::Ok) {
    reply_[0] = 0;
    r.code = SessionCode::DeviceError;
    r.deviceError = error;
    return r;
  }
  replySize_ = std::strlen(reply_);
  return r;
}
SessionResult CommandSession::poll() {
  const auto check = ready();
  if (check != SessionCode::Ok)
    return status(check);
  Guard guard(busy_);
  if (pending_)
    return encode();
  auto r = status(SessionCode::Ok);
  r.dispatch = dispatcher_.runOne(executor_);
  // An executor that ends/restarts the Device cannot send a result in a new
  // scope.
  if (!device_.matchesSession(swarm_, node_, boot_)) {
    r.code = SessionCode::SessionChanged;
    return r;
  }
  if (r.dispatch.code != DispatchCode::Completed)
    return r;
  completion_ = r.dispatch;
  pending_ = true;
  device_.txHeld_ = true;
  return encode();
}
SessionCode CommandSession::confirmReplyPublished(uint64_t sequence) {
  const auto check = ready();
  if (check != SessionCode::Ok)
    return check;
  if (!pending_ || !replySize_)
    return SessionCode::NoReply;
  if (sequence != replySequence_)
    return SessionCode::WrongSequence;
  pending_ = false;
  replySize_ = 0;
  reply_[0] = 0;
  device_.txHeld_ = false;
  return SessionCode::Ok;
}
} // namespace cts
