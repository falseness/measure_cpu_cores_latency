#include "tsc.hpp"

#if !defined(__x86_64__) && !defined(__i386__)
#error "x86 required"
#endif

#include <cpuid.h>
#include <fstream>
#include <string>

namespace icore {
namespace {

constexpr unsigned int kLeafTscRatio = 0x15;
constexpr unsigned int kLeafProcFreq = 0x16;

constexpr uint64_t kHzToHz = 1000ULL;
constexpr uint64_t MHzToHz = 1000000ULL;

constexpr const char* kSysfsTscKHzPath = "/sys/devices/system/cpu/cpu0/tsc_freq_khz";

}  // namespace

std::optional<uint64_t> GetTscHzFromCpuid() {
  unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

  if (__get_cpuid(kLeafTscRatio, &eax, &ebx, &ecx, &edx)) {
    const unsigned int denominator = eax;
    const unsigned int numerator = ebx;
    const unsigned int crystal_hz = ecx;
    if (denominator != 0U && numerator != 0U && crystal_hz != 0U) {
      const double tsc_hz =
          static_cast<double>(crystal_hz) *
          (static_cast<double>(numerator) / static_cast<double>(denominator));
      return static_cast<uint64_t>(tsc_hz);
    }
  }

  if (__get_cpuid(kLeafProcFreq, &eax, &ebx, &ecx, &edx)) {
    const unsigned int base_freq_mhz = eax;
    if (base_freq_mhz != 0U) {
      return static_cast<uint64_t>(base_freq_mhz) * MHzToHz;
    }
  }

  return std::nullopt;
}

std::optional<uint64_t> GetTscHzFromSysfs() {
  std::ifstream file(kSysfsTscKHzPath);
  if (!file) {
    return std::nullopt;
  }
  uint64_t tsc_khz = 0;
  file >> tsc_khz;
  if (!file || tsc_khz == 0ULL) {
    return std::nullopt;
  }
  return tsc_khz * kHzToHz;
}

}  // namespace icore
