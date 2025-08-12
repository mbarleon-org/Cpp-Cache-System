#include <chrono>
#include "Vector.hpp"

// int main(void)
// {
//     const std::size_t cacheSize = 3;
//     auto lruStrategy = std::make_unique<data::LRUCacheStrategy<int, std::string>>();
//     data::StrategyCache<int, std::string> cache(std::move(lruStrategy), cacheSize);

//     std::string value;
//     std::cout << "Empty cache get(1): " << (cache.get(1, value) ? "found" : "not found") << std::endl;
//     std::cout << "Cache size: " << cache.size() << "/" << cache.capacity() << std::endl;

//     cache.put(1, "one");
//     cache.put(2, "two");
//     cache.put(3, "three");

//     std::cout << "After inserting 3 items:" << std::endl;
//     std::cout << "Cache size: " << cache.size() << "/" << cache.capacity() << std::endl;

//     for (int i = 1; i <= 3; ++i) {
//         if (cache.get(i, value)) {
//             std::cout << "get(" << i << "): " << value << std::endl;
//         }
//     }
// }

int main() {
    Vector v1(1.0, 2.0, 3.0);
    Vector v2(4.0, 5.0, 6.0);
    Vector v3(1.0, 2.0, 3.0); // Same as v1

    std::cout << "=== Testing Vector Intercept Caching ===" << std::endl;

    // First call - cache miss
    auto start = std::chrono::high_resolution_clock::now();
    double result1 = v1.intercept(v2);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "First call result: " << result1
              << " (took " << duration1.count() << "ms)" << std::endl;

    // Second call with same parameters - cache hit
    start = std::chrono::high_resolution_clock::now();
    double result2 = v1.intercept(v2);
    end = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Second call result: " << result2
              << " (took " << duration2.count() << "ms)" << std::endl;

    // Third call with different vector but same values - cache hit
    start = std::chrono::high_resolution_clock::now();
    double result3 = v3.intercept(v2);
    end = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Third call (v3.intercept(v2)): " << result3
              << " (took " << duration3.count() << "ms)" << std::endl;

    std::cout << "\nAll vectors share the same cache!" << std::endl;
    return 0;
}
