// Shared cache behavior tests.
#include <Cache/Shared.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <iostream>
#include <shared_mutex>

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

int main()
{
    using Cache = cache::Shared<int, int, cache::strategy::LRU<int, int>, std::hash<int>, std::equal_to<int>, std::shared_mutex>;

    auto& cache = Cache::getInstance();

    int invalidate_callback_calls = 0;
    cache.invalidateIf([&invalidate_callback_calls](const int& key, const int& value) {
        ++invalidate_callback_calls;
        return key == 42 && value == -42;
    });
    cache.remove(42);
    check_eq("remove before initialization is a no-op", cache.size(), std::size_t(0));

    cache.initialize(3);
    cache.put(42, -42);
    int out{};
    check_false("pre-initialize invalidateIf callback invalidates a later entry", cache.get(42, out));
    check_eq("invalidated shared entry is removed", cache.size(), std::size_t(0));
    check_eq("callback receives the matching key/value pair", invalidate_callback_calls, 1);

    cache.clearInvalidationPredicate();
    cache.put(42, -42);
    check_true("entry remains available after clearing the shared predicate", cache.get(42, out));
    check_eq("cleared shared predicate is not invoked", invalidate_callback_calls, 1);
    cache.remove(42);

    cache.put(1, 100);
    cache.put(2, 200);
    cache.put(3, 300);

    (void) cache.get(1, out); // order: 2 oldest -> 3 -> 1 newest

    cache.remove(2);
    check_eq("size() decreases after remove", cache.size(), std::size_t(2));
    check_false("removed key is absent", cache.get(2, out));
    check_true("remove keeps key 1", cache.get(1, out));
    check_true("remove keeps key 3", cache.get(3, out));

    cache.remove(42);
    check_eq("removing a missing key is a no-op", cache.size(), std::size_t(2));

    cache.put(4, 400);
    cache.put(5, 500);
    check_eq("size() remains at capacity after refill", cache.size(), std::size_t(3));
    check_false("oldest remaining key is evicted", cache.get(1, out));
    check_true("new key is inserted after refill", cache.get(5, out));

    std::cout << "\nAll Shared cache tests done.\n";
    return 0;
}
