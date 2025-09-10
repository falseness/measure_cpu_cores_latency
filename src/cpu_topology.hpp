#pragma once


#include <vector>


namespace icore {


struct CpuInfo {
int cpu;
int core_id;
int package_id;
};


std::vector<CpuInfo> GetOneThreadPerCoreSameSocket(int socket);


} // namespace icore