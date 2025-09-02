#pragma once

#include <vector>
#include <cstddef>
#include "Base.hpp"
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <shared_mutex>
#include "strategy/LRU.hpp"
#include "helpers/MutexLocks.hpp"
#include "concepts/CacheConcepts.hpp"
#include "interfaces/IStrategyCache.hpp"

namespace cache {

    template<
                typename K, typename V,
                typename Strategy = strategy::LRU<K, V>,
                typename Hash = std::hash<K>,
                typename Eq = std::equal_to<K>,
                typename Mutex = std::shared_mutex,
                typename InnerMutex = std::shared_mutex
    >

    requires    concepts::StrategyLike<Strategy, K, V> &&
                concepts::MutexLike<Mutex> &&
                concepts::MutexLike<InnerMutex>

    class Fragmented final: public IStrategyCache<K, V> {
        public:
            using IsFragmentedCache = void;

            explicit Fragmented(std::size_t fragments = 4, std::size_t cap = 128):
                _nfragments(fragments), _capacity(cap),
                _capacity_per_fragment(std::max<std::size_t>(1, cap / std::max<std::size_t>(1, fragments)))
            {
                if (_nfragments == 0) {
                    throw std::invalid_argument("Cannot set 0 fragments");
                }
                if (_capacity == 0) {
                    throw std::invalid_argument("Cannot set a null capacity");
                }
                _caches.reserve(_nfragments);

                for (std::size_t i = 0; i < _nfragments; ++i) {
                    _caches.emplace_back(nullptr);
                }
            }

            virtual ~Fragmented() noexcept override = default;

            [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
            {
                auto idx = getCacheIndex(key);
                std::unique_ptr<Fragment>& slot = getFragmentSlot(idx);
                Fragment *local = nullptr;

                {
                    mutex_locks::ReadLock<decltype(_mtx)> rlock(_mtx);
                    if (!slot) {
                        return false;
                    }
                    local = slot.get();
                }
                return local->get(key, cacheOut);
            }

            virtual void put(const K& key, const V& value) override
            {
                Fragment *fragmentPtr = nullptr;
                {
                    auto idx = getCacheIndex(key);
                    mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                    auto& slot = _caches[idx];
                    if (!slot) {
                        slot = std::make_unique<Fragment>(_capacity_per_fragment);
                    }
                    fragmentPtr = slot.get();
                }
                fragmentPtr->put(key, value);
            }

            virtual void clear() noexcept override
            {
                mutex_locks::WriteLock<decltype(_mtx)> wlock(_mtx);
                for (auto& cache : _caches) {
                    if (cache) {
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
                    for (auto& up : _caches) {
                        if (up) {
                            fragments.push_back(up.get());
                        }
                    }
                }
                std::size_t res = 0;
                for (auto *f : fragments) {
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
                if constexpr (std::is_same_v<Mutex, mutex_locks::NoLock>) {
                    return false;
                }
                return true;
            }

        private:
            using Fragment = Base<K, V, Strategy, Hash, Eq, InnerMutex>;

            mutable Mutex _mtx;
            const std::size_t _nfragments;
            const std::size_t _capacity;
            const std::size_t _capacity_per_fragment;
            std::vector<std::unique_ptr<Fragment>> _caches;

            std::size_t getCacheIndex(const K& key) const noexcept
            {
                return std::hash<K>{}(key) % _nfragments;
            }

            std::unique_ptr<Fragment>& getFragmentSlot(std::size_t idx) const
            {
                return const_cast<std::unique_ptr<Fragment>&>(_caches[idx]);
            }
    };
}
