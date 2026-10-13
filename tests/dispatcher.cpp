#include "cts/Dispatcher.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
using namespace cts;
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
static_assert(!std::is_copy_constructible<Dispatcher>::value,
              "no cloned dispatchers");
constexpr uint64_t T = 1800000000000ULL;
struct Clock : ClockProvider {
  ClockReading t;
  Clock() {
    t.utcMs = T;
    t.monotonicMs = 100;
    t.synchronized = true;
  }
  ClockReading read() noexcept override { return t; }
};
struct Policy : CommandPolicy {
  bool allowed = true;
  bool permits(const Command &c) const noexcept override {
    return allowed && c.operation == Operation::Sample &&
           !std::strcmp(c.channel, "s");
  }
};
struct Executor : CommandExecutor {
  int calls = 0;
  bool ok = true;
  std::string ids;
  bool execute(const Command &c) noexcept override {
    ++calls;
    ids += std::string(c.id) + ",";
    return ok;
  }
};
std::string message(int id = 1, uint64_t created = T,
                    uint64_t expires = T + 5000, const char *channel = "s") {
  return "{\"schema\":\"genesis_swarm_command_v1\",\"swarm_id\":\"lab\",\"node_"
         "id\":\"node\",\"boot_id\":\"boot\",\"command_id\":\"c" +
         std::to_string(id) +
         "\",\"operation\":\"sample\",\"params\":{\"channel\":\"" + channel +
         "\"},\"created_ms\":" + std::to_string(created) +
         ",\"expires_ms\":" + std::to_string(expires) +
         ",\"status\":\"pending\"}";
}
DeliveryContext delivery() {
  DeliveryContext d;
  d.authenticated = true;
  d.retained = false;
  return d;
}
DispatchResult submit(Dispatcher &d, const std::string &s) {
  return d.submit(s.data(), s.size(), delivery());
}
struct Fixture {
  Clock clock;
  Policy policy;
  Executor executor;
  Dispatcher d;
  Fixture() : d(clock, policy) {
    CHECK(d.begin("lab", "node", "boot") == DispatchCode::Ok);
  }
};
int main() {
  {
    Clock c;
    Policy p;
    Dispatcher d(c, p);
    Executor e;
    CHECK(submit(d, message()).code == DispatchCode::NotStarted);
    CHECK(d.runOne(e).code == DispatchCode::NotStarted);
    CHECK(d.begin(nullptr, "node", "boot") == DispatchCode::InvalidScope);
    CHECK(d.begin("lab", "node", "boot") == DispatchCode::Ok);
    CHECK(d.begin("lab", "node", "other") == DispatchCode::AlreadyStarted);
    CHECK(d.runOne(e).code == DispatchCode::Empty);
  }
  {
    Fixture f;
    auto s = message();
    auto bad = delivery();
    bad.authenticated = false;
    CHECK(f.d.submit(s.data(), s.size(), bad).code == DispatchCode::Untrusted);
    bad = delivery();
    bad.retained = true;
    CHECK(f.d.submit(s.data(), s.size(), bad).code == DispatchCode::Retained);
    CHECK(f.d.pending() == 0);
    CHECK(submit(f.d, s).code == DispatchCode::Queued);
    CHECK(submit(f.d, s).state == CommandState::Queued);
    CHECK(submit(f.d, message(1, T, T + 4000)).code == DispatchCode::Conflict);
    CHECK(submit(f.d, message(1, T, T + 5000, "other")).code ==
          DispatchCode::Conflict);
    CHECK(f.d.pending() == 1);
    auto r = f.d.runOne(f.executor);
    CHECK(r.code == DispatchCode::Completed &&
          r.state == CommandState::Succeeded);
    CHECK(std::string(r.commandId) == "c1");
    for (int i = 0; i < 100; ++i)
      CHECK(submit(f.d, s).code == DispatchCode::Duplicate);
    CHECK(f.executor.calls == 1 && f.d.pending() == 0);
  }
  {
    Fixture f;
    CHECK(submit(f.d, message(1, T, T + 5000, "unknown")).code ==
          DispatchCode::Denied);
    CHECK(submit(f.d, "{}").code == DispatchCode::DecodeRejected);
    CHECK(submit(f.d, message(1, T + 1, T + 5000)).decodeError ==
          CommandError::Future);
    CHECK(submit(f.d, message(1, T - 10, T)).decodeError ==
          CommandError::Expired);
    CHECK(f.d.pending() == 0);
    CHECK(submit(f.d, message()).code == DispatchCode::Queued);
    f.policy.allowed = false;
    CHECK(f.d.runOne(f.executor).state == CommandState::Denied);
    CHECK(f.executor.calls == 0);
    f.policy.allowed = true;
    CHECK(submit(f.d, message()).state == CommandState::Denied);
  }
  {
    Fixture f;
    CHECK(submit(f.d, message()).code == DispatchCode::Queued);
    f.executor.ok = false;
    CHECK(f.d.runOne(f.executor).state == CommandState::Failed);
    CHECK(submit(f.d, message()).state == CommandState::Failed);
    CHECK(f.executor.calls == 1);
  }
  for (int mode = 0; mode < 2; ++mode) {
    Fixture f;
    CHECK(submit(f.d, message()).code == DispatchCode::Queued);
    if (mode == 0)
      f.clock.t.utcMs += 5000;
    else
      f.clock.t.monotonicMs += 5000;
    CHECK(f.d.runOne(f.executor).state == CommandState::Expired);
    CHECK(f.executor.calls == 0);
  }
  {
    Fixture f;
    f.clock.t.synchronized = false;
    CHECK(submit(f.d, message()).code == DispatchCode::ClockUnavailable);
    f.clock.t.synchronized = true;
    CHECK(submit(f.d, message()).code == DispatchCode::Queued);
    f.clock.t.synchronized = false;
    CHECK(f.d.runOne(f.executor).code == DispatchCode::ClockUnavailable);
    CHECK(f.d.pending() == 1 && f.executor.calls == 0);
    f.clock.t.synchronized = true;
    CHECK(f.d.runOne(f.executor).state == CommandState::Succeeded);
  }
  for (int mode = 0; mode < 2; ++mode) {
    Fixture f;
    CHECK(submit(f.d, message()).code == DispatchCode::Queued);
    if (mode == 0)
      --f.clock.t.utcMs;
    else
      --f.clock.t.monotonicMs;
    CHECK(f.d.runOne(f.executor).code == DispatchCode::ClockFault);
    f.clock.t.utcMs = T + 1;
    f.clock.t.monotonicMs = 101;
    CHECK(f.d.runOne(f.executor).code == DispatchCode::ClockFault);
    CHECK(f.d.pending() == 1 && f.executor.calls == 0);
  }
  {
    Fixture f;
    f.clock.t.monotonicMs = std::numeric_limits<uint64_t>::max() - 10;
    CHECK(submit(f.d, message()).code == DispatchCode::ClockFault);
    CHECK(f.d.pending() == 0);
  }
  {
    Fixture f;
    for (int i = 0; i < 8; ++i)
      CHECK(submit(f.d, message(i)).code == DispatchCode::Queued);
    CHECK(submit(f.d, message(8)).code == DispatchCode::QueueFull);
    CHECK(submit(f.d, message(0)).code == DispatchCode::Duplicate);
    CHECK(f.d.pending() == 8);
    for (int i = 0; i < 8; ++i)
      CHECK(f.d.runOne(f.executor).state == CommandState::Succeeded);
    CHECK(f.executor.ids == "c0,c1,c2,c3,c4,c5,c6,c7,");
    for (int i = 8; i < 16; ++i) {
      CHECK(submit(f.d, message(i)).code == DispatchCode::Queued);
      CHECK(f.d.runOne(f.executor).state == CommandState::Succeeded);
    }
    CHECK(submit(f.d, message(16)).code == DispatchCode::HistoryFull);
    CHECK(submit(f.d, message(0)).code == DispatchCode::Duplicate);
    f.clock.t.utcMs += 5000;
    CHECK(submit(f.d, message(16, T + 5000, T + 10000)).code ==
          DispatchCode::HistoryFull);
    f.clock.t.monotonicMs += 5000;
    CHECK(submit(f.d, message(16, T + 5000, T + 10000)).code ==
          DispatchCode::Queued);
    CHECK(f.d.runOne(f.executor).state == CommandState::Succeeded);
    CHECK(f.executor.calls == 17);
  }
  // Long repeated cycles exercise queue index wrap and terminal-history reuse.
  {
    Fixture f;
    for (int cycle = 0; cycle < 100; ++cycle) {
      const auto t = f.clock.t.utcMs;
      for (int j = 0; j < 8; ++j)
        CHECK(submit(f.d, message(cycle * 8 + j, t, t + 100)).code ==
              DispatchCode::Queued);
      for (int j = 0; j < 8; ++j)
        CHECK(f.d.runOne(f.executor).state == CommandState::Succeeded);
      f.clock.t.utcMs += 100;
      f.clock.t.monotonicMs += 100;
    }
    CHECK(f.executor.calls == 800 && f.d.pending() == 0);
  }
  // Same-object callback reentry cannot execute or mutate another command.
  {
    struct Reentrant : CommandExecutor {
      Dispatcher *d = nullptr;
      bool safe = false;
      bool execute(const Command &) noexcept override {
        safe = d->runOne(*this).code == DispatchCode::Busy &&
               submit(*d, message(2)).code == DispatchCode::Busy;
        return safe;
      }
    } e;
    Fixture f;
    e.d = &f.d;
    CHECK(submit(f.d, message()).code == DispatchCode::Queued);
    CHECK(f.d.runOne(e).state == CommandState::Succeeded && e.safe);
    CHECK(f.d.pending() == 0);
  }
  // Clock and policy are sampled again just before a callback, not only at
  // admission.
  for (int mode = 0; mode < 2; ++mode) {
    struct SlowPolicy : CommandPolicy {
      Clock *clock = nullptr;
      mutable int calls = 0;
      bool invalidate = false;
      bool permits(const Command &) const noexcept override {
        if (++calls == 2) {
          if (invalidate)
            clock->t.synchronized = false;
          else
            clock->t.monotonicMs += 5000;
        }
        return true;
      }
    } p;
    Clock c;
    p.clock = &c;
    p.invalidate = mode == 1;
    Executor e;
    Dispatcher d(c, p);
    CHECK(d.begin("lab", "node", "boot") == DispatchCode::Ok);
    CHECK(submit(d, message()).code == DispatchCode::Queued);
    auto r = d.runOne(e);
    if (mode == 0)
      CHECK(r.state == CommandState::Expired);
    else {
      CHECK(r.code == DispatchCode::ClockUnavailable);
      CHECK(d.pending() == 1);
    }
    CHECK(e.calls == 0);
  }
  {
    Clock c;
    Policy p;
    Dispatcher d(c, p);
    char swarm[] = "lab", node[] = "node", boot[] = "boot";
    CHECK(d.begin(swarm, node, boot) == DispatchCode::Ok);
    swarm[0] = 'x';
    node[0] = 'x';
    boot[0] = 'x';
    auto s = message();
    CHECK(submit(d, s).code == DispatchCode::Queued);
    CHECK(submit(d, "  " + s + "\n").code == DispatchCode::Duplicate);
    s.assign("destroyed input");
    Executor e;
    CHECK(d.runOne(e).state == CommandState::Succeeded && e.ids == "c1,");
  }
  {
    Fixture f;
    auto s = message();
    auto at = s.find("boot\"");
    s.replace(at, 4, "evil");
    CHECK(submit(f.d, s).decodeError == CommandError::Scope);
    CHECK(f.d.pending() == 0);
    f.clock.t.utcMs = 0;
    CHECK(submit(f.d, message()).code == DispatchCode::ClockUnavailable);
    f.clock.t.utcMs = MaxWireInteger + 1;
    CHECK(submit(f.d, message()).code == DispatchCode::ClockUnavailable);
  }
  {
    Fixture f;
    for (int i = 0; i < 8; ++i)
      CHECK(submit(f.d, message(i)).code == DispatchCode::Queued);
    f.clock.t.utcMs += 5000;
    f.clock.t.monotonicMs += 5000;
    CHECK(submit(f.d, message(9, T + 5000, T + 10000)).code ==
          DispatchCode::QueueFull);
    for (int i = 0; i < 8; ++i)
      CHECK(f.d.runOne(f.executor).state == CommandState::Expired);
    CHECK(f.executor.calls == 0);
    CHECK(submit(f.d, message(9, T + 5000, T + 10000)).code ==
          DispatchCode::Queued);
    CHECK(f.d.runOne(f.executor).state == CommandState::Succeeded);
  }
  std::cout << "dispatcher checks=" << checks << " failures=" << failures
            << " sizeof(Dispatcher)=" << sizeof(Dispatcher) << "\n";
  return failures ? 1 : 0;
}
