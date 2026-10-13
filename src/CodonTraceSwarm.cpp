// SPDX-License-Identifier: MIT
#include "CodonTraceSwarm.h"
#include "cts/Json.h"
#include <cmath>
#include <cstring>
namespace cts {
using detail::text;
using detail::Writer;
namespace {
bool finite(double n) { return std::isfinite(n) && n >= -1e12 && n <= 1e12; }
void field(Writer &w, const char *k, const char *v) {
  w.string(k);
  w.raw(":");
  w.string(v);
}
void copy(char *d, const char *s) { std::strcpy(d, s); }
void clear(char *p, size_t c) {
  if (p && c)
    p[0] = 0;
}
} // namespace
const char *errorName(Error e) {
  switch (e) {
  case Error::Busy:
    return "busy";
  case Error::Ok:
    return "ok";
  case Error::InvalidArgument:
    return "invalid_argument";
  case Error::Capacity:
    return "capacity";
  case Error::Frozen:
    return "frozen";
  case Error::NotStarted:
    return "not_started";
  case Error::InvalidSample:
    return "invalid_sample";
  case Error::SequenceExhausted:
    return "sequence_exhausted";
  }
  return "unknown";
}
Error Device::define(const ChannelSpec &s) {
  if (frozen_)
    return Error::Frozen;
  if (count_ == MaxChannels)
    return Error::Capacity;
  const Descriptor &d = s.descriptor;
  const Normalization &n = d.normalization;
  if (!text(s.name, 63, true) || !text(s.quantity, 96) || !text(s.unit, 64) ||
      !text(d.featureId, 63, true) || !text(d.configId, 63, true) ||
      !text(d.sourceUnit, 64))
    return Error::InvalidArgument;
  for (size_t i = 0; i < count_; ++i)
    if (!std::strcmp(channels_[i].name, s.name))
      return Error::InvalidArgument;
  if (static_cast<int>(d.type) < 0 || static_cast<int>(d.type) > 4 ||
      static_cast<int>(d.model) < 0 || static_cast<int>(d.model) > 3)
    return Error::InvalidArgument;
  if ((d.model == MeasurementModel::Derived ||
       d.model == MeasurementModel::Proxy) &&
      !text(d.modelId, 96))
    return Error::InvalidArgument;
  if (d.modelId && !text(d.modelId, 96))
    return Error::InvalidArgument;
  if (d.type == ValueType::Vector) {
    if (d.axisCount < 1 || d.axisCount > MaxAxes || d.length ||
        d.samplePeriodUs)
      return Error::InvalidArgument;
    for (size_t i = 0; i < d.axisCount; ++i) {
      if (!text(d.axes[i], 63, true))
        return Error::InvalidArgument;
      for (size_t j = 0; j < i; ++j)
        if (!std::strcmp(d.axes[i], d.axes[j]))
          return Error::InvalidArgument;
    }
  } else if (d.type == ValueType::Waveform) {
    if (d.axisCount || d.length < 1 || d.length > MaxValues ||
        d.samplePeriodUs < 1 || d.samplePeriodUs > 3600000000ULL)
      return Error::InvalidArgument;
  } else if (d.axisCount || d.length || d.samplePeriodUs)
    return Error::InvalidArgument;
  const bool numeric =
      d.type != ValueType::Boolean && d.type != ValueType::Category;
  if (d.hasRange && (!numeric || !finite(d.minimum) || !finite(d.maximum) ||
                     d.minimum > d.maximum))
    return Error::InvalidArgument;
  if (d.hasRate &&
      (!std::isfinite(d.minimumHz) || !std::isfinite(d.maximumHz) ||
       d.minimumHz <= 0 || d.maximumHz < d.minimumHz || d.maximumHz > 10000))
    return Error::InvalidArgument;
  if (n.method == Normalization::Method::None) {
    if (std::strcmp(s.unit, d.sourceUnit))
      return Error::InvalidArgument;
  } else {
    if (!numeric || !text(n.outputUnit, 64) || !text(n.evidenceId, 63, true) ||
        std::strcmp(n.outputUnit, s.unit))
      return Error::InvalidArgument;
    if (n.method == Normalization::Method::Affine) {
      if (!finite(n.scale) || n.scale == 0 || !finite(n.offset))
        return Error::InvalidArgument;
    } else if (n.method == Normalization::Method::Table) {
      if (!n.knots || n.knotCount < 2 || n.knotCount > 64)
        return Error::InvalidArgument;
      for (size_t i = 0; i < n.knotCount; ++i)
        if (!finite(n.knots[i].x) || !finite(n.knots[i].y) ||
            (i && n.knots[i].x <= n.knots[i - 1].x))
          return Error::InvalidArgument;
    } else
      return Error::InvalidArgument;
  }
  Channel &c = channels_[count_];
  copy(c.name, s.name);
  copy(c.config, d.configId);
  c.type = d.type;
  c.hasRate = d.hasRate;
  c.minimumHz = d.minimumHz;
  c.maximumHz = d.maximumHz;
  c.operations = 0;
  c.count = d.type == ValueType::Vector ? d.axisCount : d.length;
  c.range = d.hasRange;
  c.lo = d.minimum;
  c.hi = d.maximum;
  c.norm = n.method;
  c.scale = n.scale;
  c.offset = n.offset;
  c.knotCount = n.method == Normalization::Method::Table ? n.knotCount : 0;
  for (size_t i = 0; i < c.knotCount; ++i)
    c.knots[i] = n.knots[i];
  Writer w(c.json, sizeof(c.json));
  w.raw("{");
  field(w, "quantity", s.quantity);
  w.raw(",");
  field(w, "unit", s.unit);
  w.raw(",\"descriptor\":{");
  const char *types[] = {"scalar", "vector", "waveform", "boolean", "category"};
  const char *models[] = {"direct", "derived", "proxy", "raw"};
  field(w, "value_type", types[static_cast<int>(d.type)]);
  w.raw(",");
  field(w, "measurement_model", models[static_cast<int>(d.model)]);
  w.raw(",");
  field(w, "feature_id", d.featureId);
  w.raw(",");
  field(w, "config_id", d.configId);
  w.raw(",");
  field(w, "source_unit", d.sourceUnit);
  if (d.modelId) {
    w.raw(",");
    field(w, "model_id", d.modelId);
  }
  if (d.type == ValueType::Vector) {
    w.raw(",\"axes\":[");
    for (size_t i = 0; i < d.axisCount; ++i) {
      if (i)
        w.raw(",");
      w.string(d.axes[i]);
    }
    w.raw("]");
  }
  if (d.type == ValueType::Waveform) {
    w.raw(",\"length\":");
    w.integer(d.length);
    w.raw(",\"sample_period_us\":");
    w.integer(d.samplePeriodUs);
  }
  if (d.hasRange) {
    w.raw(",\"range\":[");
    w.number(d.minimum);
    w.raw(",");
    w.number(d.maximum);
    w.raw("]");
  }
  if (d.hasRate) {
    w.raw(",\"rate_hz\":[");
    w.number(d.minimumHz);
    w.raw(",");
    w.number(d.maximumHz);
    w.raw("]");
  }
  if (n.method != Normalization::Method::None) {
    w.raw(",\"normalization\":{");
    field(w, "method",
          n.method == Normalization::Method::Affine ? "affine" : "table");
    w.raw(",");
    field(w, "output_unit", n.outputUnit);
    w.raw(",");
    field(w, "evidence_id", n.evidenceId);
    if (n.method == Normalization::Method::Affine) {
      w.raw(",\"scale\":");
      w.number(n.scale);
      w.raw(",\"offset\":");
      w.number(n.offset);
    } else {
      w.raw(",\"knots\":[");
      for (size_t i = 0; i < n.knotCount; ++i) {
        if (i)
          w.raw(",");
        w.raw("[");
        w.number(n.knots[i].x);
        w.raw(",");
        w.number(n.knots[i].y);
        w.raw("]");
      }
      w.raw("]");
    }
    w.raw("}");
  }
  w.raw("}}");
  if (!w.good())
    return Error::Capacity;
  ++count_;
  return Error::Ok;
}
Error Device::allow(const char *name, Operation op) {
  if (frozen_)
    return Error::Frozen;
  const int index = static_cast<int>(op);
  if (!text(name, 63, true) || index < 0 || index > 6)
    return Error::InvalidArgument;
  for (size_t i = 0; i < count_; ++i) {
    auto &c = channels_[i];
    if (std::strcmp(name, c.name))
      continue;
    if ((op == Operation::Pipeline || op == Operation::Capsule) &&
        c.type != ValueType::Scalar)
      return Error::InvalidArgument;
    c.operations |= static_cast<uint8_t>(1u << index);
    return Error::Ok;
  }
  return Error::InvalidArgument;
}
bool Device::commandAllowed(const char *name, Operation op, double hz) const {
  const int index = static_cast<int>(op);
  if (!started_ || !text(name, 63, true) || index < 0 || index > 6)
    return false;
  for (size_t i = 0; i < count_; ++i) {
    const auto &c = channels_[i];
    if (std::strcmp(name, c.name))
      continue;
    if (!(c.operations & (1u << index)))
      return false;
    if (op == Operation::SetRate &&
        (!std::isfinite(hz) || hz < .01 || hz > 10000 ||
         (c.hasRate && (hz < c.minimumHz || hz > c.maximumHz))))
      return false;
    return true;
  }
  return false;
}
bool Device::matchesSession(const char *s, const char *n, const char *b) const {
  return started_ && s && n && b && !std::strcmp(s, swarm_) &&
         !std::strcmp(n, node_) && !std::strcmp(b, boot_);
}
Error Device::reply(const char *id, bool ok, const char *message, char *out,
                    size_t cap) {
  clear(out, cap);
  if (!started_)
    return Error::NotStarted;
  if (!text(id, 63, true) || (message && !text(message, 256)))
    return Error::InvalidArgument;
  Writer w(payload_, sizeof(payload_));
  w.raw("{\"command_id\":");
  w.string(id);
  w.raw(",\"ok\":");
  w.raw(ok ? "true" : "false");
  if (message) {
    w.raw(",\"message\":");
    w.string(message);
  }
  w.raw("}");
  if (!w.good())
    return Error::Capacity;
  return emit("reply", payload_, out, cap);
}
Error Device::begin(const SessionConfig &s) {
  if (txHeld_)
    return Error::Busy;
  if (started_)
    return Error::Frozen;
  if (!s.wifi || !count_ || !text(s.swarmId, 63, true) ||
      !text(s.nodeId, 63, true) || !text(s.bootId, 63, true) ||
      !text(s.processingVersion, 96))
    return Error::InvalidArgument;
  if (frozen_ && !std::strcmp(boot_, s.bootId))
    return Error::InvalidArgument;
  // Reserve envelope, including a full-width future sequence. No multipart
  // hello.
  size_t bytes = 512 + std::strlen(s.swarmId) + std::strlen(s.nodeId) +
                 std::strlen(s.bootId);
  for (size_t i = 0; i < count_; ++i)
    bytes +=
        std::strlen(channels_[i].name) + std::strlen(channels_[i].json) + 4;
  if (bytes >= TxBytes)
    return Error::Capacity;
  copy(swarm_, s.swarmId);
  copy(node_, s.nodeId);
  copy(boot_, s.bootId);
  copy(version_, s.processingVersion);
  seq_ = 0;
  exhausted_ = false;
  started_ = true;
  frozen_ = true;
  return Error::Ok;
}
Error Device::emit(const char *kind, const char *payload, char *output,
                   size_t cap) {
  clear(output, cap);
  if (!started_)
    return Error::NotStarted;
  if (txHeld_)
    return Error::Busy;
  if (exhausted_)
    return Error::SequenceExhausted;
  Writer w(output, cap < TxBytes ? cap : TxBytes);
  w.raw("{\"schema\":\"genesis_swarm_v2\",");
  field(w, "swarm_id", swarm_);
  w.raw(",");
  field(w, "node_id", node_);
  w.raw(",");
  field(w, "boot_id", boot_);
  w.raw(",\"seq\":");
  w.integer(seq_);
  w.raw(",");
  field(w, "kind", kind);
  w.raw(",\"payload\":");
  w.raw(payload);
  w.raw("}");
  if (!w.good())
    return Error::Capacity;
  if (seq_ == MaxWireInteger)
    exhausted_ = true;
  else
    ++seq_;
  return Error::Ok;
}
Error Device::hello(char *out, size_t cap) {
  Writer w(payload_, sizeof(payload_));
  w.raw("{\"wifi\":true,\"channels\":{");
  for (size_t i = 0; i < count_; ++i) {
    if (i)
      w.raw(",");
    w.string(channels_[i].name);
    w.raw(":");
    w.raw(channels_[i].json);
  }
  w.raw("},\"operations\":[");
  uint8_t all = 0;
  for (size_t i = 0; i < count_; ++i)
    all |= channels_[i].operations;
  const char *names[] = {"sample",   "raw",      "features", "result",
                         "set_rate", "pipeline", "capsule"};
  bool first = true;
  for (unsigned i = 0; i < 7; ++i)
    if (all & (1u << i)) {
      if (!first)
        w.raw(",");
      w.string(names[i]);
      first = false;
    }
  w.raw("]}");
  if (!w.good()) {
    clear(out, cap);
    return Error::Capacity;
  }
  return emit("hello", payload_, out, cap);
}
bool Device::numberValid(const Channel &c, double v) const {
  if (!finite(v))
    return false;
  double y = v;
  if (c.norm == Normalization::Method::Affine)
    y = v * c.scale + c.offset;
  else if (c.norm == Normalization::Method::Table) {
    if (v < c.knots[0].x || v > c.knots[c.knotCount - 1].x)
      return false;
    size_t i = 1;
    while (i < c.knotCount - 1 && v > c.knots[i].x)
      ++i;
    const Knot &a = c.knots[i - 1], &b = c.knots[i];
    y = a.y + (v - a.x) / (b.x - a.x) * (b.y - a.y);
  }
  return finite(y) && (!c.range || (y >= c.lo && y <= c.hi));
}
Error Device::reading(const char *name, const Sample &s, char *out,
                      size_t cap) {
  clear(out, cap);
  if (!started_)
    return Error::NotStarted;
  if (!text(name, 63, true))
    return Error::InvalidArgument;
  const Channel *c = nullptr;
  for (size_t i = 0; i < count_; ++i)
    if (!std::strcmp(name, channels_[i].name))
      c = &channels_[i];
  if (!c)
    return Error::InvalidArgument;
  if (!s.utcMs || s.utcMs > MaxWireInteger || !text(s.status, 64) ||
      (s.valid ? std::strcmp(s.status, "ok") != 0
               : std::strcmp(s.status, "ok") == 0))
    return Error::InvalidSample;
  if (s.valid) {
    if (c->type == ValueType::Scalar && !numberValid(*c, s.value))
      return Error::InvalidSample;
    if (c->type == ValueType::Vector || c->type == ValueType::Waveform) {
      if (s.count != c->count)
        return Error::InvalidSample;
      for (size_t i = 0; i < s.count; ++i)
        if (!numberValid(*c, s.values[i]))
          return Error::InvalidSample;
    }
    if (c->type == ValueType::Category && !text(s.category, 128))
      return Error::InvalidSample;
  }
  Writer w(payload_, sizeof(payload_));
  w.raw("{");
  field(w, "channel", name);
  w.raw(",\"sample_ms\":");
  w.integer(s.utcMs);
  w.raw(",\"value\":");
  if (!s.valid)
    w.raw("null");
  else
    switch (c->type) {
    case ValueType::Scalar:
      w.number(s.value);
      break;
    case ValueType::Boolean:
      w.raw(s.boolean ? "true" : "false");
      break;
    case ValueType::Category:
      w.string(s.category);
      break;
    default:
      w.raw("[");
      for (size_t i = 0; i < s.count; ++i) {
        if (i)
          w.raw(",");
        w.number(s.values[i]);
      }
      w.raw("]");
    }
  w.raw(",");
  field(w, "processing_version", version_);
  w.raw(",\"valid\":");
  w.raw(s.valid ? "true" : "false");
  w.raw(",");
  field(w, "status", s.status);
  w.raw(",");
  field(w, "config_id", c->config);
  w.raw("}");
  if (!w.good())
    return Error::Capacity;
  return emit("reading", payload_, out, cap);
}
Error Device::heartbeat(char *out, size_t cap) {
  return emit("heartbeat", "{}", out, cap);
}
} // namespace cts
