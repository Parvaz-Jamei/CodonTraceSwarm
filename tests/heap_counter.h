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
inline void hit() { if (counting) ++allocs; }
inline void release() { if (counting) ++frees; }
struct Window {
  Window() { allocs = 0; frees = 0; counting = true; }
  ~Window() { counting = false; }
};
// Bump arena: free() only counts; memory is not reused. Tests are short.
alignas(4096) static unsigned char arena[64u << 20];
static size_t used = 0;
struct Header { size_t size; size_t pad[1]; };
static void *take(size_t n, size_t align) {
  if (align < alignof(std::max_align_t)) align = alignof(std::max_align_t);
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
void *realloc(void *p, size_t n) noexcept {
  heap::hit();
  void *q = heap::take(n ? n : 1, 0);
  if (p && q) {
    bool mine = static_cast<unsigned char *>(p) >= heap::arena && static_cast<unsigned char *>(p) < heap::arena + sizeof heap::arena;
    size_t o = mine ? heap::sizeOf(p) : n; // foreign block (pre-relocation loader): copy n
    std::memcpy(q, p, o < n ? o : n);
  }
  return q;
}
void *reallocarray(void *p, size_t a, size_t b) noexcept {
  if (b && a > SIZE_MAX / b) { errno = ENOMEM; return nullptr; }
  return realloc(p, a * b);
}
void *aligned_alloc(size_t a, size_t n) noexcept { heap::hit(); return heap::take(n ? n : 1, a); }
void *memalign(size_t a, size_t n) noexcept { heap::hit(); return heap::take(n ? n : 1, a); }
int posix_memalign(void **out, size_t a, size_t n) noexcept {
  heap::hit();
  void *p = heap::take(n ? n : 1, a);
  if (!p) return ENOMEM;
  *out = p;
  return 0;
}
void *valloc(size_t n) noexcept { heap::hit(); return heap::take(n ? n : 1, 4096); }
void *pvalloc(size_t n) noexcept { heap::hit(); return heap::take(((n ? n : 1) + 4095) & ~size_t(4095), 4096); }
size_t malloc_usable_size(void *p) noexcept { return p ? heap::sizeOf(p) : 0; }
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
