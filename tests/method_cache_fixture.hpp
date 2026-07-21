#pragma once

// Fixture used by method_cache_test.cpp.

#include <Cache/MethodCacheKey.hpp>
#include <Cache/MethodManager.hpp>
#include <Cache/SharedFragmented.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <chrono>
#include <iostream>
#include <thread>

class Vector
{
  public:
    constexpr explicit Vector(double x, double y, double z) : x(x), y(y), z(z)
    { }

    double intercept(const Vector& other)
    {
        using KeyType = cache::MethodCacheKey<double, double, double, double, double, double>;
        auto& cache   = cache::MethodManager<>::getInstance().getMethodCache<KeyType, double, cache::strategy::LRU<KeyType, double>, std::hash<KeyType>, std::equal_to<KeyType>, std::shared_mutex, cache::SharedFragmented<KeyType, double, cache::strategy::LRU<KeyType, double>, std::hash<KeyType>, std::equal_to<KeyType>, std::shared_mutex>>("Vector", "intercept");

        KeyType key(x, y, z, other.x, other.y, other.z);
        double  result;

        if (cache.get(key, result))
        {
            std::cout << "Cache hit for Vector::intercept(" << x << "," << y << "," << z
                      << " with " << other.x << "," << other.y << "," << other.z << ")" << std::endl;
            return result;
        }

        std::cout << "Cache miss for Vector::intercept. Computing intercept..." << std::endl;

        result = computeIntercept(other);

        cache.put(key, result);
        return result;
    }

  private:
    double x;
    double y;
    double z;

    double computeIntercept(const Vector& other)
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000000000));
        return x * other.x + y * other.y + z * other.z;
    }
};
