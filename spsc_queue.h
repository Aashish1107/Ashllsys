#pragma once
#include <atomic>
#include <cstddef>

template<class T, std::size_t Capacity>
class spscQueue{
    static_assert(Capacity>=2 && (Capacity & (Capacity-1))==0, "Capacity must be a power of 2");
    public:
        bool try_push(const T& v){
            const std::size_t w = write.load(std::memory_order_relaxed);
            if(w-read.load(std::memory_order_acquire)==Capacity) return false;
            slots[w & (Capacity-1)] = v;
            write.store(w+1, std::memory_order_release);
            return true;
        }
        bool try_pop(T& out){
            const std::size_t r = read.load(std::memory_order_relaxed);
            if(r==write.load(std::memory_order_acquire)) return false;
            out = slots[r & (Capacity-1)];
            read.store(r+1, std::memory_order_release);
            return true;
        }
    private:
        T slots[Capacity];
        std::atomic<std::size_t> write{0};
        std::atomic<std::size_t> read{0};
};