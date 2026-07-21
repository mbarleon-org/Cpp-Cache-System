// Base cache behavior tests.
#include <Cache/Base.hpp>
#include <Cache/Helpers/MutexLocks.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <iostream>
#include <string>

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

    cache.put(1, "stale");
    cache.put(2, "fresh");
    cache.invalidateIf([&callback_calls](const int& key, const std::string& value) {
        ++callback_calls;
        return key == 1 && value == "stale";
    });

    std::string value{};
    check_true("nonmatching entry remains available", cache.get(2, value));
    check_eq("nonmatching entry keeps its value", value, std::string("fresh"));
    check_false("matching entry is returned as a miss", cache.get(1, value));
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
    cache.clearInvalidationPredicate();

    std::string value{};
    check_true("entry remains available after clearing the predicate", cache.get(1, value));
    check_eq("entry keeps its value after clearing the predicate", value, std::string("stale"));
    check_eq("cleared predicate is not invoked", callback_calls, 0);
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
