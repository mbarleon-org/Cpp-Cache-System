#pragma once

#include <Cache/Base.hpp>
#include <Cache/Concepts/CacheConcepts.hpp>
#include <Cache/Helpers/MutexLocks.hpp>
#include <Cache/Interfaces/AStrategyCache.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <Cache/Utils/Singleton.hpp>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>

namespace cache
{

    template <typename K, typename V, typename Strategy = strategy::LRU<K, V>, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>,
              typename Mutex = std::shared_mutex>

        requires concepts::StrategyLike<Strategy, K, V> && concepts::MutexLike<Mutex>

    class Shared final : public AStrategyCache<K, V>, public utils::Singleton<Shared<K, V, Strategy, Hash, Eq, Mutex>>
    {
        friend class utils::Singleton<Shared<K, V, Strategy, Hash, Eq, Mutex>>;

      public:
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
            if (!_cache)
            {
                _cache = std::make_unique<Base<K, V, Strategy, Hash, Eq, mutex_locks::NoLock>>(cap);
                if (_invalidateCallback)
                {
                    _cache->invalidateIf(std::move(_invalidateCallback));
                }
            }
        }

        [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (_cache)
            {
                return _cache->get(key, cacheOut);
            }
            return false;
        }

        virtual void put(const K& key, const V& value) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (_cache)
            {
                _cache->put(key, value);
            }
        }

        virtual void remove(const K& key) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (_cache)
            {
                _cache->remove(key);
            }
        }

        virtual void invalidateIf(std::function<bool(const K&, const V&)> predicate) override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (_cache)
            {
                _cache->invalidateIf(std::move(predicate));
            }
            else
            {
                _invalidateCallback = std::move(predicate);
            }
        }

        [[nodiscard]] virtual bool hasInvalidationPredicate() const noexcept override
        {
            mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
            return _cache ? _cache->hasInvalidationPredicate() : static_cast<bool>(_invalidateCallback);
        }

        virtual void clearInvalidationPredicate() override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (_cache)
            {
                _cache->clearInvalidationPredicate();
            }
            _invalidateCallback = nullptr;
        }

        virtual void clear() noexcept override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (_cache)
            {
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
            if (!_cache)
            {
                return false;
            }
            if (req == PutRequirement::ABSENT)
            {
                return _cache->putIfAbsent(key, value);
            }
            return _cache->putIfPresent(key, value);
        }

        [[nodiscard]] bool checkContains(const K& key, bool countAsAccess) override
        {
            if (!countAsAccess)
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                return _cache && _cache->contains(key);
            }

            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            return _cache && _cache->contains(key, true);
        }

      private:
        explicit Shared() = default;

        mutable Mutex                                                        _mtx;
        std::unique_ptr<Base<K, V, Strategy, Hash, Eq, mutex_locks::NoLock>> _cache;
        std::function<bool(const K&, const V&)>                              _invalidateCallback = nullptr;
    };
} // namespace cache
