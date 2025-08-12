#pragma once

#include "../cache/ICacheStrategy.hpp"

namespace concepts {
    template<typename M>
    concept MutexLike = requires(M& m) {
        { m.lock() } -> std::same_as<void>;
        { m.unlock() } -> std::same_as<void>;
        { m.try_lock() } -> std::convertible_to<bool>;
    };

    template<typename S, typename K, typename V>
    concept StrategyLike = std::is_base_of_v<data::ICacheStrategy<typename S::KeyType, typename S::ValType>, S>;

    template<typename M>
    concept SharedMutexLike = requires(M& m) {
        { m.lock_shared() } -> std::same_as<void>;
        { m.unlock_shared() } -> std::same_as<void>;
        { m.try_lock_shared() } -> std::convertible_to<bool>;
    };

    template<typename M>
    using ReadLock  = std::conditional_t<SharedMutexLike<M>, std::shared_lock<M>, std::unique_lock<M>>;

    template<typename M>
    using WriteLock = std::unique_lock<M>;
}
