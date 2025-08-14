#pragma once

#include <memory>
#include <stdexcept>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include "strategy/LRU.hpp"
#include "interfaces/IStrategyCache.hpp"
#include "../utils/Concepts.hpp"
#include "../utils/MutexLocks.hpp"
#include "strategy/interfaces/ICacheStrategy.hpp"

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

    class Base final: public IStrategyCache<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            explicit Base(std::size_t cap = 128): _capacity(cap)
            {
                if (cap < 1) {
                    throw(std::invalid_argument("Cannot give null capacity."));
                }
                _strategy = std::make_unique<Strategy>();
                _map.reserve(_capacity);
                _strategy->reserve(_capacity);
            }

            virtual ~Base() noexcept override = default;

            [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
            {
                {
                    MutexLocks::ReadLock<decltype(_mtx)> rlock(_mtx);
                    auto it = _map.find(key);
                    if (it == _map.end()) {
                        return false;
                    }
                }
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                auto it = _map.find(key);
                if (!_strategy->onAccess(key)) {
                    clear();
                    return false;
                }
                cacheOut = it->second;
                return true;
            }

            virtual void put(const K& key, const V& value) override
            {
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                auto it = _map.find(key);
                if (it != _map.end()) {
                    it->second = value;
                    if (!_strategy->onAccess(key)) {
                        clear();
                    }
                    return;
                }
                if (_map.size() >= _capacity) {
                    auto evictKey = _strategy->selectForEviction();
                    if (evictKey) {
                        _map.erase(*evictKey);
                        if (!_strategy->onRemove(*evictKey)) {
                            clear();
                        }
                    }
                }
                if (_map.size() < _capacity) {
                    _map[key] = value;
                    if (!_strategy->onInsert(key)) {
                        clear();
                    }
                }
            }

            virtual void clear() noexcept override
            {
                MutexLocks::WriteLock<decltype(_mtx)> wlock(_mtx);
                _map.clear();
                _strategy->onClear();
            }

            [[nodiscard]] virtual std::size_t size() const noexcept override
            {
                MutexLocks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _map.size();
            }

            [[nodiscard]] virtual std::size_t capacity() const noexcept override
            {
                return _capacity;
            }

            [[nodiscard]] virtual bool isMtSafe() const noexcept override
            {
                if constexpr (std::is_same_v<Mutex, MutexLocks::NoLock>) {
                    return false;
                }
                return true;
            }

        private:
            using MapType = std::unordered_map<K, V, Hash, Eq>;

            MapType _map;
            mutable Mutex _mtx;
            std::size_t _capacity;
            std::unique_ptr<Strategy> _strategy;
    };
}
