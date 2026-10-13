// SPDX-License-Identifier: MIT
#include "NetworkInbox.h"
#include "Json.h"
#include <cstring>
#include <limits>
namespace cts {
namespace {
void bump(uint64_t &n) {
  if (n < std::numeric_limits<uint64_t>::max())
    ++n;
}
} // namespace
void NetworkInbox::abandon() {
  if (assembling_)
    bump(health_.abandoned);
  assembling_ = false;
  used_ = total_ = 0;
}
InboxCode NetworkInbox::reject(InboxCode c, bool discard) {
  bump(health_.rejected);
  if (discard)
    abandon();
  return c;
}
InboxCode NetworkInbox::bind(const char *s, const char *n) {
  if (busy_)
    return InboxCode::Busy;
  if (bound_)
    return InboxCode::AlreadyBound;
  if (!detail::text(s, 63, true) || !detail::text(n, 63, true))
    return InboxCode::InvalidScope;
  std::strcpy(topic_, "genesis/swarm/");
  std::strcat(topic_, s);
  std::strcat(topic_, "/");
  std::strcat(topic_, n);
  std::strcat(topic_, "/down");
  topicLength_ = std::strlen(topic_);
  std::strcpy(swarm_, s);
  std::strcpy(node_, n);
  bound_ = true;
  return InboxCode::Ok;
}
InboxCode NetworkInbox::connected(uint64_t generation) {
  if (busy_)
    return InboxCode::Busy;
  if (!bound_)
    return InboxCode::NotBound;
  if (!generation || generation <= generation_)
    return InboxCode::WrongConnection;
  disconnected();
  generation_ = generation;
  online_ = true;
  return InboxCode::Ok;
}
InboxCode NetworkInbox::disconnected() {
  if (busy_)
    return InboxCode::Busy;
  for (size_t i = 0; i < queued_; ++i)
    bump(health_.disconnectDrops);
  if (assembling_)
    bump(health_.disconnectDrops);
  abandon();
  queued_ = head_ = 0;
  online_ = false;
  return InboxCode::Ok;
}
InboxCode NetworkInbox::push(const ReceiveFragment &f) {
  if (busy_)
    return InboxCode::Busy;
  bump(health_.fragments);
  if (!bound_)
    return reject(InboxCode::NotBound, false);
  if (!online_)
    return reject(InboxCode::Disconnected, false);
  if (f.connection != generation_)
    return reject(InboxCode::WrongConnection, false);
  if (!f.authenticated)
    return reject(InboxCode::Untrusted, true);
  if (!f.retainKnown)
    return reject(InboxCode::UnknownRetain, true);
  if (f.retained)
    return reject(InboxCode::Retained, true);
  if (f.total > CommandBytes)
    return reject(InboxCode::TooLarge, true);
  if (!f.token || !f.data || !f.total || !f.length || f.offset > f.total ||
      f.length > f.total - f.offset)
    return reject(InboxCode::InvalidFragment, true);
  if ((!f.offset || f.topic || f.topicLength) &&
      (!f.topic || f.topicLength != topicLength_ ||
       std::memcmp(f.topic, topic_, topicLength_)))
    return reject(InboxCode::WrongTopic, true);
  if (!f.offset) {
    abandon();
    if (queued_ == Capacity) {
      bump(health_.full);
      return reject(InboxCode::Full, false);
    }
    assembling_ = true;
    token_ = f.token;
    total_ = f.total;
    used_ = 0;
  } else if (!assembling_ || f.token != token_ || f.total != total_ ||
             f.offset != used_)
    return reject(InboxCode::InvalidFragment, true);
  std::memcpy(assembly_ + used_, f.data, f.length);
  used_ += f.length;
  if (used_ != total_)
    return InboxCode::Partial;
  auto &frame = frames_[(head_ + queued_) % Capacity];
  frame.size = used_;
  std::memcpy(frame.bytes, assembly_, used_);
  ++queued_;
  bump(health_.enqueued);
  assembling_ = false;
  used_ = total_ = 0;
  return InboxCode::Enqueued;
}
InboxResult NetworkInbox::pollOne(CommandSession &session) {
  InboxResult r;
  if (busy_) {
    r.code = InboxCode::Busy;
    return r;
  }
  if (!bound_) {
    r.code = InboxCode::NotBound;
    return r;
  }
  if (!online_) {
    r.code = InboxCode::Disconnected;
    return r;
  }
  if (!queued_)
    return r;
  if (!session.matchesRoute(swarm_, node_)) {
    r.code = InboxCode::ScopeMismatch;
    return r;
  }
  struct Guard {
    bool &b;
    explicit Guard(bool &flag) : b(flag) { b = true; }
    ~Guard() { b = false; }
  } guard(busy_);
  const auto &frame = frames_[head_];
  DeliveryContext d;
  d.authenticated = true;
  d.retained = false;
  r.session = session.submit(frame.bytes, frame.size, d);
  const auto sc = r.session.code;
  const auto dc = r.session.dispatch.code;
  if (sc == SessionCode::Busy || sc == SessionCode::NotStarted ||
      (sc == SessionCode::Ok &&
       (dc == DispatchCode::ClockUnavailable || dc == DispatchCode::QueueFull ||
        dc == DispatchCode::HistoryFull || dc == DispatchCode::Busy))) {
    bump(health_.retries);
    r.code = InboxCode::Retrying;
    return r;
  }
  head_ = (head_ + 1) % Capacity;
  --queued_;
  bump(health_.drained);
  r.code = (sc == SessionCode::Ok &&
            (dc == DispatchCode::Queued || dc == DispatchCode::Duplicate))
               ? InboxCode::Ok
               : InboxCode::Rejected;
  return r;
}
} // namespace cts
