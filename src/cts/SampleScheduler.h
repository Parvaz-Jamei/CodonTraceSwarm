// SPDX-License-Identifier: MIT
#pragma once
#include "../CodonTraceSwarm.h"
namespace cts {
enum class SchedulerCode { Ok, Rejected, Full, Fault };
struct SchedulePoll {
  SchedulerCode code = SchedulerCode::Ok;
  bool due = false;
  const char *channel = nullptr;
  uint64_t missed = 0;
};
// One event loop. No allocation. setRate replaces only the next deadline.
// A late poll emits one sample and counts the skipped slots; it does not
// burst-catch-up. Rates are checked against the Device's stored min/max.
class SampleScheduler {
public:
  static constexpr size_t Capacity = MaxChannels;
  explicit SampleScheduler(const Device &device) : device_(device) {}
  SampleScheduler(const SampleScheduler &) = delete;
  SampleScheduler &operator=(const SampleScheduler &) = delete;
  SchedulerCode add(const char *channel, double hz, uint64_t nowUs) noexcept;
  SchedulerCode setRate(const char *channel, double hz, uint64_t nowUs) noexcept;
  SchedulePoll poll(uint64_t nowUs) noexcept;
  bool deadline(const char *channel, uint64_t &outUs) const noexcept;
  uint64_t totalDrops() const noexcept { return drops_; }
  uint64_t drops(const char *channel) const noexcept;

private:
  struct Slot {
    char name[64] = {};
    double hz = 0;
    uint64_t periodUs = 0, nextUs = 0, drops = 0;
    bool used = false;
  };
  const Device &device_;
  Slot slots_[Capacity];
  uint64_t drops_ = 0;
  bool fault_ = false;
  bool limits(const char *, bool &hasRate, double &minimumHz,
              double &maximumHz) const noexcept;
  Slot *find(const char *) noexcept;
  const Slot *find(const char *) const noexcept;
  static bool period(double hz, uint64_t &out) noexcept;
  static uint64_t sat(uint64_t, uint64_t) noexcept;
};
} // namespace cts
