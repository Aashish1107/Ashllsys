#include <cstdint>
#include <cstdio>
#include <thread>
#include "mutex_queue.h"

template <class Queue>
bool check_order(Queue& q, std::uint64_t n) {
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < n; ++i)
            while (!q.try_push(i)) {}   // full: spin until the consumer catches up
    });

    std::uint64_t mismatches = 0;
    for (std::uint64_t expected = 0; expected < n; ++expected) {
        std::uint64_t v;
        while (!q.try_pop(v)) {}        // empty: spin until the producer pushes
        if (v != expected) ++mismatches;
    }
    producer.join();

    std::uint64_t extra;
    return mismatches == 0 && !q.try_pop(extra);   // nothing left over
}

int main() {
    constexpr std::uint64_t N = 100'000'000;
    MutexQueue<std::uint64_t> q(1024);
    bool ok = check_order(q, N);
    std::printf("mutex queue: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}