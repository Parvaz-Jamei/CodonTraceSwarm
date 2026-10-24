// SPDX-License-Identifier: MIT
// TEST ONLY: heap-call counters for the zero-allocation suites.
// Include in exactly one TU per test binary built by [env:native_noheap].
//
// The binary defines the whole C allocation family itself (malloc, free,
// calloc, realloc, reallocarray, aligned_alloc, memalign, posix_memalign,
// valloc, pvalloc, malloc_usable_size), backed by a private static arena.
// glibc documents this as supported malloc replacement: the dynamic linker
// binds every caller to these definitions, including calls made inside libc
// (strdup, strndup, asprintf, fopen, ...) and libstdc++ (std::string storage).
// It does not use __malloc_hook (removed in glibc 2.34) or __libc_* entry points.
// Every operator new/delete form is replaced as well, so C++ allocation is
// counted even if a toolchain's operator new did not go through malloc.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <new>
namespace heap {
static volatile bool counting = false;
static volatile unsigned long allocs = 0, frees = 0;
// realloc() calls whose pointer did not come from this pool (rejected, never read).
static volatile unsigned long foreign = 0;
inline void hit() { if (counting) ++allocs; }
inline void release() { if (counting) ++frees; }
struct Window {
  Window() { allocs = 0; frees = 0; foreign = 0; counting = true; }
  ~Window() { counting = false; }
};
// Bump arena: free() only counts; memory is not reused. Tests are short.
alignas(4096) static unsigned char arena[64u << 20];
static size_t used = 0;
struct Header { size_t size; size_t pad[1]; };
inline bool pow2(size_t a) { return a && !(a & (a - 1)); }
inline bool mine(const void *p) {
  return static_cast<const unsigned char *>(p) >= arena && static_cast<const unsigned char *>(p) < arena + sizeof arena;
}
static void *take(size_t n, size_t align) {
  if (align > sizeof arena) { errno = ENOMEM; return nullptr; }
  if (align < alignof(std::max_align_t)) align = alignof(std::max_align_t);
  while (!pow2(align)) align += align & -align; // round up to a power of two (glibc memalign)
  size_t start = (used + sizeof(Header) + align - 1) & ~(align - 1);
  if (n > sizeof arena || start + n > sizeof arena) { errno = ENOMEM; return nullptr; }
  reinterpret_cast<Header *>(arena + start - sizeof(Header))->size = n;
  used = start + n;
  return arena + start;
}
static size_t sizeOf(void *p) { return reinterpret_cast<Header *>(static_cast<unsigned char *>(p) - sizeof(Header))->size; }
} // namespace heap
extern "C" {
void *malloc(size_t n) noexcept { heap::hit(); return heap::take(n ? n : 1, 0); }
void free(void *p) noexcept { if (p) heap::release(); }
void *calloc(size_t a, size_t b) noexcept {
  heap::hit();
  if (b && a > SIZE_MAX / b) { errno = ENOMEM; return nullptr; }
  size_t n = a * b;
  void *p = heap::take(n ? n : 1, 0);
  if (p) std::memset(p, 0, n);
  return p;
}
// Semantics follow glibc (malloc(3), posix_memalign(3), aligned_alloc in glibc >= 2.38):
// realloc(p, 0) with p != NULL frees p and returns NULL; realloc(NULL, n) is malloc(n).
void *realloc(void *p, size_t n) noexcept {
  heap::hit();
  // Not from this pool (glibc would abort on an invalid pointer): count it, return NULL,
  // and never read the block, so its size is irrelevant.
  if (p && !heap::mine(p)) { heap::foreign = heap::foreign + 1; errno = EINVAL; return nullptr; }
  if (p && n == 0) { heap::release(); return nullptr; }
  void *q = heap::take(n ? n : 1, 0);
  if (p && q) {
    size_t o = heap::sizeOf(p);
    std::memcpy(q, p, o < n ? o : n);
    heap::release();
  }
  return q;
}
void *reallocarray(void *p, size_t a, size_t b) noexcept {
  if (b && a > SIZE_MAX / b) { heap::hit(); errno = ENOMEM; return nullptr; } // counted attempt
  return realloc(p, a * b);
}
// glibc >= 2.38: alignment must be a power of two, else NULL/EINVAL. Size need not be a
// multiple of the alignment (C17 DR 460; glibc does not check it).
void *aligned_alloc(size_t a, size_t n) noexcept {
  heap::hit();
  if (!heap::pow2(a)) { errno = EINVAL; return nullptr; }
  return heap::take(n ? n : 1, a);
}
void *memalign(size_t a, size_t n) noexcept { heap::hit(); return heap::take(n ? n : 1, a); }
int posix_memalign(void **out, size_t a, size_t n) noexcept {
  heap::hit();
  if (!heap::pow2(a) || a % sizeof(void *)) return EINVAL; // *out untouched, as POSIX
  void *p = heap::take(n ? n : 1, a);
  if (!p) return ENOMEM;
  *out = p;
  return 0;
}
void *valloc(size_t n) noexcept { heap::hit(); return heap::take(n ? n : 1, 4096); }
void *pvalloc(size_t n) noexcept { heap::hit(); return heap::take(((n ? n : 1) + 4095) & ~size_t(4095), 4096); }
size_t malloc_usable_size(void *p) noexcept { return p && heap::mine(p) ? heap::sizeOf(p) : 0; }
}
static void *heapNew(std::size_t n, std::size_t a) {
  heap::hit();
  if (void *p = heap::take(n ? n : 1, a)) return p;
  throw std::bad_alloc();
}
void *operator new(std::size_t n) { return heapNew(n, 0); }
void *operator new[](std::size_t n) { return heapNew(n, 0); }
void *operator new(std::size_t n, const std::nothrow_t &) noexcept { heap::hit(); return heap::take(n ? n : 1, 0); }
void *operator new[](std::size_t n, const std::nothrow_t &) noexcept { heap::hit(); return heap::take(n ? n : 1, 0); }
void *operator new(std::size_t n, std::align_val_t a) { return heapNew(n, static_cast<std::size_t>(a)); }
void *operator new[](std::size_t n, std::align_val_t a) { return heapNew(n, static_cast<std::size_t>(a)); }
void *operator new(std::size_t n, std::align_val_t a, const std::nothrow_t &) noexcept { heap::hit(); return heap::take(n ? n : 1, static_cast<std::size_t>(a)); }
void *operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t &) noexcept { heap::hit(); return heap::take(n ? n : 1, static_cast<std::size_t>(a)); }
static void heapDelete(void *p) noexcept { if (p) heap::release(); }
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
