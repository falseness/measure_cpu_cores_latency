#include "tsc.hpp"

#if !defined(__x86_64__) && !defined(__i386__)
#error "x86 required"
#endif

#include <cpuid.h>
#include <fstream>
#include <string>

namespace icore {

std::optional<uint64_t> GetTscHzFromCpuid() {
  unsigned int a, b, c, d;
  if (__get_cpuid(0x15, &a, &b, &c, &d)) {
    if (a && b && c) {
      double hz = static_cast<double>(c) * (static_cast<double>(b) / a);
      return static_cast<uint64_t>(hz);
    }
  }
  if (__get_cpuid(0x16, &a, &b, &c, &d)) {
    if (a) return static_cast<uint64_t>(a) * 1000000ull;
  }
  return std::nullopt;
}

std::optional<uint64_t> GetTscHzFromSysfs() {
  // Linux exposes kernel-calibrated TSC freq here on many platforms.
  // Value is in kHz.
  std::ifstream f("/sys/devices/system/cpu/cpu0/tsc_freq_khz");
  if (!f) return std::nullopt;
  uint64_t khz = 0;
  f >> khz;
  if (!f || khz == 0) return std::nullopt;
  return khz * 1000ull;
}

}  // namespace icore