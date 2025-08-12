#pragma once

#include <memory>
#include <stdexcept>
#include <functional>
#include <shared_mutex>
#include "StrategyCache.hpp"
#include "../utils/Singleton.hpp"

namespace data {
    template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>, typename Mutex = std::mutex>
    class SharedStrategyCache: public IStrategyCache<K, V>, public utils::Singleton<SharedStrategyCache<K, V, Hash, Eq, Mutex>> {
        friend class utils::Singleton<SharedStrategyCache<K, V, Hash, Eq, Mutex>>;
        public:
            ~SharedStrategyCache() noexcept = default;

            bool isCacheInitialized() const noexcept
            {
                return static_cast<bool>(_cache);
            }

            void initialize(CacheStrategyType<K, V> strategy, std::size_t cap = 128)
            {
                if (!_cache) {
                    _cache = std::make_unique<StrategyCache<K, V, Hash, Eq, Mutex>>(std::move(strategy), cap);
                }
            }

            virtual bool get(const K& key, V& cacheOut) override
            {
                if (_cache) {
                    return _cache->get(key, cacheOut);
                }
                return false;
            }

            virtual void put(const K& key, const V& value) override
            {
                if (_cache) {
                    _cache->put(key, value);
                }
            }

            virtual void clear() noexcept override
            {
                if (_cache) {
                    _cache->clear();
                }
            }

            virtual std::size_t size() const noexcept override
            {
                return _cache ? _cache->size() : 0;
            }

            virtual std::size_t capacity() const noexcept override
            {
                return _cache ? _cache->capacity() : 0;
            }

        private:
            SharedStrategyCache() = default;

            mutable Mutex _mtx;
            std::unique_ptr<StrategyCache<K, V, Hash, Eq, Mutex>> _cache;
    };
}
