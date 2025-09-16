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
struct BalancedLine {
  alignas(kCacheLineBytes) uint8_t local_line[kCacheLineBytes]{};

  inline void Mutate(Mailbox* mailbox, uint64_t seed) {
    if constexpr (kTwoLines) {
      MutateSecondLine(mailbox, seed);
    } else {
      auto* q = reinterpret_cast<uint64_t*>(local_line);
      for (std::size_t i = 0; i < kQwordsPerLine; ++i) { q[i] = seed + static_cast<uint64_t>(i); }
    }
  }

  inline void Touch(const Mailbox* mailbox) {
    if constexpr (kTwoLines) {
      TouchSecondLine(mailbox);
    } else {
      volatile const uint64_t* q = reinterpret_cast<const uint64_t*>(local_line);
      uint64_t sink = 0;
      for (std::size_t i = 0; i < kQwordsPerLine; ++i) { sink ^= q[i]; }
      asm volatile("" :: "r"(sink));
    }
  }
};

template <bool kTwoLines>
void WarmupReceive(const MeasureConfig& config,
                   Mailbox* mailbox,
                   uint64_t* last_seq,
                   BalancedLine<kTwoLines>* bal) {
  for (int i = 0; i < config.warmup; ++i) {
    while (mailbox->seq.load(std::memory_order_acquire) == *last_seq) { CpuRelax(); }
    *last_seq = mailbox->seq.load(std::memory_order_relaxed);
    (void)ReadTimestamp(mailbox);
    if constexpr (kTwoLines) { bal->Touch(mailbox); }
    mailbox->ack.store(*last_seq, std::memory_order_release);
  }
}

template <bool kTwoLines>
void TimedReceive(const MeasureConfig& config,
                  Mailbox* mailbox,
                  uint64_t* last_seq,
                  BalancedLine<kTwoLines>* bal,
                  std::vector<double>* samples_ns) {
  for (int i = 0; i < config.iters; ++i) {
    while (mailbox->seq.load(std::memory_order_acquire) == *last_seq) { CpuRelax(); }
    *last_seq = mailbox->seq.load(std::memory_order_relaxed);

    const uint64_t ts_send = ReadTimestamp(mailbox);
    if constexpr (kTwoLines) { TouchSecondLine(mailbox); }
    const uint64_t ts_recv = Rdtc();

    samples_ns->push_back(static_cast<double>(ts_recv - ts_send) / config.cycles_per_ns);
    mailbox->ack.store(*last_seq, std::memory_order_release);
  }
}

template <bool kTwoLines>
void WarmupSend(const MeasureConfig& config,
                Mailbox* mailbox,
                uint64_t* seq,
                BalancedLine<kTwoLines>* bal) {
  for (int i = 0; i < config.warmup; ++i) {
    const uint64_t t = Rdtc();
    WriteTimestamp(mailbox, t);
    bal->Mutate(mailbox, t);
    const uint64_t cur = ++(*seq);
    mailbox->seq.store(cur, std::memory_order_release);
    while (mailbox->ack.load(std::memory_order_acquire) != cur) { CpuRelax(); }
  }
}

template <bool kTwoLines>
void TimedSend(const MeasureConfig& config,
               Mailbox* mailbox,
               uint64_t* seq,
               BalancedLine<kTwoLines>* bal) {
  for (int i = 0; i < config.iters; ++i) {
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

  BalancedLine<kTwoLines> sender_line;
  BalancedLine<kTwoLines> receiver_line;

  std::thread receiver_thread([&] {
    PinOrTerminate(cpu_receiver, "receiver");
    TryHardRealtime();
    WaitForStart(start_flag);

    uint64_t last_seq = 0;
    WarmupReceive<kTwoLines>(config, &mailbox, &last_seq, &receiver_line);
    TimedReceive<kTwoLines>(config, &mailbox, &last_seq, &receiver_line, &samples_ns);
  });

  PinOrTerminate(cpu_sender, "sender");
  TryHardRealtime();

  start_flag.store(true, std::memory_order_release);

  uint64_t seq = 0;
  WarmupSend<kTwoLines>(config, &mailbox, &seq, &sender_line);
  TimedSend<kTwoLines>(config, &mailbox, &seq, &sender_line);

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
