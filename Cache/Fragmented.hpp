#pragma once

#include <Cache/Base.hpp>
#include <Cache/Concepts/CacheConcepts.hpp>
#include <Cache/Helpers/MutexLocks.hpp>
#include <Cache/Interfaces/AStrategyCache.hpp>
#include <Cache/Strategy/LRU.hpp>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

namespace cache
{

    template <typename K, typename V, typename Strategy = strategy::LRU<K, V>, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>,
              typename Mutex = std::shared_mutex, typename InnerMutex = std::shared_mutex>

        requires concepts::StrategyLike<Strategy, K, V> && concepts::MutexLike<Mutex> && concepts::MutexLike<InnerMutex>

    class Fragmented final : public AStrategyCache<K, V>
    {
      public:
        using IsFragmentedCache = void;

        explicit Fragmented(std::size_t fragments = 4, std::size_t cap = 128)
            : _nfragments(fragments), _capacity(cap), _capacity_per_fragment(std::max<std::size_t>(1, cap / std::max<std::size_t>(1, fragments)))
        {
            if (_nfragments == 0)
            {
                throw std::invalid_argument("Cannot set 0 fragments");
            }
            if (_capacity == 0)
            {
                throw std::invalid_argument("Cannot set a null capacity");
            }
            _caches.reserve(_nfragments);

            for (std::size_t i = 0; i < _nfragments; ++i)
            {
                _caches.emplace_back(nullptr);
            }
        }

        virtual ~Fragmented() noexcept override = default;

        [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
        {
            auto                       idx   = getCacheIndex(key);
            std::unique_ptr<Fragment>& slot  = getFragmentSlot(idx);
            Fragment*                  local = nullptr;

            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                if (!slot)
                {
                    return false;
                }
                local = slot.get();
            }
            return local->get(key, cacheOut);
        }

        virtual void put(const K& key, const V& value) override
        {
            Fragment* fragmentPtr = nullptr;
            {
                auto                                   idx = getCacheIndex(key);
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                auto&                                  slot = _caches[idx];
                if (!slot)
                {
                    slot = std::make_unique<Fragment>(_capacity_per_fragment);
                    if (_invalidateCallback)
                    {
                        slot->invalidateIf(_invalidateCallback);
                    }
                }
                fragmentPtr = slot.get();
            }
            fragmentPtr->put(key, value);
        }

        virtual void remove(const K& key) override
        {
            auto                       idx   = getCacheIndex(key);
            std::unique_ptr<Fragment>& slot  = getFragmentSlot(idx);
            Fragment*                  local = nullptr;

            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                if (!slot)
                {
                    return;
                }
                local = slot.get();
            }
            local->remove(key);
        }

        virtual void invalidateIf(std::function<bool(const K&, const V&)> predicate) override
        {
            std::vector<Fragment*> fragments;
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                _invalidateCallback = predicate;
                fragments.reserve(_caches.size());
                for (auto& up : _caches)
                {
                    if (up)
                    {
                        fragments.push_back(up.get());
                    }
                }
            }
            for (auto* f : fragments)
            {
                f->invalidateIf(predicate);
            }
        }

        virtual void clearInvalidationPredicate() override
        {
            std::vector<Fragment*> fragments;
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                _invalidateCallback = nullptr;
                fragments.reserve(_caches.size());
                for (auto& up : _caches)
                {
                    if (up)
                    {
                        fragments.push_back(up.get());
                    }
                }
            }
            for (auto* f : fragments)
            {
                f->clearInvalidationPredicate();
            }
        }

        virtual void clear() noexcept override
        {
            mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
            for (auto& cache : _caches)
            {
                if (cache)
                {
                    cache->clear();
                }
            }
        }

        [[nodiscard]] virtual std::size_t size() const noexcept override
        {
            std::vector<Fragment*> fragments;
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                fragments.reserve(_caches.size());
                for (auto& up : _caches)
                {
                    if (up)
                    {
                        fragments.push_back(up.get());
                    }
                }
            }
            std::size_t res = 0;
            for (auto* f : fragments)
            {
                res += f->size();
            }
            return res;
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
        using Fragment       = Base<K, V, Strategy, Hash, Eq, InnerMutex>;

        [[nodiscard]] bool putConditional(const K& key, const V& value, PutRequirement req) override
        {
            Fragment*  fragment = nullptr;
            const auto idx      = getCacheIndex(key);

            if (req == PutRequirement::ABSENT)
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                auto&                                  slot = _caches[idx];
                if (!slot)
                {
                    slot = std::make_unique<Fragment>(_capacity_per_fragment);
                    if (_invalidateCallback)
                    {
                        slot->invalidateIf(_invalidateCallback);
                    }
                }
                fragment = slot.get();
            }
            else
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                const auto&                           slot = _caches[idx];
                if (!slot)
                {
                    return false;
                }
                fragment = slot.get();
            }

            if (req == PutRequirement::ABSENT)
            {
                return fragment->putIfAbsent(key, value);
            }
            return fragment->putIfPresent(key, value);
        }

        [[nodiscard]] bool checkContains(const K& key, bool countAsAccess) override
        {
            Fragment* fragment = nullptr;
            {
                mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                const auto&                           slot = _caches[getCacheIndex(key)];
                if (!slot)
                {
                    return false;
                }
                fragment = slot.get();
            }
            return fragment->contains(key, countAsAccess);
        }

      private:
        mutable Mutex                           _mtx;
        const std::size_t                       _nfragments;
        const std::size_t                       _capacity;
        const std::size_t                       _capacity_per_fragment;
        std::vector<std::unique_ptr<Fragment>>  _caches;
        std::function<bool(const K&, const V&)> _invalidateCallback = nullptr;

        std::size_t getCacheIndex(const K& key) const noexcept
        {
            return std::hash<K>{}(key) % _nfragments;
        }

        std::unique_ptr<Fragment>& getFragmentSlot(std::size_t idx) const
        {
            return const_cast<std::unique_ptr<Fragment>&>(_caches[idx]);
        }
    };
} // namespace cache
