#pragma once

#include <shared_mutex>

namespace cache::concepts {
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
