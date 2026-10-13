// SPDX-License-Identifier: MIT
#pragma once
#include "CommandSession.h"
namespace cts {
// Metadata assertions belong to an authenticated transport adapter, not JSON.
// token identifies one entire publish across fragments; connection is an
// increasing local generation, not the broker's reusable MQTT packet
// identifier.
struct ReceiveFragment {
  const char *topic = nullptr, *data = nullptr;
  size_t topicLength = 0, length = 0, total = 0, offset = 0;
  uint64_t connection = 0, token = 0;
  bool authenticated = false, retainKnown = false, retained = true;
};
enum class InboxCode {
  Busy,
  ScopeMismatch,
  Ok,
  Partial,
  Enqueued,
  Empty,
  Retrying,
  Rejected,
  NotBound,
  AlreadyBound,
  InvalidScope,
  Disconnected,
  WrongConnection,
  Untrusted,
  UnknownRetain,
  Retained,
  WrongTopic,
  InvalidFragment,
  TooLarge,
  Full
};
struct InboxResult {
  InboxCode code = InboxCode::Empty;
  SessionResult session;
};
struct InboxHealth {
  uint64_t fragments = 0, enqueued = 0, drained = 0, retries = 0, rejected = 0,
           abandoned = 0, disconnectDrops = 0, full = 0;
};
// Single owner/event loop, NOT thread-safe. ESP-MQTT task callbacks must use an
// external serialized handoff/lock; do not race
// push/connect/disconnect/pollOne.
class NetworkInbox {
public:
  static constexpr size_t Capacity = 3;
  NetworkInbox() = default;
  NetworkInbox(const NetworkInbox &) = delete;
  NetworkInbox &operator=(const NetworkInbox &) = delete;
  InboxCode bind(const char *swarm, const char *node);
  InboxCode connected(uint64_t generation);
  InboxCode disconnected();
  InboxCode push(const ReceiveFragment &);
  InboxResult pollOne(CommandSession &);
  size_t size() const { return queued_; }
  const char *topic() const { return topic_; }
  const InboxHealth &health() const { return health_; }

private:
  struct Frame {
    char bytes[CommandBytes];
    size_t size = 0;
  } frames_[Capacity];
  char assembly_[CommandBytes], topic_[160] = {}, swarm_[64] = {},
                                node_[64] = {};
  size_t topicLength_ = 0, head_ = 0, queued_ = 0, used_ = 0, total_ = 0;
  uint64_t generation_ = 0, token_ = 0;
  bool busy_ = false, bound_ = false, online_ = false, assembling_ = false;
  InboxHealth health_;
  InboxCode reject(InboxCode, bool abandon);
  void abandon();
};
} // namespace cts
