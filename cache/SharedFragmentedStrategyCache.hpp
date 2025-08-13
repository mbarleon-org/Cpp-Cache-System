#pragma once

#include <memory>
#include <shared_mutex>
#include <type_traits>
#include "IStrategyCache.hpp"
#include "../utils/NoLock.hpp"
#include "LRUCacheStrategy.hpp"
#include "../utils/Concepts.hpp"
#include "../utils/Singleton.hpp"
#include "FragmentedStrategyCache.hpp"
#include "../utils/MutexLocks.hpp"

namespace data {

    template<
                typename K, typename V,
                typename Strategy = LRUCacheStrategy<K, V>,
                typename Hash = std::hash<K>,
                typename Eq = std::equal_to<K>,
                typename WrapperMutex = std::shared_mutex,
                typename FragMutex = std::shared_mutex,
                typename FragmentMutex = std::mutex
    >

    requires    concepts::StrategyLike<Strategy, K, V> &&
                concepts::MutexLike<WrapperMutex> &&
                concepts::MutexLike<FragMutex> &&
                concepts::MutexLike<FragmentMutex>

    class SharedFragmentedStrategyCache:
                public IStrategyCache<K, V>,
                public utils::Singleton<
                    SharedFragmentedStrategyCache<
                        K, V,
                        Strategy,
                        Hash,
                        Eq,
                        WrapperMutex,
                        FragMutex,
                        FragmentMutex
                    >
                >
    {

        using Fragmented = FragmentedStrategyCache<K, V, Strategy, Hash, Eq, FragMutex, FragmentMutex>;

        friend class utils::Singleton<SharedFragmentedStrategyCache<
            K, V, Strategy, Hash, Eq, WrapperMutex, FragMutex, FragmentMutex>
        >;

    public:
        using KeyType  = K;
        using ValType  = V;
        using IsSharedCache = void;
        using IsFragmentedCache = void;

        ~SharedFragmentedStrategyCache() noexcept override = default;

        void initialize(std::size_t fragments = 4, std::size_t cap = 128) {
            MutexLocks::WriteLock<decltype(_mtx)> w(_mtx);
            if (!_cache) {
                _cache = std::make_unique<Fragmented>(fragments, cap);
            }
        }

        [[nodiscard]] bool isCacheInitialized() const noexcept {
            MutexLocks::ReadLock<decltype(_mtx)> r(_mtx);
            return static_cast<bool>(_cache);
        }

        [[nodiscard]] bool get(const K& key, V& out) override {
            Fragmented* f = nullptr;
            {
                MutexLocks::ReadLock<decltype(_mtx)> r(_mtx);
                if (!_cache) {
                    return false;
                }
                f = _cache.get();
            }
            return f->get(key, out);
        }

        void put(const K& key, const V& val) override {
            Fragmented* f = nullptr;
            {
                MutexLocks::WriteLock<decltype(_mtx)> w(_mtx);
                if (!_cache) {
                    return;
                }
                f = _cache.get();
            }
            f->put(key, val);
        }

        void clear() noexcept override {
            Fragmented* f = nullptr;
            {
                MutexLocks::ReadLock<decltype(_mtx)> r(_mtx);
                f = _cache.get();
            }
            if (f) f->clear();
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            Fragmented* f = nullptr;
            {
                MutexLocks::ReadLock<decltype(_mtx)> r(_mtx);
                f = _cache.get();
            }
            return f ? f->size() : 0;
        }

        [[nodiscard]] std::size_t capacity() const noexcept override {
            Fragmented* f = nullptr;
            {
                MutexLocks::ReadLock<decltype(_mtx)> r(_mtx);
                f = _cache.get();
            }
            return f ? f->capacity() : 0;
        }

        [[nodiscard]] bool isMtSafe() const noexcept override {
            if constexpr (std::is_same_v<WrapperMutex, NoLock>) {
                return false;
            }
            return true;
        }

    private:
        explicit SharedFragmentedStrategyCache() = default;

        mutable WrapperMutex _mtx;
        std::unique_ptr<Fragmented> _cache;
    };

}
