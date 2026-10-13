// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace cts {
struct ReconnectConfig {
  uint32_t initialMs = 1000, maximumMs = 60000, timeoutMs = 10000,
           stableMs = 30000;
};
enum class LinkState { Stopped, Waiting, Connecting, Online, Fault };
enum class LinkAction { None, Connect, Close };
struct LinkStep {
  LinkAction action = LinkAction::None;
  uint64_t token = 0;
};
struct ReconnectHealth {
  uint64_t attempts = 0, failures = 0, timeouts = 0, clockBlocks = 0,
           staleEvents = 0;
};
// Single-owner policy only: no sockets, sleeps, RNG, TLS, or task synchronization.
// An adapter must close/cancel sockets on Close and implement nonblocking connect.
class ReconnectPolicy {
public:
  explicit ReconnectPolicy(const ReconnectConfig &c = ReconnectConfig()) : config_(c) {
    if (!c.initialMs || c.maximumMs < c.initialMs || !c.timeoutMs || !c.stableMs)
      state_ = LinkState::Fault;
    cap_ = c.initialMs;
  }
  bool start(uint64_t now) noexcept {
    if (state_ != LinkState::Stopped || !observe(now)) return false;
    state_ = LinkState::Waiting; due_ = now; cap_ = config_.initialMs;
    return true;
  }
  // Caller closes any socket before stop. Token is never reused on restart.
  void stop() noexcept { if (state_ != LinkState::Fault) state_ = LinkState::Stopped; }
  LinkStep poll(uint64_t now, bool clockReady, uint32_t jitter) noexcept {
    LinkStep out; out.token = token_;
    if (state_ == LinkState::Stopped || state_ == LinkState::Fault) return out;
    if (!observe(now)) { out.action = LinkAction::Close; return out; }
    if (state_ == LinkState::Connecting && now - began_ >= config_.timeoutMs) {
      bump(health_.timeouts); retry(now, jitter); out.action = LinkAction::Close; return out;
    }
    if (!clockReady) {
      bump(health_.clockBlocks);
      if (state_ == LinkState::Online || state_ == LinkState::Connecting) {
        retry(now, jitter); out.action = LinkAction::Close;
      }
      return out;
    }
    if (state_ == LinkState::Online) {
      if (now - onlineAt_ >= config_.stableMs) cap_ = config_.initialMs;
      return out;
    }
    if (state_ == LinkState::Waiting && now >= due_) {
      if (token_ == UINT64_MAX) { state_ = LinkState::Fault; return out; }
      ++token_; began_ = now; state_ = LinkState::Connecting; bump(health_.attempts);
      out.action = LinkAction::Connect; out.token = token_;
    }
    return out;
  }
  // Returns false for a late/untrusted-clock event; caller closes that attempt's socket.
  bool connected(uint64_t token, uint64_t now, bool clockReady, uint32_t jitter) noexcept {
    if (!current(token, LinkState::Connecting)) return false;
    if (!observe(now)) return false;
    if (!clockReady || now - began_ >= config_.timeoutMs) {
      if (!clockReady) bump(health_.clockBlocks); else bump(health_.timeouts);
      retry(now,jitter); return false;
    }
    state_ = LinkState::Online; onlineAt_ = now; return true;
  }
  // Transport failed/disconnected; it must already be closed. Events from old tokens ignored.
  bool lost(uint64_t token, uint64_t now, uint32_t jitter) noexcept {
    if (token != token_ || (state_ != LinkState::Connecting && state_ != LinkState::Online)) {
      bump(health_.staleEvents); return false;
    }
    if (!observe(now)) return false;
    if (state_ == LinkState::Online && now - onlineAt_ >= config_.stableMs)
      cap_ = config_.initialMs;
    retry(now,jitter); return true;
  }
  LinkState state() const noexcept { return state_; }
  uint64_t nextAttemptMs() const noexcept { return due_; }
  uint32_t nextDelayCapMs() const noexcept { return cap_; }
  uint64_t token() const noexcept { return token_; }
  const ReconnectHealth &health() const noexcept { return health_; }
private:
  static void bump(uint64_t &v) noexcept { if (v != UINT64_MAX) ++v; }
  bool current(uint64_t t, LinkState s) noexcept {
    if (t == token_ && state_ == s) return true;
    bump(health_.staleEvents); return false;
  }
  bool observe(uint64_t now) noexcept {
    if (seen_ && now < last_) { state_ = LinkState::Fault; return false; }
    last_ = now; seen_ = true; return true;
  }
  void retry(uint64_t now, uint32_t jitter) noexcept {
    bump(health_.failures);
    // Equal-jitter window [ceil(cap/2), cap]; supplied sample maps monotonically.
    const uint32_t low = cap_/2 + cap_%2;
    const uint64_t width = static_cast<uint64_t>(cap_ - low) + 1;
    const uint64_t delay = low + ((static_cast<uint64_t>(jitter)*width) >> 32);
    if (UINT64_MAX - now < delay) { state_ = LinkState::Fault; return; }
    due_ = now + delay;
    cap_ = cap_ > config_.maximumMs/2 ? config_.maximumMs : cap_*2;
    state_ = LinkState::Waiting;
  }
  ReconnectConfig config_;
  ReconnectHealth health_;
  LinkState state_ = LinkState::Stopped;
  uint64_t last_ = 0, due_ = 0, began_ = 0, onlineAt_ = 0, token_ = 0;
  uint32_t cap_ = 0;
  bool seen_ = false;
};
}
