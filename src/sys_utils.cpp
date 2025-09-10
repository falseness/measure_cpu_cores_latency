#include "sys_utils.hpp"


#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>


namespace icore {


bool PinCurrentThreadToCpu(int cpu) {
cpu_set_t set;
CPU_ZERO(&set);
CPU_SET(cpu, &set);
return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}


void TryHardRealtime() {
mlockall(MCL_CURRENT | MCL_FUTURE);
sched_param sp{.sched_priority = 80};
sched_setscheduler(0, SCHED_FIFO, &sp);
}


} // namespace icore