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
  double tsc_ghz = 0.0;  // manual override
};

static Args ParseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const std::string s = argv[i];
    if (s == "--csv") {
      a.csv = true;
    } else if (s == "--iters" && i + 1 < argc) {
      a.iters = std::stoi(argv[++i]);
    } else if (s == "--warmup" && i + 1 < argc) {
      a.warmup = std::stoi(argv[++i]);
    } else if (s == "--socket" && i + 1 < argc) {
      a.socket = std::stoi(argv[++i]);
    } else if (s == "--tsc-ghz" && i + 1 < argc) {
      a.tsc_ghz = std::stod(argv[++i]);
    } else {
      std::cerr << "Unknown arg: " << s << "\n";
      std::exit(1);
    }
  }
  return a;
}

static std::string FmtNum(double v, int precision = 1) {
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss << std::setprecision(precision) << v;
  return oss.str();
}

static void PrintMatrix(const std::string& title,
                        const std::vector<int>& cpus,
                        const std::vector<std::vector<double>>& m,
                        bool csv) {
  const std::size_t n = cpus.size();
  std::cout << "==== " << title << " ====\n";

  if (csv) {
    std::cout << "cpu/cpu";
    for (std::size_t j = 0; j < n; ++j) {
      std::cout << "," << cpus[j];
    }
    std::cout << "\n";
    for (std::size_t i = 0; i < n; ++i) {
      std::cout << cpus[i];
      for (std::size_t j = 0; j < n; ++j) {
        if (i == j) {
          std::cout << ",";
        } else {
          std::cout << "," << FmtNum(m[i][j]);
        }
      }
      std::cout << "\n";
    }
    return;
  }

  const int precision = 1;
  const int pad = 1;

  std::size_t first_col_w = std::string("cpu").size();
  for (int id : cpus) {
    const auto w = std::to_string(id).size();
    if (w > first_col_w) {
      first_col_w = w;
    }
  }

  std::vector<std::size_t> col_w(n, 0);
  for (std::size_t j = 0; j < n; ++j) {
    col_w[j] = std::to_string(cpus[j]).size();
    for (std::size_t i = 0; i < n; ++i) {
      std::size_t cell_w = 0;
      if (i == j) {
        cell_w = 1;  // "-"
      } else {
        cell_w = FmtNum(m[i][j], precision).size();
      }
      if (cell_w > col_w[j]) {
        col_w[j] = cell_w;
      }
    }
  }

  std::cout << std::setw(static_cast<int>(first_col_w)) << "cpu" << std::string(pad, ' ');
  for (std::size_t j = 0; j < n; ++j) {
    std::cout << std::setw(static_cast<int>(col_w[j])) << cpus[j];
    if (j + 1 != n) {
      std::cout << std::string(pad, ' ');
    }
  }
  std::cout << "\n";

  std::cout << std::string(first_col_w, '-') << std::string(pad, ' ');
  for (std::size_t j = 0; j < n; ++j) {
    std::cout << std::string(col_w[j], '-');
    if (j + 1 != n) {
      std::cout << std::string(pad, ' ');
    }
  }
  std::cout << "\n";

  for (std::size_t i = 0; i < n; ++i) {
    std::cout << std::setw(static_cast<int>(first_col_w)) << cpus[i] << std::string(pad, ' ');
    for (std::size_t j = 0; j < n; ++j) {
      if (i == j) {
        std::cout << std::setw(static_cast<int>(col_w[j])) << "-";
      } else {
        std::cout << std::setw(static_cast<int>(col_w[j])) << FmtNum(m[i][j], precision);
      }
      if (j + 1 != n) {
        std::cout << std::string(pad, ' ');
      }
    }
    std::cout << "\n";
  }
}

static void RunMode(const std::string& title,
                    bool two_lines,
                    const Args& args,
                    const std::vector<int>& used,
                    double cycles_per_ns) {
  const std::size_t n = used.size();
  std::vector<std::vector<double>> med(n, std::vector<double>(n, NAN));
  std::vector<std::vector<double>> p90(n, std::vector<double>(n, NAN));
  std::vector<std::vector<double>> p95(n, std::vector<double>(n, NAN));

  MeasureConfig mc;
  mc.iters = args.iters;
  mc.warmup = args.warmup;
  mc.two_lines = two_lines;
  mc.cycles_per_ns = cycles_per_ns;

  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      if (i == j) {
        continue;
      }
      const PairResult r = MeasurePair(used[i], used[j], mc);
      med[i][j] = r.median_ns;
      p90[i][j] = r.p90_ns;
      p95[i][j] = r.p95_ns;
      std::cerr << (two_lines ? "[128B] " : "[64B] ")
                << used[i] << "->" << used[j]
                << " med=" << r.median_ns
                << " p90=" << r.p90_ns
                << " p95=" << r.p95_ns << " ns\n";
    }
  }

  PrintMatrix(title + " (median)", used, med, args.csv);
  PrintMatrix(title + " (p90)", used, p90, args.csv);
  PrintMatrix(title + " (p95)", used, p95, args.csv);
}

}  // namespace icore

int main(int argc, char** argv) {
  using namespace icore;

  const Args args = ParseArgs(argc, argv);

  double cycles_per_ns = 0.0;
  if (args.tsc_ghz > 0.0) {
    cycles_per_ns = args.tsc_ghz;
    std::cerr << "TSC ~ " << (cycles_per_ns * 1e3) << " MHz (manual)\n";
  } else {
    auto hz = GetTscHzFromCpuid();
    if (!hz || *hz == 0) {
      hz = GetTscHzFromSysfs();
    }
    if (!hz || *hz == 0) {
      std::cerr << "Failed to obtain TSC frequency. Provide --tsc-ghz <GHz>.\n";
      return 1;
    }
    cycles_per_ns = static_cast<double>(*hz) / 1e9;
    std::cerr << "TSC ~ " << (cycles_per_ns * 1e3) << " MHz"
              << (GetTscHzFromCpuid() ? " (CPUID)" : " (sysfs)") << "\n";
  }

  const auto infos = GetOneThreadPerCoreSameSocket(args.socket);
  if (infos.size() < 2) {
    std::cerr << "Need >=2 cores on socket " << args.socket << "\n";
    return 1;
  }

  std::vector<int> used;
  used.reserve(infos.size());
  for (const auto& c : infos) {
    used.push_back(c.cpu);
  }

  RunMode("1 cache line", /*two_lines=*/false, args, used, cycles_per_ns);
  RunMode("2 cache lines", /*two_lines=*/true, args, used, cycles_per_ns);
  return 0;
}
