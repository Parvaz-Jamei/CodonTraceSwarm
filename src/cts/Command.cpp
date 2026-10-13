// SPDX-License-Identifier: MIT
#include "Command.h"
#include "Json.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace cts {
namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool scalar(uint32_t cp) {
  return cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}
} // namespace
const char *commandErrorName(CommandError e) {
  switch (e) {
  case CommandError::Ok:
    return "ok";
  case CommandError::InvalidContext:
    return "invalid_context";
  case CommandError::TooLarge:
    return "too_large";
  case CommandError::MalformedJson:
    return "malformed_json";
  case CommandError::Capacity:
    return "capacity";
  case CommandError::DuplicateKey:
    return "duplicate_key";
  case CommandError::Schema:
    return "schema";
  case CommandError::Scope:
    return "scope";
  case CommandError::Expired:
    return "expired";
  case CommandError::Future:
    return "future";
  case CommandError::Unsupported:
    return "unsupported";
  case CommandError::InvalidParameters:
    return "invalid_parameters";
  }
  return "unknown";
}
const char *operationName(Operation o) {
  switch (o) {
  case Operation::Sample:
    return "sample";
  case Operation::Raw:
    return "raw";
  case Operation::Features:
    return "features";
  case Operation::Result:
    return "result";
  case Operation::SetRate:
    return "set_rate";
  case Operation::Pipeline:
    return "pipeline";
  case Operation::Capsule:
    return "capsule";
  }
  return "unknown";
}
void CommandDecoder::whitespace() {
  while (pos_ < length_ && (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                            input_[pos_] == '\r' || input_[pos_] == '\n'))
    ++pos_;
}
bool CommandDecoder::literal(const char *s) {
  size_t n = std::strlen(s);
  if (n > length_ - pos_ || std::memcmp(input_ + pos_, s, n))
    return false;
  pos_ += n;
  return true;
}
bool CommandDecoder::append(char c) {
  if (used_ >= sizeof(strings_)) {
    error_ = CommandError::Capacity;
    return false;
  }
  strings_[used_++] = c;
  return true;
}
bool CommandDecoder::utf8(uint32_t c) {
  if (!scalar(c) || c == 0)
    return false;
  if (c < 0x80)
    return append(char(c));
  if (c < 0x800)
    return append(char(0xc0 | (c >> 6))) && append(char(0x80 | (c & 63)));
  if (c < 0x10000)
    return append(char(0xe0 | (c >> 12))) &&
           append(char(0x80 | ((c >> 6) & 63))) &&
           append(char(0x80 | (c & 63)));
  return append(char(0xf0 | (c >> 18))) &&
         append(char(0x80 | ((c >> 12) & 63))) &&
         append(char(0x80 | ((c >> 6) & 63))) && append(char(0x80 | (c & 63)));
}
bool CommandDecoder::hex(uint32_t &c) {
  c = 0;
  if (length_ - pos_ < 4)
    return false;
  for (int i = 0; i < 4; ++i) {
    char h = input_[pos_++];
    unsigned d;
    if (h >= '0' && h <= '9')
      d = h - '0';
    else if (h >= 'a' && h <= 'f')
      d = h - 'a' + 10;
    else if (h >= 'A' && h <= 'F')
      d = h - 'A' + 10;
    else
      return false;
    c = (c << 4) | d;
  }
  return true;
}
int CommandDecoder::quoted(int parent) {
  if (count_ >= 96) {
    error_ = CommandError::Capacity;
    return -1;
  }
  if (pos_ >= length_ || input_[pos_++] != '"')
    return -1;
  int t = int(count_++);
  tokens_[t] = {String, parent, used_, 0};
  while (pos_ < length_) {
    unsigned c = static_cast<unsigned char>(input_[pos_++]);
    if (c == '"') {
      tokens_[t].last = used_;
      return append(0) ? t : -1;
    }
    if (c < 32)
      return -1;
    if (c == '\\') {
      if (pos_ >= length_)
        return -1;
      char esc = input_[pos_++];
      if (esc == '"' || esc == '\\' || esc == '/') {
        if (!append(esc))
          return -1;
      } else if (esc == 'b' || esc == 'f' || esc == 'n' || esc == 'r' ||
                 esc == 't') {
        char v = esc == 'b'   ? '\b'
                 : esc == 'f' ? '\f'
                 : esc == 'n' ? '\n'
                 : esc == 'r' ? '\r'
                              : '\t';
        if (!append(v))
          return -1;
      } else if (esc == 'u') {
        uint32_t cp;
        if (!hex(cp))
          return -1;
        if (cp >= 0xd800 && cp <= 0xdbff) {
          if (length_ - pos_ < 2 || input_[pos_] != '\\' ||
              input_[pos_ + 1] != 'u')
            return -1;
          pos_ += 2;
          uint32_t low;
          if (!hex(low) || low < 0xdc00 || low > 0xdfff)
            return -1;
          cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
        }
        if (!utf8(cp))
          return -1;
      } else
        return -1;
    } else if (c < 128) {
      if (!append(char(c)))
        return -1;
    } else {
      uint32_t cp;
      unsigned remaining;
      uint32_t minimum;
      if (c >= 0xc2 && c <= 0xdf) {
        cp = c & 31;
        remaining = 1;
        minimum = 0x80;
      } else if (c >= 0xe0 && c <= 0xef) {
        cp = c & 15;
        remaining = 2;
        minimum = 0x800;
      } else if (c >= 0xf0 && c <= 0xf4) {
        cp = c & 7;
        remaining = 3;
        minimum = 0x10000;
      } else
        return -1;
      while (remaining--) {
        if (pos_ >= length_)
          return -1;
        unsigned b = static_cast<unsigned char>(input_[pos_++]);
        if ((b & 0xc0) != 0x80)
          return -1;
        cp = (cp << 6) | (b & 63);
      }
      if (cp < minimum || !utf8(cp))
        return -1;
    }
  }
  return -1;
}
bool CommandDecoder::numberToken(int t) {
  size_t start = pos_;
  if (input_[pos_] == '-')
    ++pos_;
  if (pos_ >= length_)
    return false;
  if (input_[pos_] == '0')
    ++pos_;
  else {
    if (input_[pos_] < '1' || input_[pos_] > '9')
      return false;
    while (pos_ < length_ && digit(input_[pos_]))
      ++pos_;
  }
  if (pos_ < length_ && input_[pos_] == '.') {
    ++pos_;
    size_t before = pos_;
    while (pos_ < length_ && digit(input_[pos_]))
      ++pos_;
    if (pos_ == before)
      return false;
  }
  if (pos_ < length_ && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
    ++pos_;
    if (pos_ < length_ && (input_[pos_] == '+' || input_[pos_] == '-'))
      ++pos_;
    size_t before = pos_;
    while (pos_ < length_ && digit(input_[pos_]))
      ++pos_;
    if (pos_ == before)
      return false;
  }
  tokens_[t].first = start;
  tokens_[t].last = pos_;
  return true;
}
int CommandDecoder::value(int parent, int depth) {
  whitespace();
  if (pos_ >= length_)
    return -1;
  if (depth > 8 || count_ >= 96) {
    error_ = CommandError::Capacity;
    return -1;
  }
  char c = input_[pos_];
  if (c == '"')
    return quoted(parent);
  int t = int(count_++);
  tokens_[t] = {Null, parent, pos_, 0};
  if (c == '{' || c == '[') {
    tokens_[t].kind = c == '{' ? Object : Array;
    ++pos_;
    whitespace();
    char end = c == '{' ? '}' : ']';
    if (pos_ < length_ && input_[pos_] == end) {
      ++pos_;
      return t;
    }
    while (true) {
      if (c == '{') {
        int key = quoted(t);
        if (key < 0)
          return -1;
        for (int i = t + 1; i < key; ++i)
          if (tokens_[i].parent == t && tokens_[i].kind == String &&
              !std::strcmp(strings_ + tokens_[i].first,
                           strings_ + tokens_[key].first)) {
            error_ = CommandError::DuplicateKey;
            return -1;
          }
        whitespace();
        if (pos_ >= length_ || input_[pos_++] != ':')
          return -1;
        if (value(key, depth + 1) < 0)
          return -1;
      } else if (value(t, depth + 1) < 0)
        return -1;
      whitespace();
      if (pos_ >= length_)
        return -1;
      if (input_[pos_] == end) {
        ++pos_;
        return t;
      }
      if (input_[pos_++] != ',')
        return -1;
      whitespace();
    }
  }
  if (c == 't' || c == 'f') {
    tokens_[t].kind = Boolean;
    return literal(c == 't' ? "true" : "false") ? t : -1;
  }
  if (c == 'n')
    return literal("null") ? t : -1;
  tokens_[t].kind = Number;
  return numberToken(t) ? t : -1;
}
bool CommandDecoder::equal(int t, const char *s) const {
  return t >= 0 && size_t(t) < count_ && tokens_[t].kind == String &&
         !std::strcmp(strings_ + tokens_[t].first, s);
}
int CommandDecoder::field(int t, const char *s) const {
  if (t < 0 || size_t(t) >= count_ || tokens_[t].kind != Object)
    return -1;
  for (size_t i = size_t(t) + 1; i < count_; ++i)
    if (tokens_[i].parent == t && equal(int(i), s))
      return i + 1 < count_ ? int(i + 1) : -1;
  return -1;
}
bool CommandDecoder::exact(int t, const char *const *keys, size_t n) const {
  if (t < 0 || size_t(t) >= count_ || tokens_[t].kind != Object)
    return false;
  size_t found = 0;
  for (size_t i = size_t(t) + 1; i < count_; ++i)
    if (tokens_[i].parent == t)
      ++found;
  if (found != n)
    return false;
  for (size_t i = 0; i < n; ++i)
    if (field(t, keys[i]) < 0)
      return false;
  return true;
}
bool CommandDecoder::integer(int t, uint64_t &v) const {
  if (t < 0 || size_t(t) >= count_ || tokens_[t].kind != Number)
    return false;
  v = 0;
  for (size_t i = tokens_[t].first; i < tokens_[t].last; ++i) {
    char c = input_[i];
    if (!digit(c))
      return false;
    unsigned d = c - '0';
    if (v > (MaxWireInteger - d) / 10)
      return false;
    v = v * 10 + d;
  }
  return true;
}
bool CommandDecoder::real(int t, double &v) const {
  if (t < 0 || size_t(t) >= count_ || tokens_[t].kind != Number)
    return false;
  size_t n = tokens_[t].last - tokens_[t].first;
  if (!n || n >= 64)
    return false;
  char b[64];
  std::memcpy(b, input_ + tokens_[t].first, n);
  b[n] = 0;
  char *end;
  v = std::strtod(b, &end);
  return end == b + n && std::isfinite(v) && v >= -1e12 && v <= 1e12;
}
bool CommandDecoder::copyIdentifier(int t, char *out, size_t cap) const {
  if (t < 0 || size_t(t) >= count_ || tokens_[t].kind != String)
    return false;
  const char *s = strings_ + tokens_[t].first;
  size_t n = tokens_[t].last - tokens_[t].first;
  if (n >= cap || !detail::text(s, 63, true))
    return false;
  std::memcpy(out, s, n + 1);
  return true;
}
CommandError CommandDecoder::decode(const char *bytes, size_t n,
                                    const CommandContext &ctx, Command &out) {
  out = Command{};
  used_ = count_ = pos_ = 0;
  input_ = bytes;
  length_ = n;
  error_ = CommandError::MalformedJson;
  if (!detail::text(ctx.swarmId, 63, true) ||
      !detail::text(ctx.nodeId, 63, true) ||
      !detail::text(ctx.bootId, 63, true) || !ctx.nowMs ||
      ctx.nowMs > MaxWireInteger)
    return CommandError::InvalidContext;
  if (n > CommandBytes)
    return CommandError::TooLarge;
  if (!bytes || !n || std::memchr(bytes, 0, n))
    return CommandError::MalformedJson;
  if (value(-1, 0) != 0)
    return error_;
  whitespace();
  if (pos_ != n)
    return CommandError::MalformedJson;
  const char *keys[] = {"schema",     "swarm_id",  "node_id", "boot_id",
                        "command_id", "operation", "params",  "created_ms",
                        "expires_ms", "status"};
  if (!exact(0, keys, 10) ||
      !equal(field(0, "schema"), "genesis_swarm_command_v1") ||
      !equal(field(0, "status"), "pending"))
    return CommandError::Schema;
  if (!equal(field(0, "swarm_id"), ctx.swarmId) ||
      !equal(field(0, "node_id"), ctx.nodeId) ||
      !equal(field(0, "boot_id"), ctx.bootId))
    return CommandError::Scope;
  Command c;
  if (!copyIdentifier(field(0, "command_id"), c.id, sizeof(c.id)) ||
      !integer(field(0, "created_ms"), c.createdMs) ||
      !integer(field(0, "expires_ms"), c.expiresMs))
    return CommandError::Schema;
  if (c.expiresMs <= c.createdMs || c.expiresMs - c.createdMs > 60000)
    return CommandError::InvalidParameters;
  if (c.createdMs > ctx.nowMs)
    return CommandError::Future;
  if (c.expiresMs <= ctx.nowMs)
    return CommandError::Expired;
  int op = field(0, "operation");
  bool recognized = false;
  for (int i = 0; i < 7; ++i)
    if (equal(op, operationName(static_cast<Operation>(i)))) {
      c.operation = static_cast<Operation>(i);
      recognized = true;
      break;
    }
  if (!recognized)
    return CommandError::Unsupported;
  int params = field(0, "params");
  if (!copyIdentifier(field(params, "channel"), c.channel, sizeof(c.channel)))
    return CommandError::InvalidParameters;
  const char *simple[] = {"channel"};
  const char *rate[] = {"channel", "hz"};
  const char *policy[] = {"channel", "pipeline", "source_digest"};
  if (c.operation == Operation::SetRate) {
    if (!exact(params, rate, 2) || !real(field(params, "hz"), c.hz) ||
        c.hz < .01 || c.hz > 10000)
      return CommandError::InvalidParameters;
  } else if (c.operation == Operation::Pipeline ||
             c.operation == Operation::Capsule) {
    int digest = field(params, "source_digest");
    if (!exact(params, policy, digest < 0 ? 2 : 3))
      return CommandError::InvalidParameters;
    if (digest >= 0) {
      if (tokens_[digest].kind != String ||
          tokens_[digest].last - tokens_[digest].first != 64)
        return CommandError::InvalidParameters;
      const char *s = strings_ + tokens_[digest].first;
      for (size_t i = 0; i < 64; ++i)
        if (!(s[i] >= '0' && s[i] <= '9') && !(s[i] >= 'a' && s[i] <= 'f'))
          return CommandError::InvalidParameters;
      std::memcpy(c.sourceDigest, s, 65);
    }
    int config = field(params, "pipeline");
    const char *pk[] = {"alpha", "clip", "event_threshold"};
    if (!exact(config, pk, 3) || !real(field(config, "alpha"), c.alpha) ||
        !real(field(config, "clip"), c.clip) ||
        !real(field(config, "event_threshold"), c.eventThreshold) ||
        c.alpha < .01 || c.alpha > 1 || c.clip < .01 || c.clip > 1e6 ||
        c.eventThreshold < .01 || c.eventThreshold > 1e6)
      return CommandError::InvalidParameters;
  } else if (!exact(params, simple, 1))
    return CommandError::InvalidParameters;
  out = c;
  return CommandError::Ok;
}
CommandError validateChannel(const Command &c, const ChannelSpec &s) {
  if (!s.name || std::strcmp(c.channel, s.name))
    return CommandError::InvalidParameters;
  if (c.operation == Operation::SetRate) {
    if (!std::isfinite(c.hz) || c.hz < .01 || c.hz > 10000)
      return CommandError::InvalidParameters;
    if (s.descriptor.hasRate &&
        (!std::isfinite(s.descriptor.minimumHz) ||
         !std::isfinite(s.descriptor.maximumHz) ||
         s.descriptor.minimumHz <= 0 ||
         s.descriptor.maximumHz < s.descriptor.minimumHz ||
         s.descriptor.maximumHz > 10000 || c.hz < s.descriptor.minimumHz ||
         c.hz > s.descriptor.maximumHz))
      return CommandError::InvalidParameters;
  }
  if ((c.operation == Operation::Pipeline ||
       c.operation == Operation::Capsule) &&
      s.descriptor.type != ValueType::Scalar)
    return CommandError::InvalidParameters;
  return CommandError::Ok;
}
} // namespace cts
