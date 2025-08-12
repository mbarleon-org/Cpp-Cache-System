#pragma once

#include <cstddef>

namespace data {
    template<typename K, typename V>
    class IStrategyCache {
        public:
            ~IStrategyCache() noexcept = default;

            virtual bool get(const K& key, V& cacheOut) = 0;
            virtual void put(const K& key, const V& value) = 0;
            virtual void clear() noexcept = 0;
            virtual std::size_t size() const noexcept = 0;
            virtual std::size_t capacity() const noexcept = 0;

        protected:
            constexpr explicit IStrategyCache() = default;
    };
}
