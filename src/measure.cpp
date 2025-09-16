#include "measure.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "mailbox.hpp"
#include "sys_utils.hpp"
#include "tsc.hpp"

namespace icore {
namespace {

double PickQuantile(std::vector<double>* values, double quantile) {
  if (values->empty()) { return NAN; }
  const std::size_t n = values->size();
  const std::size_t idx =
      static_cast<std::size_t>(std::floor(quantile * static_cast<double>(n - 1)));
  std::nth_element(values->begin(),
                   values->begin() + static_cast<std::ptrdiff_t>(idx),
                   values->end());
  return (*values)[idx];
}

void PinOrTerminate(int cpu_id, const char* role) {
  if (!PinCurrentThreadToCpu(cpu_id)) {
    std::fprintf(stderr, "PinCurrentThreadToCpu(%d) failed on %s thread\n", cpu_id, role);
    std::terminate();
  }
}

inline void WaitForStart(const std::atomic<bool>& start_flag) {
  while (!start_flag.load(std::memory_order_acquire)) { CpuRelax(); }
}

template <bool kTwoLines>
void TimedReceive(size_t iters_count,
                  double cycles_per_ns, 
                  Mailbox* mailbox,
                  uint64_t* last_seq,
                  std::vector<double>* samples_ns) {
  for (int i = 0; i < iters_count; ++i) {
    while (mailbox->seq.load(std::memory_order_acquire) == *last_seq) { CpuRelax(); }
    *last_seq = mailbox->seq.load(std::memory_order_relaxed);

    const uint64_t ts_send = ReadTimestamp(mailbox);
    if constexpr (kTwoLines) { TouchSecondLine(mailbox); }
    const uint64_t ts_recv = Rdtc();

    samples_ns->push_back(static_cast<double>(ts_recv - ts_send) / cycles_per_ns);
    mailbox->ack.store(*last_seq, std::memory_order_release);
  }
}

template <bool kTwoLines>
void TimedSend(size_t iters_count,
               Mailbox* mailbox,
               uint64_t* seq) {
  for (int i = 0; i < iters_count; ++i) {
    const uint64_t t = Rdtc();
    WriteTimestamp(mailbox, t);
    if constexpr (kTwoLines) {
      MutateSecondLine(mailbox, i);
    }
    const uint64_t cur = ++(*seq);
    mailbox->seq.store(cur, std::memory_order_release);
    while (mailbox->ack.load(std::memory_order_acquire) != cur) { CpuRelax(); }
  }
}

template <bool kTwoLines>
PairResult MeasurePairImpl(int cpu_sender, int cpu_receiver, const MeasureConfig& config) {
  alignas(kMailboxAlignBytes) Mailbox mailbox{};
  std::atomic<bool> start_flag{false};
  std::vector<double> samples_ns;
  samples_ns.reserve(static_cast<std::size_t>(config.iters));

  static constexpr size_t kWarmupItersCount = 100;
  std::thread receiver_thread([&] {
    PinOrTerminate(cpu_receiver, "receiver");
    TryHardRealtime();
    WaitForStart(start_flag);

    uint64_t last_seq = 0;
    std::vector<double> warmup_samples_ns;
    TimedReceive<kTwoLines>(kWarmupItersCount, config.cycles_per_ns, &mailbox, &last_seq, &warmup_samples_ns);
    TimedReceive<kTwoLines>(config.iters, config.cycles_per_ns, &mailbox, &last_seq, &samples_ns);
  });

  PinOrTerminate(cpu_sender, "sender");
  TryHardRealtime();

  start_flag.store(true, std::memory_order_release);

  uint64_t seq = 0;
  TimedSend<kTwoLines>(kWarmupItersCount, &mailbox, &seq);
  TimedSend<kTwoLines>(config.iters, &mailbox, &seq);

  receiver_thread.join();

  std::sort(samples_ns.begin(), samples_ns.end());
  PairResult out;
  out.median_ns = PickQuantile(&samples_ns, 0.50);
  out.p90_ns = PickQuantile(&samples_ns, 0.90);
  out.p95_ns = PickQuantile(&samples_ns, 0.95);
  return out;
}

}  // namespace

PairResult MeasurePair(int cpu_tx, int cpu_rx, const MeasureConfig& cfg) {
  if (cfg.two_lines) {
    return MeasurePairImpl<true>(cpu_tx, cpu_rx, cfg);
  } else {
    return MeasurePairImpl<false>(cpu_tx, cpu_rx, cfg);
  }
}

}  // namespace icore
