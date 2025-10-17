// main.cpp
#include <iostream>
#include "cache/Base.hpp"
#include "cache/helpers/MutexLocks.hpp"         // adjust path
#include "cache/strategy/HalvedLFU.hpp"  // your LFU-with-halving strategy

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

// Helper: try get and compare value
template <class Cache>
static void expect_present(Cache& c, int k, int vexp, const char* lbl) {
    int v{};
    bool hit = c.get(k, v);
    check_true(lbl, hit);
    if (hit) check_eq("value", v, vexp);
}

int main() {
    using K = int;
    using V = int;

    using Cache = cache::Base<
        K, V,
        cache::strategy::HalvedLFU<K,V>,  // bucketed LFU + decayAll()
        std::hash<K>, std::equal_to<K>,
        cache::mutex_locks::NoLock
    >;

    // ---- Test 1: Basic LFU eviction pre-decay ----
    {
        std::cout << "\n=== HalvedLFU: basic LFU before decay ===\n";
        Cache cache(3);
        check_eq("capacity()", cache.capacity(), std::size_t(3));
        cache.put(1, 100); // f1
        cache.put(2, 200); // f1
        cache.put(3, 300); // f1

        int out{};
        (void)cache.get(1, out); // 1:2
        (void)cache.get(1, out); // 1:3
        (void)cache.get(2, out); // 2:2

        cache.put(4, 400);       // evict min freq key -> 3 (f1)
        check_false("evict min freq (3)", cache.get(3, out));
        check_true ("keep 1", cache.get(1, out));
        check_true ("keep 2", cache.get(2, out));
        check_true ("present 4", cache.get(4, out));
    }

    // ---- Test 2: Halving changes who is "cold" ----
    // Scenario: 1 was very hot, then we trigger decay; 1’s freq halves and may no longer dominate.
    {
        std::cout << "\n=== HalvedLFU: halving rebalances popularity ===\n";
        Cache cache(3);

        cache.put(10, 1000); // f1
        cache.put(11, 1100); // f1
        cache.put(12, 1200); // f1

        // Make key 10 very hot now
        int out{};
        for (int i = 0; i < 50000; ++i) (void)cache.get(10, out); // freq grows

        // Force a decay pass (assumes your strategy exposes a way; if not, do enough ops to hit the period)
        // Either: cache.strategy().decayAll();  (if you expose it)
        // Or: do N dummy ops; here we do put/get churn on existing keys to advance internal counter
        for (int i = 0; i < 20000; ++i) {
            (void)cache.get(((i & 1) ? 11 : 12), out);
        }
        // After decayAll() internally triggers at least once, freq(10) is halved repeatedly.

        cache.put(13, 1300);

        bool h10 = cache.get(10, out);
        bool h11 = cache.get(11, out);
        bool h12 = cache.get(12, out);
        bool h13 = cache.get(13, out);

        check_false ("10 should be evicted after decay", h10);
        check_true ("13 should be present", h13);
        std::cout << "[INFO] survivors after decay insert: 10=" << h10
                  << " 11=" << h11 << " 12=" << h12 << " 13=" << h13 << "\n";
    }

    // ---- Test 3: Tie-break (LRU-within-frequency) still respected after decays ----
    {
        std::cout << "\n=== HalvedLFU: LRU within same freq still applies ===\n";
        Cache cache(3);
        cache.put(1, 100);
        cache.put(2, 200);
        cache.put(3, 300);

        // no accesses -> all freq=1, LRU order is [front:3, 2, 1:back] if you push_front on insert
        // Decay shouldn’t change anything (all f==1).
        // Trigger a decay pass (optional; no effect when f==1):
        for (int i = 0; i < 5000; ++i) { int dummy{}; (void)cache.get(3, dummy); }

        cache.put(4, 400); // should evict key 1 (LRU within min freq)
        int out{};
        check_false("evict LRU within freq=1 (1)", cache.get(1, out));
        check_true ("keep 2", cache.get(2, out));
        check_true ("keep 3", cache.get(3, out));
        check_true ("present 4", cache.get(4, out));
    }

    std::cout << "\nAll HalvedLFU tests done.\n";
    return 0;
}
