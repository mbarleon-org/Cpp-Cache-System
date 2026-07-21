#pragma once

#include <Cache/Concepts/CacheConcepts.hpp>
#include <Cache/Helpers/MutexLocks.hpp>
#include <Cache/Interfaces/AStrategyCache.hpp>
#include <Cache/Strategy/Interfaces/ICacheStrategy.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>

namespace cache
{

    template <typename K, typename V, typename Strategy = strategy::LRU<K, V>, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>,
              typename Mutex = std::shared_mutex>

        requires concepts::StrategyLike<Strategy, K, V> && concepts::MutexLike<Mutex>

    class Base final : public AStrategyCache<K, V>
    {
      public:
        explicit Base(std::size_t cap = 128) : _capacity(cap)
        {
            if (cap < 1)
            {
                throw(std::invalid_argument("Cannot give null capacity."));
            }
            _strategy = std::make_unique<Strategy>();
            _map.reserve(_capacity);
            _strategy->reserve(_capacity);
        }

        virtual ~Base() noexcept override = default;

        [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            auto                                   it = _map.find(key);
            if (it == _map.end())
            {
                return false;
            }
            if (!_strategy->onAccess(key))
            {
                clearUnlocked();
                return false;
            }
            if (_invalidateCallback && _invalidateCallback(key, it->second))
            {
                _map.erase(it);
                (void) _strategy->onRemove(key);
                return false;
            }
            cacheOut = it->second;
            return true;
        }

        virtual void put(const K& key, const V& value) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            (void) putUnlocked(key, value);
        }

        virtual void remove(const K& key) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            _map.erase(key);
            if (!_strategy->onRemove(key))
            {
                clearUnlocked();
            }
        }

        virtual void invalidateIf(std::function<bool(const K&, const V&)> predicate) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            _invalidateCallback = std::move(predicate);
        }

        virtual void clearInvalidationPredicate() override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            _invalidateCallback = nullptr;
        }

        virtual void clear() noexcept override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            clearUnlocked();
        }

        [[nodiscard]] virtual std::size_t size() const noexcept override
        {
            mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
            return _map.size();
        }

        [[nodiscard]] virtual std::size_t capacity() const noexcept override
        {
            return _capacity;
        }

        [[nodiscard]] virtual bool isMtSafe() const noexcept override
        {
            if constexpr (std::is_same_v<Mutex, mutex_locks::NoLock>)
            {
                return false;
            }
            return true;
        }

      protected:
        using PutRequirement = typename AStrategyCache<K, V>::PutRequirement;

        [[nodiscard]] bool putConditional(const K& key, const V& value, PutRequirement req) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            auto                                   it      = _map.find(key);
            bool                                   present = it != _map.end();

            if (present && isInvalidatedUnlocked(key, it))
            {
                present = false;
            }

            if ((req == PutRequirement::ABSENT && present) || (req == PutRequirement::PRESENT && !present))
            {
                return false;
            }

            return putUnlocked(key, value);
        }

        [[nodiscard]] bool checkContains(const K& key, bool countAsAccess) override
        {
            if (!countAsAccess)
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _map.find(key) != _map.end();
            }

            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            auto                                   it = _map.find(key);
            if (it == _map.end())
            {
                return false;
            }
            if (!_strategy->onAccess(key))
            {
                clearUnlocked();
                return false;
            }
            return !isInvalidatedUnlocked(key, it);
        }

      private:
        using MapType     = std::unordered_map<K, V, Hash, Eq>;
        using MapIterator = typename MapType::iterator;

        bool putUnlocked(const K& key, const V& value)
        {
            auto it = _map.find(key);
            if (it != _map.end())
            {
                it->second = value;
                if (!_strategy->onAccess(key))
                {
                    clearUnlocked();
                    return false;
                }
                return true;
            }
            if (_map.size() >= _capacity)
            {
                auto evictKey = _strategy->selectForEviction();
                if (evictKey)
                {
                    _map.erase(*evictKey);
                    if (!_strategy->onRemove(*evictKey))
                    {
                        clearUnlocked();
                    }
                }
            }
            if (_map.size() < _capacity)
            {
                _map[key] = value;
                if (!_strategy->onInsert(key))
                {
                    clearUnlocked();
                    return false;
                }
                return true;
            }
            return false;
        }

        [[nodiscard]] bool isInvalidatedUnlocked(const K& key, MapIterator it)
        {
            if (!_invalidateCallback || !_invalidateCallback(key, it->second))
            {
                return false;
            }
            _map.erase(it);
            (void) _strategy->onRemove(key);
            return true;
        }

        void clearUnlocked() noexcept
        {
            _map.clear();
            _strategy->onClear();
        }

        MapType                                 _map;
        mutable Mutex                           _mtx;
        std::size_t                             _capacity;
        std::unique_ptr<Strategy>               _strategy;
        std::function<bool(const K&, const V&)> _invalidateCallback = nullptr;
    };
} // namespace cache
