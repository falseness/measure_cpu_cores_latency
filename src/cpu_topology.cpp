#include "cpu_topology.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace icore {
namespace {

constexpr const char* kSysCpuBase = "/sys/devices/system/cpu";
constexpr const char* kCpuPrefix = "cpu";
constexpr std::size_t kCpuPrefixLen = 3; 
constexpr const char* kTopologyDir = "topology";
constexpr const char* kOnlineFile = "online";
constexpr const char* kPackageFile = "physical_package_id";
constexpr const char* kCoreFile = "core_id";
constexpr int kOnlineYes = 1;

bool ReadIntFile(const std::filesystem::path& path, int* out_value) {
  std::ifstream file(path);
  if (!file) {
    return false;
  }
  file >> *out_value;
  return static_cast<bool>(file);
}

bool IsCpuDir(const std::filesystem::directory_entry& entry, int* out_cpu_id) {
  const std::string name = entry.path().filename().string();
  if (name.rfind(kCpuPrefix, 0) != 0 || name.size() <= kCpuPrefixLen) {
    return false;
  }
  const bool numeric_suffix =
      std::all_of(name.begin() + static_cast<std::ptrdiff_t>(kCpuPrefixLen),
                  name.end(),
                  [](unsigned char ch) { return std::isdigit(ch) != 0; });
  if (!numeric_suffix) {
    return false;
  }
  *out_cpu_id = std::stoi(name.substr(kCpuPrefixLen));
  return true;
}

std::vector<int> ListOnlineCpus() {
  std::vector<int> cpu_ids;
  for (const auto& entry : std::filesystem::directory_iterator(kSysCpuBase)) {
    int cpu_id = -1;
    if (!IsCpuDir(entry, &cpu_id)) {
      continue;
    }
    const auto cpu_path = entry.path();
    const auto online_path = cpu_path / kOnlineFile;
    if (std::filesystem::exists(online_path)) {
      int online_value = kOnlineYes;
      if (!ReadIntFile(online_path, &online_value) || online_value != kOnlineYes) {
        continue;
      }
    }
    cpu_ids.push_back(cpu_id);
  }
  std::sort(cpu_ids.begin(), cpu_ids.end());
  return cpu_ids;
}

}  // namespace

std::vector<CpuInfo> GetOneThreadPerCoreSameSocket(int socket_id) {
  std::vector<CpuInfo> result;
  const auto cpu_ids = ListOnlineCpus();
  std::vector<int> seen_core_ids;

  for (int cpu_id : cpu_ids) {
    const std::filesystem::path topo_base =
        std::filesystem::path(kSysCpuBase) /
        (std::string(kCpuPrefix) + std::to_string(cpu_id)) /
        kTopologyDir;

    int package_id = -1;
    int core_id = -1;
    if (!ReadIntFile(topo_base / kPackageFile, &package_id)) {
      continue;
    }
    if (!ReadIntFile(topo_base / kCoreFile, &core_id)) {
      continue;
    }
    if (package_id != socket_id) {
      continue;
    }
    const bool already_taken =
        std::find(seen_core_ids.begin(), seen_core_ids.end(), core_id) != seen_core_ids.end();
    if (already_taken) {
      continue;
    }
    seen_core_ids.push_back(core_id);
    result.push_back(CpuInfo{cpu_id, core_id, package_id});
  }

  std::sort(result.begin(), result.end(),
            [](const CpuInfo& lhs, const CpuInfo& rhs) { return lhs.cpu < rhs.cpu; });
  return result;
}

}  // namespace icore
