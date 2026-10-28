#include <cstdint>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include "mutex_queue.h"
#include "spsc_queue.h"

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

int main(int argc, char** argv) {
    const std::uint64_t n = argc>1 ? std::strtoull(argv[1], nullptr, 10) :100'000'000;
    MutexQueue<std::uint64_t> q(1024);
    bool mutex_ok = check_order(q, n);
    std::printf("mutex queue: %s\n", mutex_ok ? "PASS" : "FAIL");

    spscQueue<std::uint64_t, 1024>sq;
    const bool spsc_ok = check_order(sq, n);
    std::printf("spsc queue: %s\n", spsc_ok ? "PASS" : "FAIL");

    return mutex_ok && spsc_ok ? 0 : 1 ;
}