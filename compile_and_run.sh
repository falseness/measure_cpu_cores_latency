mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./icore_latency --tsc-ghz 3.6 --iters 100000 --warmup 5000 --socket 0