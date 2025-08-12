#pragma once

#include <cstddef>
#include "ICacheStrategy.hpp"

namespace data {
    template<typename K, typename V>
    class ACacheStrategy: public ICacheStrategy<K, V> {
        public:
            virtual ~ACacheStrategy() noexcept override = default;

            virtual void reserve(std::size_t cap) final override {
                if (cap < 1) {
                    throw(std::invalid_argument("Cannot give null capacity."));
                }
                reserve_worker(cap);
            }

        protected:
            virtual void reserve_worker(std::size_t cap) = 0;

            constexpr explicit ACacheStrategy() = default;
    };
}
