// main.cpp
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include "cache/Fragmented.hpp"

// Shared check helpers
template <typename T>
static void check_eq(const char* name, const T& got, const T& expected) {
    if (got == expected) {
        std::cout << "[OK]   " << name << " | got=" << got << " expected=" << expected << "\n";
    } else {
        std::cout << "[FAIL] " << name << " | got=" << got << " expected=" << expected << "\n";
    }
}
static void check_true(const char* name, bool cond) {
    std::cout << (cond ? "[OK]   " : "[FAIL] ") << name << " | expected true\n";
}
static void check_false(const char* name, bool cond) {
    std::cout << (!cond ? "[OK]   " : "[FAIL] ") << name << " | expected false\n";
}

int main() {
    using K = int;
    using V = int;
    // Cache type: 4 fragments, total capacity 8 => per-fragment capacity = 2
    // Inner shards use std::mutex (you can try NoLock or shared_mutex too).
    using Cache = cache::Fragmented<
        K, V,
        cache::strategy::LRU<K, V>,     // Strategy
        std::hash<K>, std::equal_to<K>,   // Hash/Eq for registry
        std::shared_mutex,                // Mutex for registry
        std::mutex                        // InnerMutex for each shard
    >;

    Cache cache(/*fragments*/4, /*capacity*/8);

    // --- Basic properties ---
    check_eq("capacity()", cache.capacity(), static_cast<std::size_t>(8));
    check_true("isMtSafe()", cache.isMtSafe());

    // --- Miss on empty cache ---
    {
        V out{};
        bool hit = cache.get(42, out);
        check_false("miss on empty get(42)", hit);
    }

    // --- Put + Get, across shards ---
    // shard index = key % fragments (since std::hash<int> is identity-ish)
    cache.put(0, 100); // shard 0
    cache.put(1, 200); // shard 1
    cache.put(2, 300); // shard 2
    cache.put(3, 400); // shard 3

    {
        V out{};
        bool hit = cache.get(0, out);
        check_true("get(0) after put", hit);
        check_eq("value for key 0", out, 100);
    }
    {
        V out{};
        bool hit = cache.get(3, out);
        check_true("get(3) after put", hit);
        check_eq("value for key 3", out, 400);
    }

    // --- Per-fragment LRU eviction check ---
    // per-shard capacity is 2. For shard 0 (keys 0,4,8 share same shard):
    // Insert 0 (already there), then 4, then 8 => shard0 should evict the oldest.
    cache.put(4, 140); // shard 0 now has keys {0,4}
    cache.put(8, 180); // shard 0 inserts 8, capacity 2 -> evicts LRU of {0,4}
                       // Expected: key 0 was older than 4, so 0 evicted; {4,8} remain.

    {
        V out{};
        bool hit0 = cache.get(0, out);
        check_false("LRU eviction -> key 0 should be gone (shard 0)", hit0);

        bool hit4 = cache.get(4, out);
        check_true("key 4 should remain (shard 0)", hit4);
        check_eq("value for key 4", out, 140);

        bool hit8 = cache.get(8, out);
        check_true("key 8 present (shard 0)", hit8);
        check_eq("value for key 8", out, 180);
    }

    // Touch 4 to make it MRU in its shard, then insert 12 to force eviction of 8 instead.
    {
        V out{};
        (void)cache.get(4, out);  // access 4 -> becomes MRU in shard 0
        cache.put(12, 212);       // shard 0: {4 (MRU), 8 (LRU)} -> insert 12 -> evict 8
        bool hit8 = cache.get(8, out);
        check_false("after touching 4 then inserting 12, key 8 should be evicted", hit8);

        bool hit4 = cache.get(4, out);
        check_true("key 4 should remain (now with 12)", hit4);
        bool hit12 = cache.get(12, out);
        check_true("key 12 present", hit12);
        check_eq("value for key 12", out, 212);
    }

    // --- Size accounting ---
    {
        auto s = cache.size();
        // We inserted: (1,2,3) still present, shard0 has {4,12}, so total should be 2 + 3 = 5
        // (keys 1,2,3 = 3; keys 4,12 = 2)  -> 5
        check_eq("size() after inserts/evictions", s, static_cast<std::size_t>(5));
    }

    // --- Light concurrency smoke test ---
    {
        std::atomic<bool> start{false};
        std::vector<std::thread> threads;

        // Threads t = 1..3 target shards 1..3.
        for (int t = 1; t <= 3; ++t) {
            threads.emplace_back([t, &cache, &start]() {
                while (!start.load(std::memory_order_acquire)) {}
                for (int i = 0; i < 1000; ++i) {
                    int k = t + 4 * (i % 50); // shard = t % 4 (1..3)
                    cache.put(k, k * 10);
                    int v{};
                    (void)cache.get(k, v);
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& th : threads) th.join();

        // Now probe shard 0 (untouched by threads)
        int out{};
        bool hit = cache.get(4, out);
        check_true("concurrency sanity - get(4) (shard 0 reserved)", hit);
        std::cout << "[INFO] post-concurrency get(4) => " << out << "\n";
    }

    std::cout << "\nAll Fragmented cache tests done.\n";
    return 0;
}
