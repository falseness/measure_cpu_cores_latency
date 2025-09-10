#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cpu_topology.hpp"
#include "measure.hpp"
#include "tsc.hpp"

namespace icore {

struct Args {
  int iters = 30000;
  int warmup = 5000;
  int socket = 0;
  bool csv = false;
  double tsc_ghz = 0.0;
};

static Args ParseArgs(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string token = argv[i];
    if (token == "--csv") {
      args.csv = true;
    } else if (token == "--iters" && i + 1 < argc) {
      args.iters = std::stoi(argv[++i]);
    } else if (token == "--warmup" && i + 1 < argc) {
      args.warmup = std::stoi(argv[++i]);
    } else if (token == "--socket" && i + 1 < argc) {
      args.socket = std::stoi(argv[++i]);
    } else if (token == "--tsc-ghz" && i + 1 < argc) {
      args.tsc_ghz = std::stod(argv[++i]);
    } else {
      std::cerr << "Unknown arg: " << token << "\n";
      std::exit(1);
    }
  }
  return args;
}

static std::string FormatNumber(double value, int precision = 1) {
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss << std::setprecision(precision) << value;
  return oss.str();
}

static void PrintMatrix(const std::string& title,
                        const std::vector<int>& cpu_ids,
                        const std::vector<std::vector<double>>& matrix,
                        bool csv) {
  const std::size_t n = cpu_ids.size();
  std::cout << "==== " << title << " ====\n";

  if (csv) {
    std::cout << "cpu/cpu";
    for (std::size_t j = 0; j < n; ++j) {
      std::cout << "," << cpu_ids[j];
    }
    std::cout << "\n";
    for (std::size_t i = 0; i < n; ++i) {
      std::cout << cpu_ids[i];
      for (std::size_t j = 0; j < n; ++j) {
        if (i == j) {
          std::cout << ",";
        } else {
          std::cout << "," << FormatNumber(matrix[i][j]);
        }
      }
      std::cout << "\n";
    }
    return;
  }

  const int precision = 1;
  const int padding = 1;

  std::size_t first_col_width = std::string("cpu").size();
  for (int id : cpu_ids) {
    const std::size_t w = std::to_string(id).size();
    if (w > first_col_width) {
      first_col_width = w;
    }
  }

  std::vector<std::size_t> col_width(n, 0);
  for (std::size_t j = 0; j < n; ++j) {
    col_width[j] = std::to_string(cpu_ids[j]).size();
    for (std::size_t i = 0; i < n; ++i) {
      std::size_t cell_w = 0;
      if (i == j) {
        cell_w = 1;
      } else {
        cell_w = FormatNumber(matrix[i][j], precision).size();
      }
      if (cell_w > col_width[j]) {
        col_width[j] = cell_w;
      }
    }
  }

  // Header
  std::cout << std::setw(static_cast<int>(first_col_width)) << "cpu" << std::string(padding, ' ');
  for (std::size_t j = 0; j < n; ++j) {
    std::cout << std::setw(static_cast<int>(col_width[j])) << cpu_ids[j];
    if (j + 1 != n) {
      std::cout << std::string(padding, ' ');
    }
  }
  std::cout << "\n";

  // Separator
  std::cout << std::string(first_col_width, '-') << std::string(padding, ' ');
  for (std::size_t j = 0; j < n; ++j) {
    std::cout << std::string(col_width[j], '-');
    if (j + 1 != n) {
      std::cout << std::string(padding, ' ');
    }
  }
  std::cout << "\n";

  // Rows
  for (std::size_t i = 0; i < n; ++i) {
    std::cout << std::setw(static_cast<int>(first_col_width)) << cpu_ids[i]
              << std::string(padding, ' ');
    for (std::size_t j = 0; j < n; ++j) {
      if (i == j) {
        std::cout << std::setw(static_cast<int>(col_width[j])) << "-";
      } else {
        std::cout << std::setw(static_cast<int>(col_width[j]))
                  << FormatNumber(matrix[i][j], precision);
      }
      if (j + 1 != n) {
        std::cout << std::string(padding, ' ');
      }
    }
    std::cout << "\n";
  }
  std::cout << "\n";
}

static void RunMode(const std::string& title,
                    bool two_lines,
                    const Args& args,
                    const std::vector<int>& used,
                    double cycles_per_ns) {
  const std::size_t n = used.size();
  std::vector<std::vector<double>> median(n, std::vector<double>(n, NAN));
  std::vector<std::vector<double>> p90(n, std::vector<double>(n, NAN));
  std::vector<std::vector<double>> p95(n, std::vector<double>(n, NAN));

  MeasureConfig config;
  config.iters = args.iters;
  config.warmup = args.warmup;
  config.two_lines = two_lines;
  config.cycles_per_ns = cycles_per_ns;

  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      if (i == j) {
        continue;
      }
      const PairResult result = MeasurePair(used[i], used[j], config);
      median[i][j] = result.median_ns;
      p90[i][j] = result.p90_ns;
      p95[i][j] = result.p95_ns;

      // std::cerr << (two_lines ? "[128B] " : "[64B] ")
      //           << used[i] << "->" << used[j]
      //           << " med=" << FormatNumber(result.median_ns)
      //           << " p90=" << FormatNumber(result.p90_ns)
      //           << " p95=" << FormatNumber(result.p95_ns) << " ns\n";
    }
  }

  PrintMatrix(title + " (median)", used, median, args.csv);
  PrintMatrix(title + " (p90)", used, p90, args.csv);
  PrintMatrix(title + " (p95)", used, p95, args.csv);
}

static double DetermineCyclesPerNs(const Args& args) {
  if (args.tsc_ghz > 0.0) {
    const double cycles_per_ns = args.tsc_ghz;
    std::cout << "TSC ~ " << (cycles_per_ns * 1e3) << " MHz (manual)\n";
    return cycles_per_ns;
  }

  auto tsc_hz = GetTscHzFromCpuid();
  if (!tsc_hz || *tsc_hz == 0) {
    tsc_hz = GetTscHzFromSysfs();
  }
  if (!tsc_hz || *tsc_hz == 0) {
    std::cerr << "Failed to obtain TSC frequency. Provide --tsc-ghz <GHz>.\n";
    std::exit(1);
  }

  const double cycles_per_ns = static_cast<double>(*tsc_hz) / 1e9;
  std::cout << "TSC ~ " << (cycles_per_ns * 1e3)
            << " MHz" << (GetTscHzFromCpuid() ? " (CPUID)" : " (sysfs)") << "\n";
  return cycles_per_ns;
}

static void RunBenchmark(const Args& args) {
  const double cycles_per_ns = DetermineCyclesPerNs(args);

  const auto infos = GetOneThreadPerCoreSameSocket(args.socket);
  if (infos.size() < 2) {
    std::cerr << "Need >=2 cores on socket " << args.socket << "\n";
    std::exit(1);
  }

  std::vector<int> used;
  used.reserve(infos.size());
  for (const auto& info : infos) {
    used.push_back(info.cpu);
  }

  RunMode("1 cache line", /*two_lines=*/false, args, used, cycles_per_ns);
  RunMode("2 cache lines", /*two_lines=*/true, args, used, cycles_per_ns);
}

}  // namespace icore

int main(int argc, char** argv) {
  const icore::Args args = icore::ParseArgs(argc, argv);
  icore::RunBenchmark(args);
  return 0;
}
