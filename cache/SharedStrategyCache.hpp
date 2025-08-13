#pragma once

#include <memory>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <shared_mutex>
#include "StrategyCache.hpp"
#include "../utils/NoLock.hpp"
#include "LRUCacheStrategy.hpp"
#include "../utils/Concepts.hpp"
#include "../utils/Singleton.hpp"
#include "../utils/MutexLocks.hpp"

namespace data {

    template<
                typename K, typename V,
                typename Strategy = LRUCacheStrategy<K, V>,
                typename Hash = std::hash<K>,
                typename Eq = std::equal_to<K>,
                typename Mutex = std::shared_mutex
    >

    requires    concepts::StrategyLike<Strategy, K, V> &&
                concepts::MutexLike<Mutex>

    class SharedStrategyCache:
        public IStrategyCache<K, V>,
        public utils::Singleton<SharedStrategyCache<K, V, Strategy, Hash, Eq, Mutex>> {
        friend class utils::Singleton<SharedStrategyCache<K, V, Strategy, Hash, Eq, Mutex>>;

        public:
            using KeyType = K;
            using ValType = V;
            using IsSharedCache = void;

            virtual ~SharedStrategyCache() noexcept override = default;

            [[nodiscard]] bool isCacheInitialized() const noexcept
            {
                MutexLocks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return static_cast<bool>(_cache);
            }

            void initialize(std::size_t cap = 128)
            {
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (!_cache) {
                    _cache = std::make_unique<StrategyCache<K, V, Strategy, Hash, Eq, NoLock>>(cap);
                }
            }

            [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
            {
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    return _cache->get(key, cacheOut);
                }
                return false;
            }

            virtual void put(const K& key, const V& value) override
            {
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    _cache->put(key, value);
                }
            }

            virtual void clear() noexcept override
            {
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    _cache->clear();
                }
            }

            [[nodiscard]] virtual std::size_t size() const noexcept override
            {
                MutexLocks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->size() : 0;
            }

            [[nodiscard]] virtual std::size_t capacity() const noexcept override
            {
                MutexLocks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->capacity() : 0;
            }

            [[nodiscard]] virtual bool isMtSafe() const noexcept override
            {
                if constexpr (std::is_same_v<Mutex, NoLock>) {
                    return false;
                }
                return true;
            }

        private:
            explicit SharedStrategyCache() = default;

            mutable Mutex _mtx;
            std::unique_ptr<StrategyCache<K, V, Strategy, Hash, Eq, NoLock>> _cache;
    };
}
