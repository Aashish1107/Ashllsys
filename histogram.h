#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

class Histogram{
    private:
        std::vector<std::uint64_t> samples;
        std::size_t dropped = 0;
    public:
        explicit Histogram(std::size_t capacity){
            samples.reserve(capacity);
        }

        void record(std::uint64_t v){
            if(samples.size() < samples.capacity())
                samples.push_back(v);
            else ++dropped;
        }

        std::uint64_t percentile(std::size_t num, std::size_t den){
            if(samples.empty()) return 0;
            if(!std::is_sorted(samples.begin(), samples.end()))
                std::sort(samples.begin(), samples.end());
            std::size_t rank = (samples.size()*num+den-1)/den;
            return samples[rank-1];
        }

        void print(const char* name, double scale =1.0){
            std::printf("%s: n=%zu\n dropped=%zu\n p50=%.2f\n p99=%.2f\n p99.9=%.2f\n max=%.2f\n",
                name, samples.size(), dropped,
                percentile(50,100)*scale,
                percentile(99,100)*scale,
                percentile(999,1000)*scale,
                percentile(100,100)*scale
            );
        }
};