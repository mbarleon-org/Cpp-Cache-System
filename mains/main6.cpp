// main.cpp
#include <iostream>
#include "cache/Base.hpp"
#include "cache/helpers/MutexLocks.hpp"
#include "cache/strategy/LFU.hpp"

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

    // Use NoLock for single-threaded test clarity
    using Cache = cache::Base<
        K, V,
        cache::strategy::LFU<K,V>,
        std::hash<K>, std::equal_to<K>,
        cache::mutex_locks::NoLock
    >;

    // ---------------- Test 1: Evict lowest frequency ----------------
    {
        std::cout << "\n=== LFU: evict lowest frequency ===\n";
        Cache cache(3);
        check_eq("capacity()", cache.capacity(), std::size_t(3));

        // Insert 1,2,3 (all start at freq=1)
        cache.put(1, 100);
        cache.put(2, 200);
        cache.put(3, 300);

        // Bump frequencies: 1 -> 3, 2 -> 2, 3 -> 1
        int out{};
        (void)cache.get(1, out); // 1:2
        (void)cache.get(1, out); // 1:3
        (void)cache.get(2, out); // 2:2

        // Insert 4; min freq is 1 => evict key 3
        cache.put(4, 400);

        check_false("get(3) after insert(4): key 3 should be evicted (min freq)", cache.get(3, out));
        check_true ("get(1) survives (freq=3)", cache.get(1, out));
        check_true ("get(2) survives (freq=2)", cache.get(2, out));
        check_true ("get(4) present (freq=1)", cache.get(4, out));
    }

    // --------------- Test 2: Tie-break by LRU within min freq ---------------
    {
        std::cout << "\n=== LFU: tie-break LRU within same freq ===\n";
        Cache cache(3);

        // Insert 1,2,3 with no accesses → all freq=1.
        // With push_front on insert, the list order for freq=1 is: front=3,2,1=back.
        cache.put(1, 100);
        cache.put(2, 200);
        cache.put(3, 300);

        // Insert 4 triggers eviction in freq=1.
        // Evict LRU within freq=1 bucket => key 1 (at back) should go.
        cache.put(4, 400);

        int out{};
        check_false("tie-break: key 1 should be evicted (LRU within freq=1)", cache.get(1, out));
        check_true ("tie-break: key 2 should remain", cache.get(2, out));
        check_true ("tie-break: key 3 should remain", cache.get(3, out));
        check_true ("tie-break: key 4 present", cache.get(4, out));
    }

    // --------------- Test 3: Update value without affecting frequency ---------------
    // If your put() “update” path calls onAccess, this will also bump frequency.
    // Adjust expectation depending on your chosen semantics.
    {
        std::cout << "\n=== LFU: update semantics (upsert-touch) ===\n";
        Cache cache(2);

        cache.put(10, 1000); // f=1
        cache.put(11, 1100); // f=1

        // Update key 10; by default our earlier code counted it as an access (touch)
        cache.put(10, 1001);

        int out{};
        (void)cache.get(10, out); // ensure it’s present
        check_eq("updated value for key 10", out, 1001);

        // Now insert 12 -> if updates count as access, 10 has higher freq than 11,
        // so 11 should be evicted; otherwise (no-touch), tie-break LRU decides.
        cache.put(12, 1200);

        bool hit11 = cache.get(11, out);
        bool hit10 = cache.get(10, out);
        bool hit12 = cache.get(12, out);

        std::cout << "[INFO] presence after insert(12): "
                  << "10=" << hit10 << " 11=" << hit11 << " 12=" << hit12 << "\n";
        // We won't assert here because it depends on your chosen put() semantics.
    }

    std::cout << "\nAll LFU tests done.\n";
    return 0;
}
