#pragma once

#include <mutex>
#include <memory>
#include <stdexcept>
#include <functional>
#include <unordered_map>
#include "ICacheStrategy.hpp"
#include "IStrategyCache.hpp"

namespace data {
    template<typename K, typename V>
    using CacheStrategyType = std::unique_ptr<ICacheStrategy<K, V>>;

    template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>, typename Mutex = std::mutex>
    class StrategyCache final: public IStrategyCache<K, V> {
        public:
            explicit StrategyCache(CacheStrategyType<K, V> strategy, std::size_t cap = 128):
                _strategy(std::move(strategy)), _capacity(cap)
            {
                if (cap < 1) {
                    throw(std::invalid_argument("Cannot give null capacity."));
                }
                if (_strategy.get() == nullptr) {
                    throw(std::invalid_argument("Cannot give empty strategy."));
                }
                _map.reserve(_capacity);
                _strategy->reserve(_capacity);
            }

            ~StrategyCache() noexcept = default;

            virtual bool get(const K& key, V& cacheOut) override
            {
                const auto it = _map.find(key);
                if (it == _map.end()) {
                    return false;
                }
                try {
                    _strategy->onAccess(key);
                } catch (const std::invalid_argument &e) {
                    clear();
                    return false;
                }
                cacheOut = it->second;
                return true;
            }

            virtual void put(const K& key, const V& value) override
            {
                const auto it = _map.find(key);
                if (it != _map.end()) {
                    it->second = value;
                    try {
                        _strategy->onAccess(key);
                    } catch (const std::invalid_argument &e) {
                        clear();
                    }
                    return;
                }
                if (_map.size() >= _capacity) {
                    const auto evictKey = _strategy->selectForEviction();
                    if (evictKey) {
                        _map.erase(*evictKey);
                        _strategy->onRemove(*evictKey);
                    }
                }
                if (_map.size() < _capacity) {
                    _map[key] = value;
                    _strategy->onInsert(key);
                }
            }

            virtual void clear() noexcept override
            {
                _map.clear();
                _strategy->onClear();
            }

            virtual std::size_t size() const noexcept override
            {
                return _map.size();
            }

            virtual std::size_t capacity() const noexcept override
            {
                return _capacity;
            }

        private:
            using MapType = std::unordered_map<K, V, Hash, Eq>;

            MapType _map;
            std::size_t _capacity;
            CacheStrategyType<K, V> _strategy;
    };
}
