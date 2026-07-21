// Base cache behavior tests.
#include <Cache/Base.hpp>
#include <Cache/Helpers/MutexLocks.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <atomic>
#include <iostream>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

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

using IntStringCache = cache::Base<int, std::string, cache::strategy::LRU<int, std::string>, std::hash<int>, std::equal_to<int>, cache::mutex_locks::NoLock>;

static void test_basic_operations()
{
    std::cout << "\n=== LRU: basic operations ===\n";
    IntStringCache cache(3);
    check_eq("capacity()", cache.capacity(), std::size_t(3));

    std::string value{};
    check_false("empty cache get(1)", cache.get(1, value));
    check_eq("size() after miss", cache.size(), std::size_t(0));

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    check_eq("size() after three inserts", cache.size(), std::size_t(3));

    check_true("get(1) after insert", cache.get(1, value));
    check_eq("value for key 1", value, std::string("one"));
    check_true("get(2) after insert", cache.get(2, value));
    check_eq("value for key 2", value, std::string("two"));
    check_true("get(3) after insert", cache.get(3, value));
    check_eq("value for key 3", value, std::string("three"));
}

static void test_lru_eviction()
{
    std::cout << "\n=== LRU: eviction policy ===\n";
    IntStringCache cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    std::string value{};
    (void) cache.get(1, value); // make key 1 MRU

    cache.put(4, "four");

    check_true("key 1 stays present", cache.get(1, value));
    check_eq("value for key 1 after eviction", value, std::string("one"));
    check_false("key 2 evicted (LRU)", cache.get(2, value));
    check_true("key 3 still present", cache.get(3, value));
    check_true("key 4 inserted", cache.get(4, value));
    check_eq("cache size remains capacity", cache.size(), std::size_t(3));
}

static void test_update_existing()
{
    std::cout << "\n=== LRU: updating an existing entry ===\n";
    IntStringCache cache(3);

    cache.put(1, "original");
    cache.put(2, "two");
    cache.put(3, "three");

    std::string value{};
    check_true("get(1) before update", cache.get(1, value));
    check_eq("value before update", value, std::string("original"));

    cache.put(1, "updated");
    check_true("get(1) after update", cache.get(1, value));
    check_eq("value after update", value, std::string("updated"));
    check_eq("size() remains constant", cache.size(), std::size_t(3));
}

static void test_remove()
{
    std::cout << "\n=== LRU: remove() ===\n";
    IntStringCache cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    std::string value{};
    (void) cache.get(1, value); // order: 2 oldest -> 3 -> 1 newest

    cache.remove(2);
    check_eq("size() decreases after remove", cache.size(), std::size_t(2));
    check_false("removed key is absent", cache.get(2, value));
    check_true("remove keeps key 1", cache.get(1, value));
    check_true("remove keeps key 3", cache.get(3, value));

    cache.remove(99);
    check_eq("removing a missing key is a no-op", cache.size(), std::size_t(2));

    // Refill and overflow the cache to verify remove() also updates the
    // strategy's eviction bookkeeping.
    cache.put(4, "four");
    cache.put(5, "five");
    check_eq("size() remains at capacity after refill", cache.size(), std::size_t(3));
    check_false("oldest remaining key is evicted", cache.get(1, value));
    check_true("key 3 remains after refill", cache.get(3, value));
    check_true("key 4 remains after refill", cache.get(4, value));
    check_true("new key is inserted after refill", cache.get(5, value));
}

static void test_clear()
{
    std::cout << "\n=== LRU: clear() ===\n";
    IntStringCache cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    check_eq("size() before clear", cache.size(), std::size_t(2));

    cache.clear();
    check_eq("size() after clear", cache.size(), std::size_t(0));

    std::string value{};
    check_false("get(1) after clear", cache.get(1, value));
}

static void test_invalidate_if()
{
    std::cout << "\n=== LRU: invalidateIf() ===\n";
    IntStringCache cache(3);
    int            callback_calls = 0;

    check_false("cache initially has no invalidation predicate", cache.hasInvalidationPredicate());
    cache.put(1, "stale");
    cache.put(2, "fresh");
    cache.invalidateIf([&callback_calls](const int& key, const std::string& value) {
        ++callback_calls;
        return key == 1 && value == "stale";
    });
    check_true("invalidateIf registers an invalidation predicate", cache.hasInvalidationPredicate());

    std::string value{};
    check_true("nonmatching entry remains available", cache.get(2, value));
    check_eq("nonmatching entry keeps its value", value, std::string("fresh"));
    check_false("contains reports an invalidated entry as absent", cache.contains(1));
    check_eq("matching entry is removed", cache.size(), std::size_t(1));
    check_false("invalidated entry remains absent", cache.get(1, value));
    check_eq("callback runs only for entries found in the cache", callback_calls, 2);
}

static void test_clear_invalidation_predicate()
{
    std::cout << "\n=== LRU: clearInvalidationPredicate() ===\n";
    IntStringCache cache(3);
    int            callback_calls = 0;

    cache.put(1, "stale");
    cache.invalidateIf([&callback_calls](const int&, const std::string&) {
        ++callback_calls;
        return true;
    });
    check_true("cache reports the predicate before it is cleared", cache.hasInvalidationPredicate());
    cache.clearInvalidationPredicate();
    check_false("cache reports no predicate after it is cleared", cache.hasInvalidationPredicate());

    std::string value{};
    check_true("entry remains available after clearing the predicate", cache.get(1, value));
    check_eq("entry keeps its value after clearing the predicate", value, std::string("stale"));
    check_eq("cleared predicate is not invoked", callback_calls, 0);
}

static void test_contains()
{
    std::cout << "\n=== LRU: contains() ===\n";

    IntStringCache cache(2);
    cache.put(1, "one");
    cache.put(2, "two");
    check_true("contains finds an existing key", cache.contains(1));
    check_false("contains misses an absent key", cache.contains(3));
    cache.put(3, "three");

    std::string value{};
    check_false("contains does not count as an access by default", cache.get(1, value));

    IntStringCache accessed_cache(2);
    accessed_cache.put(1, "one");
    accessed_cache.put(2, "two");
    check_true("contains can count as an access", accessed_cache.contains(1, true));
    accessed_cache.put(3, "three");
    check_true("counted contains keeps the accessed key", accessed_cache.get(1, value));
    check_false("counted contains makes the other key the eviction candidate", accessed_cache.get(2, value));

    IntStringCache invalidating_cache(2);
    int            predicate_calls = 0;
    invalidating_cache.put(1, "stale");
    invalidating_cache.invalidateIf([&predicate_calls](const int&, const std::string& current) {
        ++predicate_calls;
        return current == "stale";
    });
    check_false("contains evaluates the invalidation predicate", invalidating_cache.contains(1));
    check_eq("contains removes an invalidated entry", invalidating_cache.size(), std::size_t(0));
    check_false("contains misses an entry after invalidating it", invalidating_cache.contains(1));
    check_eq("contains does not invoke the predicate for an absent key", predicate_calls, 1);
}

static void test_conditional_puts()
{
    std::cout << "\n=== LRU: conditional puts ===\n";

    IntStringCache cache(3);
    check_true("putIfAbsent inserts a missing key", cache.putIfAbsent(1, "one"));
    check_false("putIfAbsent rejects an existing key", cache.putIfAbsent(1, "replacement"));

    std::string value{};
    check_true("conditionally inserted key is present", cache.get(1, value));
    check_eq("rejected putIfAbsent preserves the value", value, std::string("one"));
    check_false("putIfPresent rejects a missing key", cache.putIfPresent(2, "two"));
    check_true("putIfPresent updates an existing key", cache.putIfPresent(1, "updated"));
    check_true("conditionally updated key is present", cache.get(1, value));
    check_eq("putIfPresent stores the new value", value, std::string("updated"));

    IntStringCache invalidating_cache(3);
    invalidating_cache.invalidateIf([](const int&, const std::string& current) { return current == "stale"; });
    invalidating_cache.put(1, "stale");
    check_true("putIfAbsent replaces an invalidated entry", invalidating_cache.putIfAbsent(1, "fresh"));
    check_true("replacement for invalidated entry is present", invalidating_cache.get(1, value));
    check_eq("replacement for invalidated entry stores the new value", value, std::string("fresh"));

    invalidating_cache.put(2, "stale");
    check_false("putIfPresent rejects an invalidated entry", invalidating_cache.putIfPresent(2, "updated"));
    check_false("rejected invalidated entry is removed", invalidating_cache.contains(2));
}

static void test_concurrent_put_if_absent()
{
    std::cout << "\n=== LRU: concurrent putIfAbsent() ===\n";
    using ThreadSafeCache = cache::Base<int, int, cache::strategy::LRU<int, int>, std::hash<int>, std::equal_to<int>, std::shared_mutex>;

    ThreadSafeCache          cache(4);
    std::atomic<bool>        start{false};
    std::atomic<int>         successful_inserts{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 16; ++i)
    {
        threads.emplace_back([&cache, &start, &successful_inserts, i] {
            while (!start.load(std::memory_order_acquire))
            {
            }
            if (cache.putIfAbsent(42, i))
            {
                successful_inserts.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& thread : threads)
    {
        thread.join();
    }

    check_eq("exactly one concurrent putIfAbsent succeeds", successful_inserts.load(), 1);
    check_eq("concurrent putIfAbsent stores one entry", cache.size(), std::size_t(1));
    check_true("concurrently inserted key is present", cache.contains(42));
}

static void test_string_keys()
{
    std::cout << "\n=== LRU: string keys ===\n";
    using StringIntCache =
        cache::Base<std::string, int, cache::strategy::LRU<std::string, int>, std::hash<std::string>, std::equal_to<std::string>, cache::mutex_locks::NoLock>;

    StringIntCache cache(2);
    check_eq("capacity()", cache.capacity(), std::size_t(2));

    cache.put("hello", 1);
    cache.put("world", 2);
    cache.put("test", 3); // should evict "hello"

    int value = 0;
    check_false("string key 'hello' evicted", cache.get("hello", value));
    check_true("string key 'world' present", cache.get("world", value));
    check_eq("value for 'world'", value, 2);
    check_true("string key 'test' present", cache.get("test", value));
    check_eq("value for 'test'", value, 3);
}

static void test_zero_capacity_behavior()
{
    std::cout << "\n=== LRU: zero capacity handling ===\n";
    try
    {
        IntStringCache cache(0);
        std::string    value{};
        cache.put(1, "should not store");
        check_eq("size() stays zero", cache.size(), std::size_t(0));
        check_false("get() on zero-capacity cache", cache.get(1, value));
    }
    catch (const std::exception& e)
    {
        std::cout << "[INFO] zero capacity threw exception: " << e.what() << "\n";
    }
}

static void test_complex_lru_behavior()
{
    std::cout << "\n=== LRU: recency ordering ===\n";
    IntStringCache cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    std::string value{};
    (void) cache.get(1, value); // order: 2 oldest -> 3 -> 1 newest
    (void) cache.get(2, value); // order: 3 oldest -> 1 -> 2 newest

    cache.put(4, "four"); // should evict key 3

    check_false("key 3 evicted after insert(4)", cache.get(3, value));
    check_true("key 1 remains", cache.get(1, value));
    check_true("key 2 remains", cache.get(2, value));
    check_true("key 4 present", cache.get(4, value));
    check_eq("size() stays at capacity", cache.size(), std::size_t(3));
}

int main()
{
    try
    {
        test_basic_operations();
        test_lru_eviction();
        test_update_existing();
        test_remove();
        test_clear();
        test_invalidate_if();
        test_clear_invalidation_predicate();
        test_contains();
        test_conditional_puts();
        test_concurrent_put_if_absent();
        test_string_keys();
        test_complex_lru_behavior();
        test_zero_capacity_behavior();
    }
    catch (const std::exception& e)
    {
        std::cout << "[FAIL] unhandled exception: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nAll LRU cache tests done.\n";
    return 0;
}
