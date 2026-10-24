#pragma once
#include <cstddef>
#include <mutex>
#include <queue>

template <class T>
class MutexQueue{
    public:

        explicit MutexQueue(std::size_t capacity) : capacity(capacity){}

        bool try_push(const T& v){
            std::lock_guard lock(m);
            if(q.size() >=capacity) return false;
            q.push(v);
            return true;
        }
        bool try_pop(T& out){
            std::lock_guard lock(m);
            if(q.empty()) return false;
            out = q.front();
            q.pop();
            return true;
        }

    private:
        std::mutex m;
        std::queue<T> q;
        std::size_t capacity;
};