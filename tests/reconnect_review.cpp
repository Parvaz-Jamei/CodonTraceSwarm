// SPDX-License-Identifier: MIT
// Independent CP09 adversarial tests; synthetic time/events, no network or MCU.
#include "cts/ReconnectPolicy.h"
#include "cts/Millis32.h"
#include <iostream>
using namespace cts;
static unsigned checks = 0, failures = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; std::cerr << __LINE__ << ": " << #expr << '\n'; } } while (false)

int main() {
  ReconnectConfig cfg;
  cfg.initialMs = 5; cfg.maximumMs = 11; cfg.timeoutMs = 7; cfg.stableMs = 10;
  {
    ReconnectPolicy p(cfg);
    CHECK(p.state() == LinkState::Stopped);
    CHECK(p.poll(99, true, 0).action == LinkAction::None);
    CHECK(p.start(100));
    CHECK(!p.start(100));
    CHECK(p.poll(100, false, 0).action == LinkAction::None);
    CHECK(p.health().attempts == 0);
    CHECK(p.health().clockBlocks == 1);
    LinkStep s = p.poll(101, true, 0);
    CHECK(s.action == LinkAction::Connect && s.token == 1);
    CHECK(p.poll(107, true, 0).action == LinkAction::None);
    // Timeout is inclusive, including a success callback at the exact boundary.
    CHECK(!p.connected(s.token, 108, true, 0));
    CHECK(p.state() == LinkState::Waiting);
    CHECK(p.health().timeouts == 1);
    CHECK(p.nextAttemptMs() == 111);
    CHECK(p.nextDelayCapMs() == 10);
    CHECK(p.poll(110, true, 0).action == LinkAction::None);
    s = p.poll(111, true, 0);
    CHECK(s.action == LinkAction::Connect && s.token == 2);
    // Old callback's old timestamp must not poison the active attempt's clock.
    CHECK(!p.connected(1, 0, true, 0));
    CHECK(!p.lost(1, 0, 0));
    CHECK(p.state() == LinkState::Connecting);
    CHECK(p.health().staleEvents == 2);
    CHECK(p.connected(2, 112, true, 0));
    CHECK(p.state() == LinkState::Online);
    CHECK(!p.connected(2, 0, true, 0));
    CHECK(p.state() == LinkState::Online);
    CHECK(p.lost(2, 121, UINT32_MAX));
    CHECK(p.nextAttemptMs() == 131); // Unstable session keeps elevated cap.
    CHECK(p.nextDelayCapMs() == 11);
    s = p.poll(131, true, 0);
    CHECK(s.token == 3 && s.action == LinkAction::Connect);
    CHECK(p.connected(3, 131, true, 0));
    CHECK(p.lost(3, 141, UINT32_MAX)); // Stable exactly at boundary resets cap.
    CHECK(p.nextAttemptMs() == 146);
    CHECK(p.nextDelayCapMs() == 10);
    CHECK(!p.lost(3, 0, 0));
    CHECK(p.state() == LinkState::Waiting);
    p.stop();
    CHECK(p.state() == LinkState::Stopped);
    CHECK(p.start(142));
    s = p.poll(142, true, 0);
    CHECK(s.action == LinkAction::Connect && s.token == 4);
    CHECK(!p.connected(3, 0, true, 0));
    CHECK(p.connected(4, 143, true, 0));
    CHECK(p.poll(153, true, 0).action == LinkAction::None);
    CHECK(p.nextDelayCapMs() == 5);
  }
  {
    ReconnectPolicy p(cfg);
    CHECK(p.start(0));
    CHECK(p.poll(0, true, 0).action == LinkAction::Connect);
    CHECK(p.poll(1, false, UINT32_MAX).action == LinkAction::Close);
    CHECK(p.state() == LinkState::Waiting && p.nextAttemptMs() == 6);
    CHECK(!p.connected(1, 2, true, 0));
    CHECK(p.poll(6, false, 0).action == LinkAction::None);
    CHECK(p.poll(7, true, 0).action == LinkAction::Connect);
    CHECK(p.connected(2, 8, true, 0));
    CHECK(p.poll(9, false, 0).action == LinkAction::Close);
    CHECK(p.state() == LinkState::Waiting && p.nextAttemptMs() == 14);
    CHECK(p.poll(14, true, 0).action == LinkAction::Connect);
    CHECK(p.poll(21, true, 0).action == LinkAction::Close);
    CHECK(p.health().timeouts == 1);
    CHECK(p.health().failures == 3);
    CHECK(p.health().clockBlocks == 3);
    CHECK(p.nextAttemptMs() == 27);
    CHECK(p.nextDelayCapMs() == 11);
  }
  {
    ReconnectPolicy p(cfg);
    CHECK(p.start(20));
    CHECK(p.poll(20, true, 0).action == LinkAction::Connect);
    CHECK(!p.connected(1, 19, true, 0));
    CHECK(p.state() == LinkState::Fault);
    p.stop();
    CHECK(p.state() == LinkState::Fault);
    CHECK(!p.start(30));
    CHECK(p.poll(30, true, 0).action == LinkAction::None);
  }
  {
    ReconnectPolicy p(cfg);
    CHECK(p.start(20));
    CHECK(p.poll(19, true, 0).action == LinkAction::Close);
    CHECK(p.state() == LinkState::Fault);
  }
  {
    ReconnectPolicy p(cfg);
    CHECK(p.start(UINT64_MAX - 1));
    CHECK(p.poll(UINT64_MAX - 1, true, 0).action == LinkAction::Connect);
    CHECK(p.lost(1, UINT64_MAX, 0));
    CHECK(p.state() == LinkState::Fault); // Deadline cannot wrap into the past.
  }
  // All invalid configuration families latch fail-closed.
  for (unsigned which = 0; which < 4; ++which) {
    ReconnectConfig bad = cfg;
    if (which == 0) bad.initialMs = 0;
    if (which == 1) bad.maximumMs = 4;
    if (which == 2) bad.timeoutMs = 0;
    if (which == 3) bad.stableMs = 0;
    ReconnectPolicy p(bad);
    CHECK(p.state() == LinkState::Fault);
    CHECK(!p.start(0));
    CHECK(p.poll(1, true, 0).action == LinkAction::None);
  }
  // Exercise endpoint jitter at tiny, odd, even, and largest uint32 caps.
  const uint32_t caps[] = {1, 2, 3, 5, 16, UINT32_MAX};
  const uint32_t samples[] = {0, 1, UINT32_MAX/2, UINT32_MAX-1, UINT32_MAX};
  for (uint32_t cap : caps) {
    uint64_t previous = 0;
    for (uint32_t sample : samples) {
      ReconnectConfig c = cfg; c.initialMs = cap; c.maximumMs = cap;
      ReconnectPolicy p(c);
      CHECK(p.start(0));
      CHECK(p.poll(0, true, 0).action == LinkAction::Connect);
      CHECK(p.lost(1, 1, sample));
      const uint64_t delay = p.nextAttemptMs()-1;
      CHECK(delay >= cap/2 + cap%2 && delay <= cap);
      CHECK(delay >= previous); previous = delay;
      CHECK(p.nextDelayCapMs() == cap);
      if (sample == 0) CHECK(delay == cap/2 + cap%2);
      if (sample == UINT32_MAX) CHECK(delay == cap);
    }
  }
  {
    // Millis32 independent wrap and half-range boundary checks.
    Millis32 m;
    CHECK(!m.ready()); CHECK(!m.faulted());
    CHECK(m.update(UINT32_MAX));
    CHECK(m.elapsedMs() == 0 && m.ready());
    CHECK(m.update(0)); CHECK(m.elapsedMs() == 1);
    CHECK(m.update(0)); CHECK(m.elapsedMs() == 1);
    CHECK(m.update(UINT32_C(0x7fffffff)));
    CHECK(m.elapsedMs() == UINT64_C(0x80000000));
    CHECK(!m.update(UINT32_MAX)); // Exactly 2^31 delta is forbidden.
    CHECK(m.faulted() && !m.ready());
    CHECK(!m.update(1));
    CHECK(m.elapsedMs() == UINT64_C(0x80000000));
  }
  std::cout << "Independent reconnect/Millis32 review: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
