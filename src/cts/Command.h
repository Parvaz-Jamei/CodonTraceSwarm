// SPDX-License-Identifier: MIT
#pragma once
#include "../CodonTraceSwarm.h"
namespace cts {
constexpr size_t CommandBytes = 4096;
enum class CommandError {
  Ok,
  InvalidContext,
  TooLarge,
  MalformedJson,
  Capacity,
  DuplicateKey,
  Schema,
  Scope,
  Expired,
  Future,
  Unsupported,
  InvalidParameters
};

struct Command {
  char id[64] = {}, channel[64] = {}, sourceDigest[65] = {};
  Operation operation = Operation::Sample;
  uint64_t createdMs = 0, expiresMs = 0;
  double hz = 0, alpha = 0, clip = 0, eventThreshold = 0;
};
struct CommandContext {
  const char *swarmId = nullptr;
  const char *nodeId = nullptr;
  const char *bootId = nullptr;
  uint64_t nowMs = 0;
};
// Decode only: no command is advertised, authorized or executed by this class.
// Owns bounded scratch memory. Single-threaded; use long-lived/static storage
// on MCU. Output reset on any error; successful Command owns all strings and
// survives parser reuse.
class CommandDecoder {
public:
  CommandError decode(const char *, size_t, const CommandContext &, Command &);

private:
  enum Kind { Object, Array, String, Number, Boolean, Null };
  struct Token {
    Kind kind;
    int parent;
    size_t first, last;
  } tokens_[96];
  char strings_[CommandBytes + 1];
  size_t used_ = 0, count_ = 0, pos_ = 0, length_ = 0;
  const char *input_ = nullptr;
  CommandError error_ = CommandError::MalformedJson;
  void whitespace();
  bool literal(const char *);
  bool append(char);
  bool utf8(uint32_t);
  bool hex(uint32_t &);
  int quoted(int);
  int value(int, int);
  bool numberToken(int);
  bool equal(int, const char *) const;
  int field(int, const char *) const;
  bool exact(int, const char *const *, size_t) const;
  bool integer(int, uint64_t &) const;
  bool real(int, double &) const;
  bool copyIdentifier(int, char *, size_t) const;
};
// Separate registry/capability gate; caller must additionally check advertised
// operations.
CommandError validateChannel(const Command &, const ChannelSpec &);
const char *commandErrorName(CommandError);
const char *operationName(Operation);
} // namespace cts
