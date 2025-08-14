#pragma once

#include <shared_mutex>
#include "../concepts/MutexConcepts.hpp"

namespace cache::mutex_locks {
    template<typename M>
    using ReadLock = std::conditional_t<concepts::SharedMutexLike<M>, std::shared_lock<M>, std::unique_lock<M>>;

    template<typename M>
    using WriteLock = std::unique_lock<M>;

    struct NoLock {
        void lock() noexcept {}
        void unlock() noexcept {}
        bool try_lock() noexcept { return true; }

        void lock_shared() noexcept {}
        void unlock_shared() noexcept {}
        bool try_lock_shared() noexcept { return true; }
    };
}
