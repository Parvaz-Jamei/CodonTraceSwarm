// Independent checkpoint-12 probe. Does not modify the library.
// Synthetic inputs only. Not a bench, flash, or scientific result.
#include "cts/Fdc2214Guard.h"
#include "cts/MockDrivers.h"
#include "cts/ReconnectOrchestrator.h"
#include "cts/SampleScheduler.h"
#include "session_fixture.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
static int checks = 0, failures = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      ++failures;                                                              \
      std::cerr << "FAIL " << __LINE__ << " " #x "\n";                         \
    }                                                                          \
  } while (0)
static cts::ChannelSpec spec(const char *name, bool rate, double lo, double hi) {
  cts::ChannelSpec c = fixture::channel(name);
  c.descriptor.hasRate = rate;
  c.descriptor.minimumHz = lo;
  c.descriptor.maximumHz = hi;
  return c;
}
struct Mem : cts::ByteBus {
  uint8_t mem[256] = {};
  bool failWrite = false, failRead = false;
  int writes = 0, reads = 0;
  uint8_t pointer = 0;
  cts::BusStatus write(uint8_t, const uint8_t *data, size_t n) noexcept override {
    ++writes;
    if (failWrite || !data || !n)
      return cts::BusStatus::Fail;
    pointer = data[0];
    for (size_t i = 1; i < n; ++i)
      mem[static_cast<uint8_t>(pointer + i - 1)] = data[i];
    return cts::BusStatus::Ok;
  }
  cts::BusStatus read(uint8_t, uint8_t *data, size_t n) noexcept override {
    ++reads;
    if (failRead || !data || !n)
      return cts::BusStatus::Fail;
    for (size_t i = 0; i < n; ++i)
      data[i] = mem[static_cast<uint8_t>(pointer + i)];
    return cts::BusStatus::Ok;
  }
};
struct Link : cts::ConnectAttemptClient {
  int starts = 0, polls = 0, cancels = 0;
  bool done = false, ok = true, failPoll = false;
  uint64_t gen = 0, cancelledGen = 0;
  cts::AttemptIo start(uint64_t g) noexcept override {
    ++starts;
    gen = g;
    return cts::AttemptIo::Ok;
  }
  cts::AttemptIo poll(uint64_t g, bool &p, bool &d, bool &o) noexcept override {
    ++polls;
    if (failPoll)
      return cts::AttemptIo::Fail;
    p = true;
    d = done && g == gen;
    o = ok;
    return cts::AttemptIo::Ok;
  }
  void cancel(uint64_t g) noexcept override {
    ++cancels;
    cancelledGen = g;
  }
};
struct Pub : cts::QosPublisher {
  enum class Mode { Complete, Partial, Fail, WriteOnly, ShortComplete, QosEarly, ZeroAccepted } mode =
      Mode::Complete;
  int steps = 0, cancels = 0, completed = 0;
  uint64_t lastGen = 0, cancelGen = 0;
  char last[8192] = {};
  char held[8192] = {};
  size_t heldN = 0;
  bool step(uint64_t generation, const char *data, size_t size, size_t &accepted,
            bool &writeComplete, bool &qos1Complete) noexcept override {
    ++steps;
    lastGen = generation;
    last[0] = 0;
    if (!data || !size || size >= sizeof(last))
      return false;
    std::memcpy(last, data, size);
    last[size] = 0;
    std::memcpy(held, data, size + 1);
    heldN = size;
    if (mode == Mode::Fail) {
      accepted = size > 3 ? 3 : 0;
      writeComplete = false;
      qos1Complete = false;
      return false;
    }
    if (mode == Mode::Partial) {
      accepted = size > 4 ? 4 : size;
      writeComplete = false;
      qos1Complete = false;
      return true;
    }
    if (mode == Mode::WriteOnly) {
      accepted = size;
      writeComplete = true;
      qos1Complete = false;
      return true;
    }
    if (mode == Mode::ShortComplete) {
      accepted = size > 1 ? size - 1 : 0;
      writeComplete = true;
      qos1Complete = true;
      return true;
    }
    if (mode == Mode::QosEarly) {
      accepted = size;
      writeComplete = false;
      qos1Complete = true;
      return true;
    }
    if (mode == Mode::ZeroAccepted) {
      accepted = 0;
      writeComplete = true;
      qos1Complete = true;
      return true;
    }
    accepted = size;
    writeComplete = true;
    qos1Complete = true;
    ++completed;
    return true;
  }
  void cancel(uint64_t g) noexcept override {
    ++cancels;
    cancelGen = g;
  }
};
static bool kind(const char *bytes, const char *name) {
  char needle[32];
  std::snprintf(needle, sizeof(needle), "\"kind\":\"%s\"", name);
  return bytes && std::strstr(bytes, needle);
}
static void putLe(Mem &bus, uint8_t reg, int32_t v) {
  const uint32_t u = static_cast<uint32_t>(v);
  bus.mem[reg] = static_cast<uint8_t>(u);
  bus.mem[reg + 1] = static_cast<uint8_t>(u >> 8);
  bus.mem[reg + 2] = static_cast<uint8_t>(u >> 16);
  bus.mem[reg + 3] = static_cast<uint8_t>(u >> 24);
}
static bool invalidSample(const cts::Sample &s) {
  return !s.valid && std::strcmp(s.status, "ok") != 0 && std::isfinite(s.value);
}
static void attackScheduler() {
  using namespace cts;
  Device device;
  CHECK(device.define(spec("s", true, 1, 10)) == Error::Ok);
  CHECK(device.define(spec("b", true, 1, 10)) == Error::Ok);
  auto widened = spec("s", true, 1, 10);
  widened.descriptor.maximumHz = 100;
  SampleScheduler scheduler(device);
  CHECK(widened.descriptor.maximumHz == 100);
  CHECK(scheduler.add("s", 50, 0) == SchedulerCode::Rejected);
  CHECK(scheduler.add("s", 10, 0) == SchedulerCode::Ok);
  CHECK(scheduler.add("s", 10, 0) == SchedulerCode::Rejected);
  CHECK(scheduler.add("s", 10.1, 0) == SchedulerCode::Rejected);
  uint64_t deadline = 0;
  CHECK(scheduler.deadline("s", deadline) && deadline == 100000);
  const uint64_t at = 100000ull + 250ull * 100000ull;
  auto burst = scheduler.poll(at);
  CHECK(burst.due && burst.missed == 250);
  CHECK(scheduler.totalDrops() == 250);
  auto again = scheduler.poll(at);
  CHECK(!again.due && again.missed == 0);
  CHECK(scheduler.totalDrops() == 250);
  CHECK(scheduler.add("b", 1, 0) == SchedulerCode::Ok);
  auto other = scheduler.poll(1000000);
  CHECK(other.due && other.missed == 0);
  CHECK(other.channel && !std::strcmp(other.channel, "b"));
  CHECK(!scheduler.poll(1000000).due);
  // Tight bound that is not an exact divisor of 1e6. Nearest microsecond can
  // land one quantum over the stored maximum. Observed, not treated as a burst.
  Device tight;
  CHECK(tight.define(spec("fast", true, 1, 9999)) == Error::Ok);
  SampleScheduler fast(tight);
  CHECK(fast.add("fast", 9999, 0) == SchedulerCode::Ok);
  uint64_t fastDue = 0;
  CHECK(fast.deadline("fast", fastDue));
  std::cout << "note rate9999_period_us=" << fastDue
            << " effective_hz=" << (1000000.0 / static_cast<double>(fastDue))
            << "\n";
  // Far-future single poll must not emit a second sample at the same time.
  Device edge;
  CHECK(edge.define(spec("e", true, 1, 10000)) == Error::Ok);
  SampleScheduler edgeSch(edge);
  CHECK(edgeSch.add("e", 10000, 0) == SchedulerCode::Ok);
  auto far = edgeSch.poll(std::numeric_limits<uint64_t>::max());
  CHECK(far.code == SchedulerCode::Fault || (far.due && far.missed > 0));
  auto far2 = edgeSch.poll(std::numeric_limits<uint64_t>::max());
  CHECK(!(far.due && far2.due));
  CHECK(far2.code == SchedulerCode::Fault || !far2.due);
}
static void attackDrivers() {
  using namespace cts;
  const uint64_t utc = 1800000000000ULL;
  {
    Mem bus;
    Sht4x sensor(bus, 0x44);
    Sht4xView view;
    const int writes = bus.writes, reads = bus.reads;
    CHECK(Sht4x::bind("pressure", view) == DriverStatus::Unsupported);
    CHECK(!view.bound && view.quantity[0] == 0);
    Sample attacked = Sample::scalar(12, utc);
    CHECK(sensor.readQuantity("pressure", attacked, utc) == DriverStatus::Unsupported);
    CHECK(invalidSample(attacked));
    CHECK(std::strcmp(attacked.status, "unsupported") == 0);
    CHECK(bus.writes == writes && bus.reads == reads);
    CHECK(sensor.readQuantity(nullptr, attacked, utc) == DriverStatus::InvalidArgument);
    CHECK(invalidSample(attacked));
    bus.failRead = true;
    auto failed = sensor.read(utc);
    CHECK(failed.status == DriverStatus::ReadFailed);
    CHECK(invalidSample(failed.temperature) && invalidSample(failed.humidity));
    bus.failRead = false;
    bus.mem[0xFD] = 0;
    bus.mem[0xFE] = 0;
    bus.mem[0x100 - 256] = 0;
    // raw 0 is a successful conversion, not a failed read of numeric zero.
    auto zeroRaw = sensor.read(utc);
    CHECK(zeroRaw.status == DriverStatus::Ok);
    CHECK(zeroRaw.temperature.valid && zeroRaw.humidity.valid);
    CHECK(zeroRaw.temperature.value != 0 || zeroRaw.humidity.value != 0);
  }
  {
    Mem bus;
    Fdc1004 sensor(bus, 0x50);
    CHECK(sensor.applyCapdac(0, 0) == DriverStatus::Ok);
    const int writes = bus.writes;
    CHECK(sensor.applyCapdac(0, 0) == DriverStatus::AlreadyApplied);
    CHECK(sensor.applyCapdac(0, 7) == DriverStatus::AlreadyApplied);
    CHECK(sensor.capdac(0) == 0);
    CHECK(sensor.capdacApplied(0));
    CHECK(bus.writes == writes);
    CHECK(sensor.rejectedCapdacApplies() == 2);
    CHECK(sensor.applyCapdac(1, 31) == DriverStatus::Ok);
    CHECK(sensor.applyCapdac(1, 31) == DriverStatus::AlreadyApplied);
    CHECK(sensor.capdac(1) == 31);
    bus.failWrite = true;
    CHECK(sensor.applyCapdac(2, 4) == DriverStatus::ReadFailed);
    CHECK(!sensor.capdacApplied(2));
    bus.failWrite = false;
    CHECK(sensor.applyCapdac(2, 4) == DriverStatus::Ok);
    bus.failRead = true;
    auto failed = sensor.read(0, utc);
    CHECK(failed.status == DriverStatus::ReadFailed);
    CHECK(!failed.hasPicofarads && !failed.hasRaw);
    CHECK(invalidSample(failed.picofarads) && invalidSample(failed.rawSample));
    bus.failRead = false;
    bus.mem[0] = 0;
    bus.mem[1] = 0;
    bus.mem[2] = 0;
    auto z = sensor.read(0, utc);
    CHECK(z.status == DriverStatus::Ok && z.hasRaw && z.raw == 0);
    CHECK(z.hasPicofarads && z.picofarads.valid);
  }
  {
    Mem bus;
    bus.mem[0x10] = 0;
    bus.mem[0x11] = 0;
    bus.mem[0x12] = 0;
    bus.mem[0x13] = 0;
    Ina219 ina(bus, 0x40);
    auto raw = ina.read(utc);
    CHECK(raw.status == DriverStatus::NotConfigured);
    CHECK(raw.hasCounts && raw.shuntCounts == 0);
    CHECK(!raw.hasCurrent);
    CHECK(invalidSample(raw.current));
    CHECK(ina.setShuntOhms(0) == DriverStatus::InvalidArgument);
    CHECK(ina.setShuntOhms(-1) == DriverStatus::InvalidArgument);
    CHECK(ina.setShuntOhms(std::numeric_limits<double>::quiet_NaN()) ==
          DriverStatus::InvalidArgument);
    CHECK(!ina.shuntConfigured());
    CHECK(ina.setShuntOhms(0.1) == DriverStatus::Ok);
    auto zeroA = ina.read(utc);
    CHECK(zeroA.status == DriverStatus::Ok && zeroA.hasCurrent);
    CHECK(zeroA.current.valid && zeroA.current.value == 0);
    bus.failRead = true;
    auto bad = ina.read(utc);
    CHECK(bad.status == DriverStatus::ReadFailed);
    CHECK(!bad.hasCurrent && !bad.hasCounts);
    CHECK(invalidSample(bad.current) && invalidSample(bad.shunt));
  }
  {
    Mem bus;
    Hx711 hx(bus, 1);
    bus.mem[0] = 0;
    bus.mem[1] = 0;
    bus.mem[2] = 0;
    auto raw = hx.read(utc);
    CHECK(raw.hasRaw && raw.raw == 0 && raw.rawSample.valid);
    CHECK(!raw.hasKilograms && invalidSample(raw.kilograms));
    CHECK(hx.setTare(0) == DriverStatus::Ok);
    CHECK(!hx.calibrated());
    auto tared = hx.read(utc);
    CHECK(!tared.hasKilograms && invalidSample(tared.kilograms));
    CHECK(hx.setSpanCountsPerKilogram(0) == DriverStatus::InvalidArgument);
    CHECK(hx.setSpanCountsPerKilogram(std::numeric_limits<double>::infinity()) ==
          DriverStatus::InvalidArgument);
    CHECK(hx.setSpanCountsPerKilogram(1000) == DriverStatus::Ok);
    auto kg = hx.read(utc);
    CHECK(kg.hasKilograms && kg.kilograms.valid && kg.kilograms.value == 0);
    bus.failRead = true;
    auto bad = hx.read(utc);
    CHECK(!bad.hasRaw && !bad.hasKilograms);
    CHECK(invalidSample(bad.rawSample) && invalidSample(bad.kilograms));
    CHECK(bad.rawSample.value == 0 && !bad.rawSample.valid);
  }
  {
    Mem bus;
    bus.mem[0x00] = 2;
    bus.mem[0x02] = 0;
    bus.mem[0x03] = 0;
    Vl53l0x tof(bus, 0x29);
    auto timeout = tof.read(utc);
    CHECK(timeout.status == DriverStatus::ReadFailed);
    CHECK(!timeout.rangeValid);
    CHECK(std::strcmp(timeout.distance.status, "timeout") == 0);
    CHECK(invalidSample(timeout.distance));
    CHECK(!(timeout.distance.valid && timeout.distance.value == 0));
    bus.mem[0x00] = 0;
    auto zero = tof.read(utc);
    CHECK(zero.rangeValid && zero.distance.valid && zero.distance.value == 0);
    CHECK(zero.millimeters == 0);
  }
  {
    Mem bus;
    bus.mem[0xD0] = 0x60;
    bus.mem[0xF3] = 0;
    Bme280 bme(bus, 0x76);
    bus.failRead = true;
    auto bad = bme.read(utc);
    CHECK(bad.status == DriverStatus::ReadFailed);
    CHECK(invalidSample(bad.temperature) && invalidSample(bad.pressure) &&
          invalidSample(bad.humidity));
    bus.failRead = false;
    putLe(bus, 0x10, 0);
    putLe(bus, 0x14, 0);
    putLe(bus, 0x18, 0);
    auto zero = bme.read(utc);
    CHECK(zero.status == DriverStatus::Ok);
    CHECK(zero.temperature.valid && zero.temperature.value == 0);
    CHECK(zero.pressure.valid && zero.pressure.value == 0);
    CHECK(zero.humidity.valid && zero.humidity.value == 0);
  }
}
static void attackFdc() {
  using namespace cts;
  const uint32_t raw = 1u << 27;
  const double clock = 40e6;
  const uint16_t div0 = static_cast<uint16_t>(2u << 12); // FIN_SEL=2, divider 0
  const uint16_t div1 = static_cast<uint16_t>((2u << 12) | 1u);
  auto zero = Fdc2214Guard::readFrequencyHzFromRegisters(Fdc2214Variant::Fdc2214, raw,
                                                         clock, div0, nullptr);
  auto one = Fdc2214Guard::readFrequencyHzFromRegisters(Fdc2214Variant::Fdc2214, raw,
                                                        clock, div1, nullptr);
  CHECK(zero.status == Fdc2214Status::InvalidArgument);
  CHECK(!zero.hasFrequency);
  CHECK(zero.frequencyHz != one.frequencyHz);
  CHECK(one.status == Fdc2214Status::Ok && one.hasFrequency);
  CHECK(one.divider == 1);
  CHECK(zero.path == Fdc2214Path::Registers);
  auto caller = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock, 2,
                                              nullptr);
  CHECK(caller.path == Fdc2214Path::CallerReference);
  CHECK(caller.path != one.path);
  CHECK(caller.frequencyHz == one.frequencyHz); // divider 1 matches effective fREF
  CHECK(caller.divider == 0);                   // caller path does not invent divider 1
  auto regs2 = Fdc2214Guard::readFrequencyHzFromRegisters(
      Fdc2214Variant::Fdc2214, raw, clock,
      static_cast<uint16_t>((2u << 12) | 2u), nullptr);
  CHECK(regs2.frequencyHz * 2 == caller.frequencyHz);
  for (auto variant : {Fdc2214Variant::Fdc2112, Fdc2214Variant::Fdc2114}) {
    Fdc2214LcModel model;
    model.present = true;
    model.inductanceH = 18e-6;
    model.parasiticF = 0;
    auto a = Fdc2214Guard::readFrequencyHz(variant, raw, clock, 1, &model);
    auto b = Fdc2214Guard::readFrequencyHzFromRegisters(variant, raw, clock, div1, &model);
    auto z = Fdc2214Guard::readFrequencyHzFromRegisters(variant, raw, clock, div0, &model);
    CHECK(a.status == Fdc2214Status::NotSupported);
    CHECK(b.status == Fdc2214Status::NotSupported);
    CHECK(z.status == Fdc2214Status::NotSupported);
    CHECK(!a.hasFrequency && !b.hasFrequency && !z.hasFrequency);
    CHECK(!a.capacitanceDerived && !b.capacitanceDerived && !z.capacitanceDerived);
    CHECK(a.frequencyHz != caller.frequencyHz);
  }
  auto bare = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2212, raw, clock, 1, nullptr);
  CHECK(bare.hasFrequency && !bare.capacitanceDerived);
  CHECK(std::strcmp(bare.capacitanceStatus, "capacitance_not_derived") == 0);
  Fdc2214LcModel absent;
  absent.present = false;
  absent.inductanceH = 18e-6;
  absent.parasiticF = 0;
  auto missing = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock, 1, &absent);
  CHECK(missing.hasFrequency && !missing.capacitanceDerived && missing.capacitanceF == 0);
  Fdc2214LcModel badL;
  badL.present = true;
  badL.inductanceH = 0;
  badL.parasiticF = 0;
  auto noL = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock, 1, &badL);
  CHECK(noL.hasFrequency && !noL.capacitanceDerived);
  Fdc2214LcModel neg;
  neg.present = true;
  neg.inductanceH = 18e-6;
  neg.parasiticF = -1;
  auto noP = Fdc2214Guard::readFrequencyHz(Fdc2214Variant::Fdc2214, raw, clock, 1, &neg);
  CHECK(noP.hasFrequency && !noP.capacitanceDerived);
}
static void attackOrchestrator() {
  using namespace cts;
  char scratch[TxBytes];
  const Sample sample = Sample::scalar(1.25, fixture::T);
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(0));
    link.done = true;
    CHECK(orch.poll(0, false, 0) == OrchCode::Refused);
    CHECK(link.starts == 0 && link.polls == 0);
    CHECK(orch.poll(1000, false, 99) == OrchCode::Refused);
    CHECK(link.starts == 0);
    CHECK(orch.poll(1000, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1);
    CHECK(orch.helloComplete());
    CHECK(kind(pub.last, "hello"));
    CHECK(!kind(pub.last, "reading"));
    CHECK(orch.publishReading("s", sample, scratch, sizeof(scratch)) == OrchCode::Ok);
    CHECK(kind(pub.last, "reading"));
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(0));
    link.done = false;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1 && orch.generation() == 0);
    const char held[8] = {};
    (void)held;
    CHECK(orch.poll(1, false, 0) == OrchCode::Refused);
    CHECK(link.cancels == 1);
    CHECK(link.starts == 1);
    CHECK(!orch.readingsAllowed());
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(0));
    link.done = true;
    pub.mode = Pub::Mode::Fail;
    CHECK(fixture::submit(session, fixture::command()).dispatch.code == DispatchCode::Queued);
    CHECK(session.poll().code == SessionCode::ReplyPending);
    const auto seq = session.replySequence();
    const auto size = session.replySize();
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(orch.helloComplete() == false || session.replySize() == size);
    CHECK(session.replySize() == size);
    CHECK(session.replySequence() == seq);
    CHECK(orch.health().repliesConfirmed == 0);
    pub.mode = Pub::Mode::ShortComplete;
    CHECK(orch.poll(1, true, 0) == OrchCode::Ok);
    CHECK(session.replySize() == size);
    CHECK(orch.health().repliesConfirmed == 0);
    pub.mode = Pub::Mode::QosEarly;
    CHECK(orch.poll(2, true, 0) == OrchCode::Ok);
    CHECK(session.replySize() == size);
    CHECK(orch.health().repliesConfirmed == 0);
    pub.mode = Pub::Mode::ZeroAccepted;
    CHECK(orch.poll(3, true, 0) == OrchCode::Ok);
    CHECK(session.replySize() == size);
    CHECK(orch.health().repliesConfirmed == 0);
    pub.mode = Pub::Mode::WriteOnly;
    CHECK(orch.poll(4, true, 0) == OrchCode::Ok);
    CHECK(orch.publishActive());
    CHECK(session.replySize() == size);
    CHECK(orch.health().repliesConfirmed == 0);
    const auto gen = orch.generation();
    const char *before = pub.held;
    CHECK(kind(before, "reply") || kind(pub.last, "reply"));
    CHECK(orch.onDisconnected(gen, 5, 0) == OrchCode::Lost);
    CHECK(orch.losses() == 1);
    CHECK(session.replySize() == size);
    CHECK(session.replySequence() == seq);
    CHECK(orch.health().repliesConfirmed == 0);
    CHECK(orch.onDisconnected(gen, 6, 0) == OrchCode::Stale);
    CHECK(orch.losses() == 1);
    // Buffer captured at the in-flight step must still be the reply, not a reading.
    CHECK(!kind(pub.held, "reading"));
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(10));
    link.done = true;
    CHECK(orch.poll(10, true, 0) == OrchCode::Ok);
    CHECK(orch.helloComplete());
    CHECK(kind(pub.last, "hello"));
    pub.mode = Pub::Mode::Partial;
    CHECK(orch.publishReading("s", sample, scratch, sizeof(scratch)) == OrchCode::Ok);
    CHECK(orch.publishActive());
    CHECK(kind(pub.last, "reading"));
    char frozen[8192];
    std::memcpy(frozen, pub.held, pub.heldN + 1);
    CHECK(orch.poll(11, true, 0) == OrchCode::Ok);
    CHECK(std::memcmp(frozen, pub.held, pub.heldN + 1) == 0);
    CHECK(orch.health().readings == 0);
    const auto gen = orch.generation();
    CHECK(orch.onDisconnected(gen, 12, 0) == OrchCode::Lost);
    CHECK(orch.losses() == 1);
    const auto due = policy.nextAttemptMs();
    pub.mode = Pub::Mode::Complete;
    const int completed = pub.completed;
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(kind(pub.last, "hello"));
    CHECK(pub.completed == completed + 1);
    CHECK(orch.helloComplete());
    const int afterHello = pub.completed;
    CHECK(orch.publishReading("s", sample, scratch, sizeof(scratch)) == OrchCode::Ok);
    CHECK(pub.completed == afterHello + 1);
    CHECK(kind(pub.last, "reading"));
  }
}
static void attackCancelHoles() {
  using namespace cts;
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(4, 100, 1000) == AttemptCode::Started);
    CHECK(attempt.poll(4, 50) == AttemptCode::Fault);
    CHECK(attempt.phase() == AttemptPhase::Failed);
    CHECK(link.cancels == 0);
    CHECK(attempt.cancel(4) == AttemptCode::Stale);
    CHECK(link.cancels == 0);
    CHECK(attempt.start(5, 50, 1000) == AttemptCode::Started);
    CHECK(link.starts == 2 && link.cancels == 0);
    std::cout << "DEFECT rollback_no_cancel cancels=" << link.cancels
              << " starts=" << link.starts << "\n";
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(3, 0, 1000) == AttemptCode::Started);
    link.done = true;
    link.ok = false;
    CHECK(attempt.poll(3, 1) == AttemptCode::Failed);
    CHECK(link.cancels == 0);
    CHECK(attempt.cancel(3) == AttemptCode::Stale);
    std::cout << "DEFECT handshake_fail_no_cancel cancels=" << link.cancels << "\n";
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(8, 0, 1000) == AttemptCode::Started);
    link.done = true;
    link.ok = true;
    CHECK(attempt.poll(8, 1) == AttemptCode::Succeeded);
    CHECK(attempt.cancel(8) == AttemptCode::Stale);
    CHECK(link.cancels == 0);
    std::cout << "DEFECT succeeded_cancel_is_stale cancels=" << link.cancels << "\n";
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(4, 0, 1000) == AttemptCode::Started);
    link.failPoll = true;
    CHECK(attempt.poll(4, 1) == AttemptCode::Failed);
    CHECK(link.cancels == 0);
    CHECK(attempt.cancel(4) == AttemptCode::Stale);
    CHECK(link.cancels == 0);
  }
  {
    Link link;
    NonblockingAttempt attempt(link);
    CHECK(attempt.start(9, std::numeric_limits<uint64_t>::max() - 5, 1000) ==
          AttemptCode::Started);
    CHECK(attempt.poll(9, std::numeric_limits<uint64_t>::max()) == AttemptCode::Progress);
    CHECK(attempt.phase() == AttemptPhase::Running);
    CHECK(link.cancels == 0);
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox; // deliberately not bound
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(orch.start(0));
    link.done = true;
    link.ok = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1);
    CHECK(link.cancels == 0);
    CHECK(!orch.readingsAllowed());
    CHECK(orch.generation() == 0);
    CHECK(pub.steps == 0);
    const auto due = policy.nextAttemptMs();
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    std::cout << "note unbound_inbox starts=" << link.starts
              << " cancels=" << link.cancels << " polls=" << link.polls
              << " due=" << due << " policy=" << static_cast<int>(policy.state())
              << "\n";
    CHECK(link.cancels == 0);
    CHECK(link.starts >= 2);
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(inbox.connected(50) == InboxCode::Ok); // generation ahead of the first token
    CHECK(orch.start(0));
    link.done = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    std::cout << "note stale_inbox starts=" << link.starts
              << " cancels=" << link.cancels << " gen=" << orch.generation()
              << " online_gen_api=" << orch.generation()
              << " policy=" << static_cast<int>(policy.state()) << "\n";
    CHECK(link.starts == 1);
    CHECK(link.cancels == 0);
    CHECK(orch.generation() == 0);
    const auto due = policy.nextAttemptMs();
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    CHECK(link.cancels == 0);
    CHECK(link.starts >= 2);
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(0));
    link.failPoll = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    std::cout << "note poll_fail starts=" << link.starts
              << " cancels=" << link.cancels
              << " policy=" << static_cast<int>(policy.state()) << "\n";
    CHECK(link.cancels == 0);
    const auto due = policy.nextAttemptMs();
    link.failPoll = false;
    link.done = true;
    CHECK(orch.poll(due, true, 0) == OrchCode::Ok);
    std::cout << "note poll_fail_retry starts=" << link.starts
              << " cancels=" << link.cancels << "\n";
    CHECK(link.cancels == 0);
    CHECK(link.starts >= 2);
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(100));
    link.done = false;
    CHECK(orch.poll(100, true, 0) == OrchCode::Ok);
    CHECK(link.starts == 1 && link.cancels == 0);
    const auto rolled = orch.poll(50, true, 0);
    std::cout << "note orch_rollback code=" << static_cast<int>(rolled)
              << " cancels=" << link.cancels
              << " policy=" << static_cast<int>(policy.state()) << "\n";
    CHECK(link.cancels == 1);
  }
  {
    Device device;
    fixture::Clock clock;
    fixture::Executor exec;
    CommandSession session(device, clock, exec);
    NetworkInbox inbox;
    ReconnectPolicy policy;
    Link link;
    Pub pub;
    ReconnectOrchestrator orch(device, session, inbox, policy, link, pub);
    char scratch[TxBytes];
    CHECK(device.define(fixture::channel()) == Error::Ok);
    CHECK(device.allow("s", Operation::Sample) == Error::Ok);
    CHECK(session.begin(fixture::config()).code == SessionCode::Ok);
    CHECK(inbox.bind("lab", "node") == InboxCode::Ok);
    CHECK(orch.start(0));
    link.done = true;
    CHECK(orch.poll(0, true, 0) == OrchCode::Ok);
    pub.mode = Pub::Mode::Fail;
    const Sample sample = Sample::scalar(1.25, fixture::T);
    CHECK(orch.publishReading("s", sample, scratch, sizeof(scratch)) == OrchCode::Failed);
    CHECK(orch.health().readings == 0);
    pub.mode = Pub::Mode::Complete;
    const int before = pub.completed;
    CHECK(orch.poll(1, true, 0) == OrchCode::Ok);
    std::cout << "note failed_reading_retry completed_delta=" << (pub.completed - before)
              << " last_reading=" << kind(pub.last, "reading") << "\n";
  }
}
int main() {
  attackScheduler();
  attackDrivers();
  attackFdc();
  attackOrchestrator();
  attackCancelHoles();
  if (failures) {
    std::cerr << "checks=" << checks << " failures=" << failures << "\n";
    return 1;
  }
  std::cout << "PASS adversarial checks=" << checks << " failures=0\n";
  return 0;
}
