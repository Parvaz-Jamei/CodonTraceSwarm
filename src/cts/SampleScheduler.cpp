// SPDX-License-Identifier: MIT
#include "SampleScheduler.h"
#include <cmath>
#include <cstring>
#include <limits>
namespace cts {
namespace {
// 2^64 is the first positive value that does not fit in uint64_t. It is
// exactly representable as a double. static_cast<double>(UINT64_MAX) is also
// 2^64, so a greater-than comparison against that cast accepts a period of
// exactly 2^64 and the following conversion overflows.
bool strictlyBelowTwo64(double value) noexcept {
  return std::isfinite(value) && value < 18446744073709551616.0;
}
} // namespace
bool SampleScheduler::period(double hz, uint64_t &out) noexcept {
  if (!std::isfinite(hz) || hz <= 0)
    return false;
  const double us = 1000000.0 / hz;
  if (!(us >= 1.0) || !strictlyBelowTwo64(us))
    return false;
  const double rounded = us + 0.5;
  if (!strictlyBelowTwo64(rounded))
    return false;
  out = static_cast<uint64_t>(rounded);
  return out != 0;
}
uint64_t SampleScheduler::sat(uint64_t a, uint64_t b) noexcept {
  if (a > std::numeric_limits<uint64_t>::max() - b)
    return std::numeric_limits<uint64_t>::max();
  return a + b;
}
bool SampleScheduler::limits(const char *name, bool &hasRate, double &minimumHz,
                            double &maximumHz) const noexcept {
  hasRate = false;
  if (!name)
    return false;
  for (size_t i = 0; i < device_.count_; ++i) {
    if (std::strcmp(device_.channels_[i].name, name))
      continue;
    hasRate = device_.channels_[i].hasRate;
    minimumHz = device_.channels_[i].minimumHz;
    maximumHz = device_.channels_[i].maximumHz;
    return true;
  }
  return false;
}
SampleScheduler::Slot *SampleScheduler::find(const char *name) noexcept {
  if (!name)
    return nullptr;
  for (size_t i = 0; i < Capacity; ++i)
    if (slots_[i].used && !std::strcmp(slots_[i].name, name))
      return &slots_[i];
  return nullptr;
}
const SampleScheduler::Slot *SampleScheduler::find(const char *name) const noexcept {
  if (!name)
    return nullptr;
  for (size_t i = 0; i < Capacity; ++i)
    if (slots_[i].used && !std::strcmp(slots_[i].name, name))
      return &slots_[i];
  return nullptr;
}
SchedulerCode SampleScheduler::add(const char *channel, double hz,
                                  uint64_t nowUs) noexcept {
  if (fault_ || find(channel))
    return SchedulerCode::Rejected;
  bool has = false;
  double lo = 0, hi = 0;
  if (!limits(channel, has, lo, hi) || !has)
    return SchedulerCode::Rejected;
  if (!std::isfinite(hz) || hz < lo || hz > hi)
    return SchedulerCode::Rejected;
  uint64_t step = 0;
  if (!period(hz, step))
    return SchedulerCode::Rejected;
  if (step > std::numeric_limits<uint64_t>::max() - nowUs)
    return SchedulerCode::Fault;
  size_t n = 0;
  while (channel[n]) {
    if (n >= 63)
      return SchedulerCode::Rejected;
    ++n;
  }
  Slot *slot = nullptr;
  for (size_t i = 0; i < Capacity; ++i)
    if (!slots_[i].used) {
      slot = &slots_[i];
      break;
    }
  if (!slot)
    return SchedulerCode::Full;
  std::memcpy(slot->name, channel, n + 1);
  slot->hz = hz;
  slot->periodUs = step;
  slot->nextUs = nowUs + step;
  slot->drops = 0;
  slot->used = true;
  return SchedulerCode::Ok;
}
SchedulerCode SampleScheduler::setRate(const char *channel, double hz,
                                      uint64_t nowUs) noexcept {
  if (fault_)
    return SchedulerCode::Fault;
  Slot *slot = find(channel);
  bool has = false;
  double lo = 0, hi = 0;
  if (!slot || !limits(channel, has, lo, hi) || !has)
    return SchedulerCode::Rejected;
  if (!std::isfinite(hz) || hz < lo || hz > hi)
    return SchedulerCode::Rejected;
  uint64_t step = 0;
  if (!period(hz, step))
    return SchedulerCode::Rejected;
  if (step > std::numeric_limits<uint64_t>::max() - nowUs)
    return SchedulerCode::Fault;
  slot->hz = hz;
  slot->periodUs = step;
  slot->nextUs = nowUs + step;
  return SchedulerCode::Ok;
}
SchedulePoll SampleScheduler::poll(uint64_t nowUs) noexcept {
  SchedulePoll out;
  if (fault_) {
    out.code = SchedulerCode::Fault;
    return out;
  }
  int chosen = -1;
  for (size_t i = 0; i < Capacity; ++i) {
    if (!slots_[i].used || nowUs < slots_[i].nextUs)
      continue;
    if (chosen < 0 || slots_[i].nextUs < slots_[chosen].nextUs)
      chosen = static_cast<int>(i);
  }
  if (chosen < 0)
    return out;
  Slot &slot = slots_[chosen];
  if (!slot.periodUs) {
    fault_ = true;
    out.code = SchedulerCode::Fault;
    return out;
  }
  const uint64_t late = nowUs - slot.nextUs;
  const uint64_t skipped = late / slot.periodUs;
  const uint64_t steps = skipped + 1;
  if (steps > std::numeric_limits<uint64_t>::max() / slot.periodUs) {
    fault_ = true;
    out.code = SchedulerCode::Fault;
    return out;
  }
  const uint64_t advance = steps * slot.periodUs;
  if (advance > std::numeric_limits<uint64_t>::max() - slot.nextUs) {
    fault_ = true;
    out.code = SchedulerCode::Fault;
    return out;
  }
  slot.drops = sat(slot.drops, skipped);
  drops_ = sat(drops_, skipped);
  slot.nextUs += advance;
  out.due = true;
  out.channel = slot.name;
  out.missed = skipped;
  return out;
}
bool SampleScheduler::deadline(const char *channel, uint64_t &outUs) const noexcept {
  const Slot *slot = find(channel);
  if (!slot)
    return false;
  outUs = slot->nextUs;
  return true;
}
uint64_t SampleScheduler::drops(const char *channel) const noexcept {
  const Slot *slot = find(channel);
  return slot ? slot->drops : 0;
}
} // namespace cts
