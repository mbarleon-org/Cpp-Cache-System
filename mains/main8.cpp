// main.cpp
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "cache/Base.hpp"
#include "cache/helpers/MutexLocks.hpp"        // adjust path if needed
#include "cache/strategy/RedisLFU.hpp"  // the strategy provided earlier

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

    // Cache alias: StrategyCache with RedisLFU + data:: (single-threaded test)
    using Cache = cache::Base<
        K, V,
        cache::strategy::RedisLFU<K,V>,
        std::hash<K>, std::equal_to<K>,
        cache::mutex_locks::NoLock
    >;

    std::cout << "\n=== RedisLFU: basic insert/get ===\n";
    {
        Cache cache(3);
        check_eq("capacity()", cache.capacity(), std::size_t(3));


        // Miss on empty
        int out{};
        check_false("miss on empty get(1)", cache.get(1, out));

        // Insert 3 keys
        cache.put(1, 100);
        cache.put(2, 200);
        cache.put(3, 300);

        // Basic hits
        check_true("get(1)", cache.get(1, out));
        check_eq  ("value(1)", out, 100);
        check_true("get(2)", cache.get(2, out));
        check_eq  ("value(2)", out, 200);
        check_true("get(3)", cache.get(3, out));
        check_eq  ("value(3)", out, 300);
    }

    std::cout << "\n=== RedisLFU: time decay demonstration (optional) ===\n";
    std::cout << "[INFO] This block shows how to observe time-based decay; it may sleep.\n";
    {
        Cache cache(2);

        cache.put(10, 1000);
        cache.put(11, 1100);

        int out{};
        for (int i = 0; i < 1000; ++i) (void)cache.get(10, out); // heat key 10

        std::cout << "[INFO] Sleeping ~65s to allow 1 minute decay step (Ctrl+C to skip)…\n";
        // If you don't want to sleep in your test, comment the next two lines.
        std::this_thread::sleep_for(std::chrono::seconds(65));
        // After a minute, key 10/11 will have decayed depending on last access times.

        // Insert 12 to trigger eviction; hot-but-decayed key 10 should still tend to win
        cache.put(12, 1200);

        bool h10 = cache.get(10, out);
        bool h11 = cache.get(11, out);
        bool h12 = cache.get(12, out);
        check_true("12 present", h12);
        std::cout << "[INFO] survivors after decay+insert: "
                  << "10=" << h10 << " 11=" << h11 << " 12=" << h12 << "\n";
    }

    std::cout << "\nAll RedisLFU tests done.\n";
    return 0;
}
