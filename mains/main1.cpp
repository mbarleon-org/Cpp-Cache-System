// main.cpp
#include <chrono>
#include <iostream>
#include <utility>

#include "Vector.hpp"

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

static std::pair<double, std::chrono::milliseconds>
timed_intercept(const char* label, Vector& lhs, Vector& rhs) {
    const auto start = std::chrono::high_resolution_clock::now();
    double result = lhs.intercept(rhs);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "[INFO] " << label << " duration=" << duration.count() << "ms\n";
    return {result, duration};
}

int main() {
    std::cout << "\n=== Vector::intercept cache semantics ===\n";

    Vector v1(1.0, 2.0, 3.0);
    Vector v2(4.0, 5.0, 6.0);
    Vector v3(1.0, 2.0, 3.0); // separate instance, same values as v1
    constexpr double expected = 32.0; // dot product of v1 · v2

    auto [result1, duration1] = timed_intercept("v1.intercept(v2) - first call", v1, v2);
    check_eq("first call result", result1, expected);

    auto [result2, duration2] = timed_intercept("v1.intercept(v2) - cached", v1, v2);
    check_eq("cached call returns same value", result2, expected);
    check_true("cached call faster than cold call", duration2 < duration1);

    auto [result3, duration3] = timed_intercept("v3.intercept(v2) - shared cache", v3, v2);
    check_eq("different instance hits shared cache", result3, expected);
    check_true("shared cache call faster than cold call", duration3 < duration1);

    std::cout << "\nAll Vector intercept cache checks done.\n";
    return 0;
}
