#pragma once

#include "../cache/IStrategyCache.hpp"
#include "../cache/ICacheStrategy.hpp"

namespace concepts {
    template<typename S, typename K, typename V>
    concept StrategyLike = std::is_base_of_v<data::ICacheStrategy<typename S::KeyType, typename S::ValType>, S>;

    template<typename C, typename K, typename V>
    concept CacheLike = std::is_base_of_v<data::IStrategyCache<typename C::KeyType, typename C::ValType>, C>;

    template<typename C, typename K, typename V>
    concept FragmentedCacheLike = CacheLike<C, K, V> && requires {
        typename C::IsFragmentedCache;
    };

    template<typename C, typename K, typename V>
    concept SharedCacheLike = CacheLike<C, K, V> && requires {
        typename C::IsSharedCache;
    };

    template<typename M>
    concept MutexLike = requires(M& m) {
        { m.lock() } -> std::same_as<void>;
        { m.unlock() } -> std::same_as<void>;
        { m.try_lock() } -> std::convertible_to<bool>;
    };

    template<typename M>
    concept SharedMutexLike = requires(M& m) {
        { m.lock_shared() } -> std::same_as<void>;
        { m.unlock_shared() } -> std::same_as<void>;
        { m.try_lock_shared() } -> std::convertible_to<bool>;
    };
}
