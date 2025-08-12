#pragma once

namespace data {
    struct NoLock {
        void lock() noexcept {}
        void unlock() noexcept {}
        bool try_lock() noexcept { return true; }

        void lock_shared() noexcept {}
        void unlock_shared() noexcept {}
        bool try_lock_shared() noexcept { return true; }
    };
}
