// Shared fragmented cache behavior tests.
#include <Cache/SharedFragmented.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

// Shared check helpers
template <typename T>
static void check_eq(const char* name, const T& got, const T& expected)
{
    if (got == expected)
    {
        std::cout << "[OK]   " << name << " | got=" << got << " expected=" << expected << "\n";
    }
    else
    {
        std::cout << "[FAIL] " << name << " | got=" << got << " expected=" << expected << "\n";
    }
}

static void check_true(const char* name, bool cond)
{
    std::cout << (cond ? "[OK]   " : "[FAIL] ") << name << " | expected true\n";
}

static void check_false(const char* name, bool cond)
{
    std::cout << (!cond ? "[OK]   " : "[FAIL] ") << name << " | expected false\n";
}

// Pick a key that maps to a target fragment index for int keys.
// Assumes FragmentedStrategyCache uses std::hash<int>{}(k) % fragments.
int key_for_fragment(int fragment_idx, int fragments, int seq = 0)
{
    return fragment_idx + fragments * seq;
}

int main()
{
    using K = int;
    using V = int;

    using Cache = cache::SharedFragmented<K, V, cache::strategy::LRU<K, V>, // Strategy
                                          std::hash<K>, std::equal_to<K>,   // Hash/Eq (used inside)
                                          std::shared_mutex,                // Wrapper mutex
                                          std::shared_mutex,                // Fragmented (registry) mutex
                                          std::mutex                        // Per-fragment (inner StrategyCache) mutex
                                          >;

    auto& cache = Cache::getInstance();

    // --------- basic single-thread checks ----------
    check_false("isCacheInitialized()", cache.isCacheInitialized());
    check_false("shared-fragmented cache initially has no invalidation predicate", cache.hasInvalidationPredicate());
    int invalidate_callback_calls = 0;
    cache.invalidateIf([&invalidate_callback_calls](const K& key, const V& value) {
        ++invalidate_callback_calls;
        return key == 42 && value == -42;
    });
    check_true("shared-fragmented cache reports a predicate before initialization", cache.hasInvalidationPredicate());
    cache.remove(42);
    check_eq("remove before initialization is a no-op", cache.size(), std::size_t(0));
    check_false("putIfAbsent before initialization is a no-op", cache.putIfAbsent(7, 70));
    check_false("contains before initialization misses", cache.contains(7));
    cache.initialize(/*fragments*/ 4, /*capacity*/ 64); // 16 per fragment
    check_true("shared-fragmented cache reports the predicate after initialization", cache.hasInvalidationPredicate());
    check_eq("capacity()", cache.capacity(), std::size_t(64));
    check_true("isCacheInitialized()", cache.isCacheInitialized());

    int out = 0;
    check_false("miss on empty get(42)", cache.get(42, out));

    cache.put(42, -42); // its fragment is created after callback registration
    check_false("contains invokes the pre-initialize invalidation predicate", cache.contains(42));
    check_eq("invalidated shared-fragmented entry is removed", cache.size(), std::size_t(0));
    check_eq("callback receives the matching key/value pair", invalidate_callback_calls, 1);

    cache.clearInvalidationPredicate();
    check_false("shared-fragmented cache reports no predicate after clearing it", cache.hasInvalidationPredicate());
    cache.put(42, -42);
    check_true("entry remains available after clearing the shared-fragmented predicate", cache.get(42, out));
    check_eq("cleared shared-fragmented predicate is not invoked", invalidate_callback_calls, 1);
    cache.remove(42);

    check_true("shared-fragmented putIfAbsent inserts a missing key", cache.putIfAbsent(7, 70));
    check_false("shared-fragmented putIfAbsent rejects an existing key", cache.putIfAbsent(7, 71));
    check_true("shared-fragmented contains finds the inserted key", cache.contains(7));
    check_true("shared-fragmented putIfPresent updates an existing key", cache.putIfPresent(7, 72));
    check_true("shared-fragmented conditional value is readable", cache.get(7, out));
    check_eq("shared-fragmented putIfPresent stores the new value", out, 72);
    cache.remove(7);

    cache.put(0, 100);
    check_true("hit get(0) after put", cache.get(0, out));
    check_eq("value for key 0", out, 100);

    cache.put(3, 400);
    check_true("hit get(3) after put", cache.get(3, out));
    check_eq("value for key 3", out, 400);

    check_eq("size() after two inserts", cache.size(), std::size_t(2));

    cache.remove(0);
    check_false("removed key 0 is absent", cache.get(0, out));
    check_true("removing key 0 keeps key 3", cache.get(3, out));
    check_eq("size() decreases after remove", cache.size(), std::size_t(1));

    cache.remove(0);
    check_eq("removing key 0 again is a no-op", cache.size(), std::size_t(1));

    cache.put(0, 100);
    check_true("removed key 0 can be reinserted", cache.get(0, out));
    check_eq("reinserted value for key 0", out, 100);

    // --------- Concurrency A: threads on disjoint fragments ----------
    {
        std::cout << "\n[TEST] Concurrency A: disjoint fragments\n";
        const int fragments       = 4;
        const int threads_n       = 4; // one per fragment
        const int keys_per_thread = 8; // stay well under per-fragment capacity (16)
        const int iters           = 1000;

        std::atomic<bool>        start{false};
        std::vector<std::thread> threads;
        for (int t = 0; t < threads_n; ++t)
        {
            threads.emplace_back([t, fragments, keys_per_thread, iters, &cache, &start] {
                // precompute keys that map to this fragment
                std::vector<int> keys;
                keys.reserve(keys_per_thread);
                for (int i = 0; i < keys_per_thread; ++i)
                {
                    keys.push_back(key_for_fragment(t, fragments, i));
                }
                while (!start.load(std::memory_order_acquire))
                { /* spin */
                }

                for (int i = 0; i < iters; ++i)
                {
                    int k = keys[i % keys_per_thread];
                    int v = k * 10 + (i & 7);
                    cache.put(k, v);
                    int out{};
                    (void) cache.get(k, out); // not asserting exact value due to recency updates
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& th : threads)
            th.join();

        // After join, all these sentinel keys should be present
        for (int frag = 0; frag < fragments; ++frag)
        {
            int  k = key_for_fragment(frag, fragments, 0);
            int  v{};
            bool hit = cache.get(k, v);
            check_true(("post A: get(" + std::to_string(k) + ")").c_str(), hit);
        }
    }

    // --------- Concurrency B: all threads hammer the SAME fragment ----------
    {
        std::cout << "\n[TEST] Concurrency B: same fragment\n";
        const int fragments       = 4;
        const int target_fragment = 2; // everyone writes to this fragment
        const int threads_n       = 8;
        const int keys_pool       = 8; // keep <= per-fragment capacity (16)
        const int iters           = 1000;

        std::vector<int> keys;
        for (int i = 0; i < keys_pool; ++i)
            keys.push_back(key_for_fragment(target_fragment, fragments, i));

        std::atomic<bool>        start{false};
        std::vector<std::thread> threads;
        for (int t = 0; t < threads_n; ++t)
        {
            threads.emplace_back([&] {
                while (!start.load(std::memory_order_acquire))
                {
                }
                for (int i = 0; i < iters; ++i)
                {
                    int k = keys[i % keys_pool];
                    int v = k * 100 + (i & 15);
                    cache.put(k, v);
                    int out{};
                    (void) cache.get(k, out);
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& th : threads)
            th.join();

        // All pool keys should still be present (no eviction expected)
        for (int k : keys)
        {
            int  v{};
            bool hit = cache.get(k, v);
            check_true(("post B: get(" + std::to_string(k) + ")").c_str(), hit);
        }
    }

    // --------- Clear and final sanity ----------
    cache.clear();
    check_eq("size() after clear", cache.size(), std::size_t(0));

    std::cout << "\nAll tests done.\n";
    return 0;
}
