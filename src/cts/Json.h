// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace cts {
namespace detail {
bool text(const char *, size_t, bool identifier = false);
class Writer {
  char *p_;
  size_t cap_, n_;
  bool good_;

public:
  Writer(char *p, size_t cap) : p_(p), cap_(cap), n_(0), good_(p && cap) {
    if (good_)
      p_[0] = 0;
  }
  void raw(const char *);
  void string(const char *);
  void number(double);
  void integer(uint64_t);
  bool good() const { return good_; }
  size_t size() const { return n_; }

private:
  void ch(char);
};
} // namespace detail
} // namespace cts
