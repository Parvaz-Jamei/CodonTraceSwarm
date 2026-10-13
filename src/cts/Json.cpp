// SPDX-License-Identifier: MIT
#include "Json.h"
#include <cmath>
#include <cstdio>
#include <cstring>
namespace cts {
namespace detail {
namespace {
bool white(uint32_t cp) {
  return (cp >= 9 && cp <= 13) || (cp >= 0x1c && cp <= 0x20) || cp == 0x85 ||
         cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
         cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f ||
         cp == 0x3000;
}
} // namespace
bool text(const char *p, size_t max, bool id) {
  if (!p || !*p)
    return false;
  size_t n = 0;
  uint32_t cp = 0;
  unsigned remaining = 0;
  uint32_t lower = 0;
  bool visible = false;
  for (; p[n]; ++n) {
    if (n >= max)
      return false;
    unsigned c = static_cast<unsigned char>(p[n]);
    if (id) {
      bool al = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9');
      if (!al && !(n && (c == '_' || c == '-')))
        return false;
      visible = true;
      continue;
    }
    if (remaining) {
      if ((c & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (c & 63);
      if (!--remaining &&
          (cp < lower || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)))
        return false;
      if (!remaining && !white(cp))
        visible = true;
    } else if (c < 0x80) {
      if (!white(c))
        visible = true;
    } else if (c >= 0xc2 && c <= 0xdf) {
      cp = c & 31;
      remaining = 1;
      lower = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
      cp = c & 15;
      remaining = 2;
      lower = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
      cp = c & 7;
      remaining = 3;
      lower = 0x10000;
    } else
      return false;
  }
  return remaining == 0 && visible;
}
void Writer::ch(char c) {
  if (!good_)
    return;
  if (n_ + 1 >= cap_) {
    good_ = false;
    p_[0] = 0;
    return;
  }
  p_[n_++] = c;
  p_[n_] = 0;
}
void Writer::raw(const char *s) {
  if (!s) {
    good_ = false;
    if (p_ && cap_)
      p_[0] = 0;
    return;
  }
  while (*s)
    ch(*s++);
}
void Writer::string(const char *s) {
  if (!s) {
    raw("null");
    return;
  }
  ch('"');
  for (; *s; ++s) {
    unsigned c = static_cast<unsigned char>(*s);
    if (c == '"' || c == '\\') {
      ch('\\');
      ch(static_cast<char>(c));
    } else if (c < 32) {
      char b[7];
      std::snprintf(b, sizeof(b), "\\u%04x", c);
      raw(b);
    } else
      ch(static_cast<char>(c));
  }
  ch('"');
}
void Writer::number(double x) {
  if (!std::isfinite(x)) {
    good_ = false;
    if (p_ && cap_)
      p_[0] = 0;
    return;
  }
  char b[48];
  int n = std::snprintf(b, sizeof(b), "%.17g", x);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(b) || std::strchr(b, ',')) {
    good_ = false;
    if (p_ && cap_)
      p_[0] = 0;
    return;
  }
  raw(b);
}
void Writer::integer(uint64_t x) {
  char b[32];
  std::snprintf(b, sizeof(b), "%llu", static_cast<unsigned long long>(x));
  raw(b);
}
} // namespace detail
} // namespace cts
