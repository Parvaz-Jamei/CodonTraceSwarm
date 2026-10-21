// SPDX-License-Identifier: MIT
// TEST ONLY: heap-call counters for the zero-allocation suites.
// Include in exactly one TU per test binary built by [env:native_noheap].
// - malloc/calloc/realloc/free (and aligned_alloc, posix_memalign) are
//   intercepted at link time with -Wl,--wrap=..., so every call made from
//   CodonTraceSwarm objects and the test TU is counted.
// - operator new/new[]/delete/delete[] are replaced (plain, nothrow, sized,
//   std::align_val_t aligned, aligned nothrow), which also catches allocations
//   made through libstdc++ (std::string, std::vector, std::function).
#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>
extern "C" {
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
void *__real_aligned_alloc(size_t, size_t);
int __real_posix_memalign(void **, size_t, size_t);
}
namespace heap {
static volatile bool counting = false;
static volatile unsigned long allocs = 0, frees = 0;
inline void hit() { if (counting) ++allocs; }
inline void release() { if (counting) ++frees; }
struct Window {
  Window() { allocs = 0; frees = 0; counting = true; }
  ~Window() { counting = false; }
};
} // namespace heap
extern "C" {
void *__wrap_malloc(size_t n) { heap::hit(); return __real_malloc(n); }
void *__wrap_calloc(size_t a, size_t b) { heap::hit(); return __real_calloc(a, b); }
void *__wrap_realloc(void *p, size_t n) { heap::hit(); return __real_realloc(p, n); }
void __wrap_free(void *p) { if (p) heap::release(); __real_free(p); }
void *__wrap_aligned_alloc(size_t a, size_t n) { heap::hit(); return __real_aligned_alloc(a, n); }
int __wrap_posix_memalign(void **p, size_t a, size_t n) { heap::hit(); return __real_posix_memalign(p, a, n); }
}
static void *heapNew(std::size_t n) {
  heap::hit();
  if (void *p = __real_malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
static void *heapNewAligned(std::size_t n, std::align_val_t a) {
  heap::hit();
  void *p = nullptr;
  if (__real_posix_memalign(&p, static_cast<std::size_t>(a) < sizeof(void *) ? sizeof(void *) : static_cast<std::size_t>(a), n ? n : 1) == 0) return p;
  throw std::bad_alloc();
}
static void heapDelete(void *p) noexcept { if (p) heap::release(); __real_free(p); }
void *operator new(std::size_t n) { return heapNew(n); }
void *operator new[](std::size_t n) { return heapNew(n); }
void *operator new(std::size_t n, const std::nothrow_t &) noexcept { try { return heapNew(n); } catch (...) { return nullptr; } }
void *operator new[](std::size_t n, const std::nothrow_t &) noexcept { try { return heapNew(n); } catch (...) { return nullptr; } }
void *operator new(std::size_t n, std::align_val_t a) { return heapNewAligned(n, a); }
void *operator new[](std::size_t n, std::align_val_t a) { return heapNewAligned(n, a); }
void *operator new(std::size_t n, std::align_val_t a, const std::nothrow_t &) noexcept { try { return heapNewAligned(n, a); } catch (...) { return nullptr; } }
void *operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t &) noexcept { try { return heapNewAligned(n, a); } catch (...) { return nullptr; } }
void operator delete(void *p) noexcept { heapDelete(p); }
void operator delete[](void *p) noexcept { heapDelete(p); }
void operator delete(void *p, std::size_t) noexcept { heapDelete(p); }
void operator delete[](void *p, std::size_t) noexcept { heapDelete(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { heapDelete(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { heapDelete(p); }
void operator delete(void *p, std::align_val_t) noexcept { heapDelete(p); }
void operator delete[](void *p, std::align_val_t) noexcept { heapDelete(p); }
void operator delete(void *p, std::size_t, std::align_val_t) noexcept { heapDelete(p); }
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept { heapDelete(p); }
void operator delete(void *p, std::align_val_t, const std::nothrow_t &) noexcept { heapDelete(p); }
void operator delete[](void *p, std::align_val_t, const std::nothrow_t &) noexcept { heapDelete(p); }
