#pragma once

#include "Concepts.hpp"

namespace MutexLocks {
    template<typename M>
    using ReadLock  = std::conditional_t<concepts::SharedMutexLike<M>, std::shared_lock<M>, std::unique_lock<M>>;

    template<typename M>
    using WriteLock = std::unique_lock<M>;
}
