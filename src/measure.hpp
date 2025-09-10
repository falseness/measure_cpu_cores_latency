#pragma once

namespace icore {

struct PairResult {
  double median_ns;
  double p90_ns;
  double p95_ns;
};

struct MeasureConfig {
  int iters;
  int warmup;
  bool two_lines;
  double cycles_per_ns;
};

PairResult MeasurePair(int cpu_tx, int cpu_rx, const MeasureConfig& cfg);

}  // namespace icore
