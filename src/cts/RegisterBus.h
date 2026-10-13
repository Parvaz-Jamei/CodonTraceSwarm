// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace cts {
// Tiny injected bus for mock drivers. This is not Wire, not I2C, and not a
// claim that a board transaction succeeded. read/write/fail only.
enum class BusStatus { Ok, Fail, NotReady };
class ByteBus {
public:
  virtual ~ByteBus() = default;
  virtual BusStatus write(uint8_t address, const uint8_t *data,
                          size_t n) noexcept = 0;
  virtual BusStatus read(uint8_t address, uint8_t *data, size_t n) noexcept = 0;
};
} // namespace cts
