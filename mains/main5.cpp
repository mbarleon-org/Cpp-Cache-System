// main.cpp
#include <string>
#include <iostream>
#include <type_traits>
#include "cache/helpers/MutexLocks.hpp"
#include "cache/Base.hpp"
#include "cache/strategy/2Q.hpp"
#include "cache/strategy/LRU.hpp"
#include "cache/strategy/MRU.hpp"
#include "cache/strategy/SLRU.hpp"
#include "cache/strategy/FIFO.hpp"

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

template<class Strategy>
static void test_policy(const std::string& label) {
    using K = int;
    using V = int;
    // Single-thread test: disable internal locking to keep things simple/fast
    cache::Base<K, V, Strategy, std::hash<K>, std::equal_to<K>, cache::mutex_locks::NoLock> cache(/*capacity*/3);

    std::cout << "\n=== " << label << " ===\n";
    check_eq("capacity()", cache.capacity(), std::size_t(3));

    // Empty miss
    {
        V out{};
        check_false("miss on empty get(1)", cache.get(1, out));
    }

    // Insert 3 keys
    cache.put(1, 100);
    cache.put(2, 200);
    cache.put(3, 300);

    // Sanity hits
    {
        V out{};
        check_true("get(1)", cache.get(1, out));
        check_eq("value(1)", out, 100);
        check_true("get(2)", cache.get(2, out));
        check_eq("value(2)", out, 200);
        check_true("get(3)", cache.get(3, out));
        check_eq("value(3)", out, 300);
    }

    // Make one access pattern before inserting the 4th key to trigger an eviction.
    if constexpr (std::is_same_v<Strategy, cache::strategy::MRU<K,V>>) {
        // MRU evicts the **most-recently used**: make 2 the MRU
        V out{};
        (void)cache.get(2, out);
        // Insert 4 -> expect 2 evicted
        cache.put(4, 400);
        {
            V v{};
            check_false("MRU: key 2 should be evicted", cache.get(2, v));
            check_true("MRU: key 1 should remain", cache.get(1, v));
            check_true("MRU: key 3 should remain", cache.get(3, v));
            check_true("MRU: key 4 present", cache.get(4, v));
        }
    } else if constexpr (std::is_same_v<Strategy, cache::strategy::LRU<K,V>>) {
        // LRU evicts **least-recently used**:
        // After puts (and get checks above), touch 2 to make it recent; 1 remains LRU.
        V out{};
        (void)cache.get(2, out);  // 1 is LRU now
        cache.put(4, 400);        // expect 1 evicted
        {
            V v{};
            check_false("LRU: key 1 should be evicted", cache.get(1, v));
            check_true("LRU: key 2 should remain", cache.get(2, v));
            check_true("LRU: key 3 should remain", cache.get(3, v));
            check_true("LRU: key 4 present", cache.get(4, v));
        }
    } else if constexpr (std::is_same_v<Strategy, cache::strategy::FIFO<K,V>>) {
        // FIFO evicts the **earliest inserted**; we don't do any access reordering.
        cache.put(4, 400);        // expect 1 evicted
        {
            V v{};
            check_false("FIFO: key 1 should be evicted", cache.get(1, v));
            check_true("FIFO: key 2 should remain", cache.get(2, v));
            check_true("FIFO: key 3 should remain", cache.get(3, v));
            check_true("FIFO: key 4 present", cache.get(4, v));
        }
    } else if constexpr (std::is_same_v<Strategy, cache::strategy::TwoQueues<K,V>>) {
        // 2Q: new items start in A1; we promote 2 into Am with a hit; eviction prefers A1.
        V out{};
        (void)cache.get(2, out);  // promote 2 to Am in your TwoQ impl
        cache.put(4, 400);        // expect 1 evicted from A1
        {
            V v{};
            check_false("2Q: key 1 should be evicted (A1 LRU)", cache.get(1, v));
            check_true("2Q: key 2 should remain (Am)", cache.get(2, v));
            check_true("2Q: key 3 should remain (A1)", cache.get(3, v));
            check_true("2Q: key 4 present (A1)", cache.get(4, v));
        }
    } else if constexpr (std::is_same_v<Strategy, cache::strategy::SLRU<K,V>>) {
    // Use a fresh cache for a deterministic sequence
        cache::Base<K, V, Strategy, std::hash<K>, std::equal_to<K>, cache::mutex_locks::NoLock> c(3);

        c.put(1, 100);
        c.put(2, 200);
        c.put(3, 300);

        int out{};
        (void)c.get(2, out);  // promote 2
        (void)c.get(3, out);  // promote 3
        c.put(4, 400);        // should evict 1 (probation LRU)

        check_false("SLRU: key 1 should be evicted", c.get(1, out));
        check_true ("SLRU: key 2 should remain (protected)", c.get(2, out));
        check_true ("SLRU: key 3 should remain (protected)", c.get(3, out));
        check_true ("SLRU: key 4 present (probation)", c.get(4, out));
    } else {
        // Fallback: just trigger an eviction and report size
        cache.put(4, 400);
        check_eq("size() post-eviction", cache.size(), std::size_t(3));
    }
}

int main() {
    test_policy<cache::strategy::LRU<int,int>>("LRU");
    test_policy<cache::strategy::MRU<int,int>>("MRU");
    test_policy<cache::strategy::FIFO<int,int>>("FIFO");
    test_policy<cache::strategy::TwoQueues<int,int>>("2Q");
    test_policy<cache::strategy::SLRU<int,int>>("SLRU");
    std::cout << "\nAll tests done.\n";
    return 0;
}
