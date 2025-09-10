#pragma once

#include <cstdint>
#include <optional>

namespace icore {

inline uint64_t Rdtc() {
#if defined(__x86_64__) || defined(__i386__)
  unsigned int lo = 0, hi = 0;
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx", "memory");
  return (static_cast<uint64_t>(hi) << 32) | lo;
#else
# error "x86 required"
#endif
}

std::optional<uint64_t> GetTscHzFromCpuid();
std::optional<uint64_t> GetTscHzFromSysfs();

}  // namespace icore
