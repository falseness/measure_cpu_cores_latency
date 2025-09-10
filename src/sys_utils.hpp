#pragma once


#include <pthread.h>


namespace icore {


bool PinCurrentThreadToCpu(int cpu);
void TryHardRealtime();
inline void CpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
__builtin_ia32_pause();
#else
asm volatile("");
#endif
}


} // namespace icore