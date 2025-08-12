#pragma once

#include <vector>
#include <cstddef>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <shared_mutex>
#include "StrategyCache.hpp"
#include "IStrategyCache.hpp"
#include "../utils/NoLock.hpp"
#include "LRUCacheStrategy.hpp"
#include "../utils/Concepts.hpp"

namespace data {

    template<
                typename K, typename V,
                typename Strategy = LRUCacheStrategy<K, V>,
                typename Hash = std::hash<K>,
                typename Eq = std::equal_to<K>,
                typename Mutex = std::shared_mutex,
                typename InnerMutex = std::shared_mutex
    >

    requires    concepts::StrategyLike<Strategy, K, V> &&
                concepts::MutexLike<Mutex> &&
                concepts::MutexLike<InnerMutex>

    class FragmentedStrategyCache final: public IStrategyCache<K, V> {
        public:
            using KeyType = K;
            using ValType = V;
            using IsFragmentedCache = void;

            explicit FragmentedStrategyCache(std::size_t fragments = 4, std::size_t cap = 128):
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

            virtual ~FragmentedStrategyCache() noexcept override = default;

            [[nodiscard]] virtual bool get(const K& key, V& cacheOut) override
            {
                const auto idx = getCacheIndex(key);
                std::unique_ptr<Fragment>& slot = getFragmentSlot(idx);
                Fragment *local = nullptr;

                {
                    concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                    if (!slot) {
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
                    const auto idx = getCacheIndex(key);
                    concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
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
                concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
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
                    concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
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
                if constexpr (std::is_same_v<Mutex, NoLock>) {
                    return false;
                }
                return true;
            }

        private:
            using Fragment = StrategyCache<K, V, Strategy, Hash, Eq, InnerMutex>;

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
