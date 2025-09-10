#include "cpu_topology.hpp"


#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>


namespace icore {
namespace {


std::vector<int> ListOnlineCpus() {
std::vector<int> cpus;
for (const auto& p : std::filesystem::directory_iterator("/sys/devices/system/cpu")) {
const std::string name = p.path().filename().string();
if (name.rfind("cpu", 0) == 0 && name.size() > 3 &&
std::all_of(name.begin() + 3, name.end(), ::isdigit)) {
const int id = std::stoi(name.substr(3));
if (std::filesystem::exists(p.path() / "online")) {
std::ifstream on(p.path() / "online");
int v = 1; on >> v; if (v != 1) continue;
}
cpus.push_back(id);
}
}
std::sort(cpus.begin(), cpus.end());
return cpus;
}


bool ReadIntFile(const std::filesystem::path& path, int* out) {
std::ifstream f(path);
if (!f) return false;
f >> *out;
return static_cast<bool>(f);
}


} // namespace


std::vector<CpuInfo> GetOneThreadPerCoreSameSocket(int socket) {
std::vector<CpuInfo> out;
const auto cpus = ListOnlineCpus();
std::vector<int> seen_core_ids;
for (int cpu : cpus) {
const auto base = std::filesystem::path("/sys/devices/system/cpu/") / ("cpu" + std::to_string(cpu)) / "topology";
int pkg = -1, core = -1;
if (!ReadIntFile(base / "physical_package_id", &pkg)) continue;
if (!ReadIntFile(base / "core_id", &core)) continue;
if (pkg != socket) continue;
if (std::find(seen_core_ids.begin(), seen_core_ids.end(), core) != seen_core_ids.end()) continue;
seen_core_ids.push_back(core);
out.push_back(CpuInfo{cpu, core, pkg});
}
std::sort(out.begin(), out.end(), [](const CpuInfo& a, const CpuInfo& b) { return a.cpu < b.cpu; });
return out;
}


} // namespace icore