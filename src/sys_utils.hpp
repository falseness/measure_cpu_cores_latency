#pragma once

#include <pthread.h>

namespace icore {

// Pin the calling thread to a specific CPU.
bool PinCurrentThreadToCpu(int cpu_id);

// Try to lock memory and switch to a high-priority realtime scheduler.
void TryHardRealtime();

// Architecture-friendly spin-wait hint.
inline void CpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#else
  asm volatile("");
#endif
}

}  // namespace icore
