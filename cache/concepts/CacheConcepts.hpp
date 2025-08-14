#pragma once

#include "../interfaces/IStrategyCache.hpp"
#include "../strategy/interfaces/ICacheStrategy.hpp"

namespace cache::concepts {
    template<typename S, typename K, typename V>
    concept StrategyLike = std::is_base_of_v<cache::strategy::ICacheStrategy<typename S::KeyType, typename S::ValType>, S>;

    template<typename C, typename K, typename V>
    concept CacheLike = std::is_base_of_v<cache::IStrategyCache<typename C::KeyType, typename C::ValType>, C>;

    template<typename C, typename K, typename V>
    concept FragmentedCacheLike = CacheLike<C, K, V> && requires {
        typename C::IsFragmentedCache;
    };

    template<typename C, typename K, typename V>
    concept SharedCacheLike = CacheLike<C, K, V> && requires {
        typename C::IsSharedCache;
    };
}
