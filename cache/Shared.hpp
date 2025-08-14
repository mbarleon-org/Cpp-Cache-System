#pragma once

#include <memory>
#include "Base.hpp"
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <shared_mutex>
#include "strategy/LRU.hpp"
#include "utils/Singleton.hpp"
#include "helpers/MutexLocks.hpp"
#include "concepts/CacheConcepts.hpp"

namespace cache {

    template<
                typename K, typename V,
                typename Strategy = strategy::LRU<K, V>,
                typename Hash = std::hash<K>,
                typename Eq = std::equal_to<K>,
                typename Mutex = std::shared_mutex
    >

    requires    concepts::StrategyLike<Strategy, K, V> &&
                concepts::MutexLike<Mutex>

    class Shared final:
        public IStrategyCache<K, V>,
        public utils::Singleton<Shared<K, V, Strategy, Hash, Eq, Mutex>> {
        friend class utils::Singleton<Shared<K, V, Strategy, Hash, Eq, Mutex>>;

        public:
            using KeyType = K;
            using ValType = V;
            using IsSharedCache = void;

            virtual ~Shared() noexcept override = default;

            [[nodiscard]] bool isCacheInitialized() const noexcept
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return static_cast<bool>(_cache);
            }

            void initialize(std::size_t cap = 128)
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (!_cache) {
                    _cache = std::make_unique<Base<K, V, Strategy, Hash, Eq, mutex_locks::NoLock>>(cap);
                }
            }

            [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    return _cache->get(key, cacheOut);
                }
                return false;
            }

            virtual void put(const K& key, const V& value) override
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    _cache->put(key, value);
                }
            }

            virtual void clear() noexcept override
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    _cache->clear();
                }
            }

            [[nodiscard]] virtual std::size_t size() const noexcept override
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->size() : 0;
            }

            [[nodiscard]] virtual std::size_t capacity() const noexcept override
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->capacity() : 0;
            }

            [[nodiscard]] virtual bool isMtSafe() const noexcept override
            {
                if constexpr (std::is_same_v<Mutex, mutex_locks::NoLock>) {
                    return false;
                }
                return true;
            }

        private:
            explicit Shared() = default;

            mutable Mutex _mtx;
            std::unique_ptr<StrategyCache<K, V, Strategy, Hash, Eq, mutex_locks::NoLock>> _cache;
    };
}
