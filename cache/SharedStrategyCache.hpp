#pragma once

#include <memory>
#include <stdexcept>
#include <functional>
#include <shared_mutex>
#include "StrategyCache.hpp"
#include "../utils/NoLock.hpp"
#include "LRUCacheStrategy.hpp"
#include "../utils/Concepts.hpp"
#include "../utils/Singleton.hpp"

namespace data {
    template<typename K, typename V, typename Strategy = LRUCacheStrategy<K, V>,
        typename Hash = std::hash<K>, typename Eq = std::equal_to<K>, typename Mutex = std::shared_mutex>
    requires concepts::StrategyLike<Strategy, K, V> && concepts::MutexLike<Mutex>
    class SharedStrategyCache: public IStrategyCache<K, V>, public utils::Singleton<SharedStrategyCache<K, V, Strategy, Hash, Eq, Mutex>> {
        friend class utils::Singleton<SharedStrategyCache<K, V, Strategy, Hash, Eq, Mutex>>;
        public:
            ~SharedStrategyCache() noexcept = default;

            bool isCacheInitialized() const noexcept
            {
                concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                return static_cast<bool>(_cache);
            }

            void initialize(std::size_t cap = 128)
            {
                concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (!_cache) {
                    _cache = std::make_unique<StrategyCache<K, V, Strategy, Hash, Eq, NoLock>>(cap);
                }
            }

            virtual bool get(const K& key, V& cacheOut) override
            {
                concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    return _cache->get(key, cacheOut);
                }
                return false;
            }

            virtual void put(const K& key, const V& value) override
            {
                concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    _cache->put(key, value);
                }
            }

            virtual void clear() noexcept override
            {
                concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
                if (_cache) {
                    _cache->clear();
                }
            }

            virtual std::size_t size() const noexcept override
            {
                concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->size() : 0;
            }

            virtual std::size_t capacity() const noexcept override
            {
                concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->capacity() : 0;
            }

            virtual bool isMtSafe() const noexcept override
            {
                concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache ? _cache->isMtSafe() : false;
            }

        private:
            explicit SharedStrategyCache() = default;

            mutable Mutex _mtx;
            std::unique_ptr<StrategyCache<K, V, Strategy, Hash, Eq, Mutex>> _cache;
    };
}
