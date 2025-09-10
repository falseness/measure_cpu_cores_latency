# measure_cpu_cores_latency

mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./icore_latency --iters 50000 --warmup 5000 --socket 0 --csv
