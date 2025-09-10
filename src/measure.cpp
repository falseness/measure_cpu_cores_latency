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
  if (values->empty()) {
    return NAN;
  }
  const std::size_t count = values->size();
  const std::size_t index =
      static_cast<std::size_t>(std::floor(quantile * static_cast<double>(count - 1)));
  std::nth_element(values->begin(),
                   values->begin() + static_cast<std::ptrdiff_t>(index),
                   values->end());
  return (*values)[index];
}

void PinOrTerminate(int cpu_id, const char* role) {
  if (!PinCurrentThreadToCpu(cpu_id)) {
    std::fprintf(stderr, "PinCurrentThreadToCpu(%d) failed on %s thread\n", cpu_id, role);
    std::terminate();
  }
}

template <bool kTwoLines>
inline void MaybeTouchSecondLine(const Mailbox* mailbox) {
  if constexpr (kTwoLines) {
    TouchSecondLine(mailbox);
  }
}

template <bool kTwoLines>
inline void MaybeMutateSecondLine(Mailbox* mailbox, uint64_t seed) {
  if constexpr (kTwoLines) {
    MutateSecondLine(mailbox, seed);
  }
}

void WaitForStart(const std::atomic<bool>& start_flag) {
  while (!start_flag.load(std::memory_order_acquire)) {
    CpuRelax();
  }
}

template <bool kTwoLines>
void WarmupReceiveLoop(const MeasureConfig& config,
                       Mailbox* mailbox,
                       uint64_t* last_sequence_observed) {
  for (int warmup_iteration = 0; warmup_iteration < config.warmup; ++warmup_iteration) {
    while (mailbox->seq.load(std::memory_order_acquire) == *last_sequence_observed) {
      CpuRelax();
    }
    *last_sequence_observed = mailbox->seq.load(std::memory_order_relaxed);
    MaybeTouchSecondLine<kTwoLines>(mailbox);
    mailbox->ack.store(*last_sequence_observed, std::memory_order_release);
  }
}

template <bool kTwoLines>
void TimedReceiveLoop(const MeasureConfig& config,
                      Mailbox* mailbox,
                      uint64_t* last_sequence_observed,
                      std::vector<double>* samples_nanoseconds) {
  for (int iteration = 0; iteration < config.iters; ++iteration) {
    while (mailbox->seq.load(std::memory_order_acquire) == *last_sequence_observed) {
      CpuRelax();
    }
    *last_sequence_observed = mailbox->seq.load(std::memory_order_relaxed);

    const uint64_t timestamp_receive = Rdtc();
    const uint64_t timestamp_send = ReadTimestamp(mailbox);
    MaybeTouchSecondLine<kTwoLines>(mailbox);

    const double delta_nanoseconds =
        static_cast<double>(timestamp_receive - timestamp_send) / config.cycles_per_ns;
    samples_nanoseconds->push_back(delta_nanoseconds);

    mailbox->ack.store(*last_sequence_observed, std::memory_order_release);
  }
}

template <bool kTwoLines>
void WarmupSendLoop(const MeasureConfig& config, Mailbox* mailbox, uint64_t* sequence) {
  for (int warmup_iteration = 0; warmup_iteration < config.warmup; ++warmup_iteration) {
    const uint64_t timestamp = Rdtc();
    WriteTimestamp(mailbox, timestamp);
    MaybeMutateSecondLine<kTwoLines>(mailbox, timestamp);

    const uint64_t current_sequence = ++(*sequence);
    mailbox->seq.store(current_sequence, std::memory_order_release);

    while (mailbox->ack.load(std::memory_order_acquire) != current_sequence) {
      CpuRelax();
    }
  }
}

template <bool kTwoLines>
void TimedSendLoop(const MeasureConfig& config, Mailbox* mailbox, uint64_t* sequence) {
  for (int iteration = 0; iteration < config.iters; ++iteration) {
    const uint64_t timestamp = Rdtc();
    WriteTimestamp(mailbox, timestamp);
    MaybeMutateSecondLine<kTwoLines>(mailbox, timestamp);

    const uint64_t current_sequence = ++(*sequence);
    mailbox->seq.store(current_sequence, std::memory_order_release);

    while (mailbox->ack.load(std::memory_order_acquire) != current_sequence) {
      CpuRelax();
    }
  }
}

template <bool kTwoLines>
PairResult MeasurePairImpl(int cpu_sender, int cpu_receiver, const MeasureConfig& config) {
  alignas(2 * kCacheLineBytes) Mailbox mailbox{};

  std::atomic<bool> start_flag{false};
  std::vector<double> samples_nanoseconds;
  samples_nanoseconds.reserve(static_cast<std::size_t>(config.iters));

  std::thread receiver_thread([&] {
    PinOrTerminate(cpu_receiver, "receiver");
    TryHardRealtime();
    WaitForStart(start_flag);

    uint64_t last_sequence_observed = 0;
    WarmupReceiveLoop<kTwoLines>(config, &mailbox, &last_sequence_observed);
    TimedReceiveLoop<kTwoLines>(config, &mailbox, &last_sequence_observed, &samples_nanoseconds);
  });

  PinOrTerminate(cpu_sender, "sender");
  TryHardRealtime();

  start_flag.store(true, std::memory_order_release);

  uint64_t sequence = 0;
  WarmupSendLoop<kTwoLines>(config, &mailbox, &sequence);
  TimedSendLoop<kTwoLines>(config, &mailbox, &sequence);

  receiver_thread.join();

  std::sort(samples_nanoseconds.begin(), samples_nanoseconds.end());

  PairResult result;
  result.median_ns = PickQuantile(&samples_nanoseconds, 0.50);
  result.p90_ns = PickQuantile(&samples_nanoseconds, 0.90);
  result.p95_ns = PickQuantile(&samples_nanoseconds, 0.95);
  return result;
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
