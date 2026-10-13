#include "cts/Millis32.h"
#include <cassert>
#include <iostream>
int main() {
  cts::Millis32 c;
  assert(!c.ready()); assert(c.update(0xfffffff0u));
  assert(c.elapsedMs()==0); assert(c.update(0x10));
  assert(c.elapsedMs()==32); assert(c.update(0x10));
  assert(c.elapsedMs()==32); assert(!c.update(0x0f));
  assert(c.faulted()); assert(!c.update(0x11));
  assert(c.elapsedMs()==32); assert(!c.ready());
  cts::Millis32 boundary;
  assert(boundary.update(0)); assert(boundary.update(0x7fffffff));
  assert(!boundary.update(0xffffffff));
  cts::Millis32 multiple;
  uint32_t raw=0; uint64_t expected=0;
  assert(multiple.update(raw));
  for(int i=0;i<10000;++i) {
    raw+=1000000u;expected+=1000000u;
    assert(multiple.update(raw)); assert(multiple.elapsedMs()==expected);
  }
  assert(multiple.ready());
  std::cout << "PASS: millis32 wrap, repeated tick, rollback latch, half-range boundary and 10000 increments across multiple wraps\n";
}
