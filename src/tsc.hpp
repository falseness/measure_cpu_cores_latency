#pragma once

#include <cstdint>
#include <optional>

namespace icore {

inline uint64_t Rdtcs() {
#if defined(__x86_64__)
unsigned int lo, hi;
asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx", "memory");

return (static_cast<uint64_t>(hi) << 32) | lo;
#else
# error "x86_64 required"
#endif
}

std::optional<uint64_t> GetTscHzFromCpuid();
std::optional<uint64_t> GetTscHzFromSysfs();

}  // namespace icore