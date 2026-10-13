// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace cts {
static const uint64_t MaxWireInteger = 9007199254740991ULL;
static const size_t MaxChannels = 8, MaxValues = 256, MaxAxes = 16,
                    TxBytes = 8192;
enum class Error {
  Ok,
  InvalidArgument,
  Capacity,
  Frozen,
  NotStarted,
  InvalidSample,
  SequenceExhausted,
  Busy
};
enum class Operation {
  Sample,
  Raw,
  Features,
  Result,
  SetRate,
  Pipeline,
  Capsule
};
class CommandSession;
enum class ValueType { Scalar, Vector, Waveform, Boolean, Category };
enum class MeasurementModel { Direct, Derived, Proxy, Raw };
struct Knot {
  double x = 0, y = 0;
  Knot() {}
  Knot(double a, double b) : x(a), y(b) {}
};
struct Normalization {
  enum class Method { None, Affine, Table };
  Method method = Method::None;
  double scale = 1, offset = 0;
  const char *outputUnit = nullptr;
  const char *evidenceId = nullptr;
  const Knot *knots = nullptr;
  size_t knotCount = 0;
};
struct Descriptor {
  ValueType type = ValueType::Scalar;
  MeasurementModel model = MeasurementModel::Direct;
  const char *featureId = nullptr;
  const char *configId = nullptr;
  const char *sourceUnit = nullptr;
  const char *modelId = nullptr;
  const char *axes[MaxAxes] = {};
  size_t axisCount = 0;
  size_t length = 0;
  uint64_t samplePeriodUs = 0;
  bool hasRange = false;
  double minimum = 0, maximum = 0;
  bool hasRate = false;
  double minimumHz = 0, maximumHz = 0;
  Normalization normalization;
};
struct ChannelSpec {
  const char *name = nullptr;
  const char *quantity = nullptr;
  const char *unit = nullptr;
  Descriptor descriptor;
};
struct Sample {
  bool valid = false;
  const char *status = "not_ready";
  uint64_t utcMs = 0;
  double value = 0;
  bool boolean = false;
  double values[MaxValues] = {};
  size_t count = 0;
  const char *category = nullptr;
  static Sample scalar(double v, uint64_t utc) {
    Sample s;
    s.valid = true;
    s.status = "ok";
    s.utcMs = utc;
    s.value = v;
    return s;
  }
  static Sample invalid(const char *why, uint64_t utc) {
    Sample s;
    s.status = why;
    s.utcMs = utc;
    return s;
  }
};
struct SessionConfig {
  const char *swarmId = nullptr;
  const char *nodeId = nullptr;
  const char *bootId = nullptr;
  const char *processingVersion = "cts-rebuild-01";
  bool wifi = true;
};
// Fixed-capacity, single-threaded codec. No transport or command execution.
// Keep this object in static/global storage on constrained devices (34 KiB).
// define() deep-copies descriptor data; Sample pointers are call-scoped views.
// begin() freezes the registry; each later session requires a fresh boot ID.
// Successful serialization reserves a sequence: retry the same serialized
// bytes. The caller must deliver hello before readings, and enforce clock
// freshness.
class Device {
public:
  Device() = default;
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;
  Error define(const ChannelSpec &);
  // Explicit declaration by an installed application executor; default is none.
  Error allow(const char *channel, Operation);
  bool commandAllowed(const char *channel, Operation, double hz = 0) const;
  bool matchesSession(const char *swarm, const char *node,
                      const char *boot) const;
  Error begin(const SessionConfig &);
  Error hello(char *output, size_t capacity);
  Error reading(const char *channel, const Sample &, char *output,
                size_t capacity);
  Error heartbeat(char *output, size_t capacity);
  Error reply(const char *commandId, bool ok, const char *message, char *output,
              size_t capacity);
  void end() { started_ = false; }
  uint64_t activeSessionNumber() const { return started_ ? sessionNumber_ : 0; }
  size_t channelCount() const { return count_; }
  uint64_t nextSequence() const { return seq_; }

private:
  friend class CommandSession;
  bool txHeld_ = false;
  struct Channel {
    char name[64] = {}, config[64] = {}, json[2048] = {};
    ValueType type = ValueType::Scalar;
    size_t count = 0;
    bool hasRate = false;
    double minimumHz = 0, maximumHz = 0;
    uint8_t operations = 0;
    bool range = false;
    double lo = 0, hi = 0;
    Normalization::Method norm = Normalization::Method::None;
    double scale = 1, offset = 0;
    Knot knots[64];
    size_t knotCount = 0;
  };
  char payload_[TxBytes] = {};
  Channel channels_[MaxChannels];
  size_t count_ = 0;
  bool started_ = false, frozen_ = false, exhausted_ = false;
  uint64_t seq_ = 0, sessionNumber_ = 0;
  char swarm_[64] = {}, node_[64] = {}, boot_[64] = {}, version_[129] = {};
  Error emit(const char *kind, const char *payload, char *, size_t);
  bool numberValid(const Channel &, double) const;
};
const char *errorName(Error);
} // namespace cts
