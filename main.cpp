#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <random>
#include <thread>
#include <vector>
#include "histogram.h"
#include "tsc.h"

int main() {

    std::vector<std::uint64_t> values(1000);
    std::iota(values.begin(), values.end(), 1);
    std::shuffle(values.begin(), values.end(), std::mt19937{42});
    Histogram h(values.size());
    for (auto v : values) h.record(v);
    h.print("1..1000");
    if (h.percentile(50, 100) != 500 || h.percentile(99, 100) != 990 ||
        h.percentile(999, 1000) != 999 || h.percentile(100, 100) != 1000) {
        std::puts("histogram self-check FAILED");
        return 1;
    }

    const double ns_per_tick = calibrate_ns_per_tick();
    std::printf("ns per tick: %.6f (TSC %.3f GHz)\n", ns_per_tick, 1.0 / ns_per_tick);

    for (int ms : {100, 1}) {
        auto t0 = rdtsc();
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        auto t1 = rdtsc();
        std::printf("sleep %3d ms -> measured %.3f ms\n", ms, (t1 - t0) * ns_per_tick / 1e6);
    }
}