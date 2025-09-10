#include "sys_utils.hpp"

#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

namespace icore {
namespace {
constexpr int kRealtimePriority = 80;
}

bool PinCurrentThreadToCpu(int cpu_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu_id, &mask);
  const int rc = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
  return rc == 0;
}

void TryHardRealtime() {
  mlockall(MCL_CURRENT | MCL_FUTURE);
  sched_param param{};
  param.sched_priority = kRealtimePriority;
  sched_setscheduler(0, SCHED_FIFO, &param);
}

}  // namespace icore
