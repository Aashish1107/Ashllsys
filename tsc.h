#pragma once
#include <chrono>
#include <cstdint>
#include <thread>
#include <x86intrin.h>

inline std::uint64_t rdtsc(){
    _mm_lfence();
    std::uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}

inline double calibrate_ns_per_tick(){
    using clock = std::chrono::steady_clock;
    auto c0 = clock::now();
    auto t0 = rdtsc();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto c1 = clock::now();
    auto t1 = rdtsc();
    
    double ns = std::chrono::duration<double, std::nano>(c1 - c0).count();
    return ns / double(t1 - t0);
}