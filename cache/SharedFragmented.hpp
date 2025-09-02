#pragma once

#include <memory>
#include <type_traits>
#include <shared_mutex>
#include "Fragmented.hpp"
#include "strategy/LRU.hpp"
#include "utils/Singleton.hpp"
#include "helpers/MutexLocks.hpp"
#include "concepts/CacheConcepts.hpp"
#include "interfaces/IStrategyCache.hpp"

namespace cache {

    template<
                typename K, typename V,
                typename Strategy = strategy::LRU<K, V>,
                typename Hash = std::hash<K>,
                typename Eq = std::equal_to<K>,
                typename WrapperMutex = std::shared_mutex,
                typename FragMutex = std::shared_mutex,
                typename FragmentMutex = std::mutex
    >

    requires    concepts::StrategyLike<Strategy, K, V> &&
                concepts::MutexLike<WrapperMutex> &&
                concepts::MutexLike<FragMutex> &&
                concepts::MutexLike<FragmentMutex>

    class SharedFragmented final:
                public IStrategyCache<K, V>,
                public utils::Singleton<
                    SharedFragmented<
                        K, V,
                        Strategy,
                        Hash,
                        Eq,
                        WrapperMutex,
                        FragMutex,
                        FragmentMutex
                    >
                >
    {

        using FragmentedType = Fragmented<K, V, Strategy, Hash, Eq, FragMutex, FragmentMutex>;

        friend class utils::Singleton<SharedFragmented<
            K, V, Strategy, Hash, Eq, WrapperMutex, FragMutex, FragmentMutex>
        >;

    public:
        using IsSharedCache = void;
        using IsFragmentedCache = void;

        ~SharedFragmented() noexcept override = default;

        void initialize(std::size_t fragments = 4, std::size_t cap = 128) {
            mutex_locks::WriteLock<decltype(_mtx)> w(_mtx);
            if (!_cache) {
                _cache = std::make_unique<FragmentedType>(fragments, cap);
            }
        }

        [[nodiscard]] bool isCacheInitialized() const noexcept {
            mutex_locks::ReadLock<decltype(_mtx)> r(_mtx);
            return static_cast<bool>(_cache);
        }

        [[nodiscard]] bool get(const K& key, V& out) override {
            FragmentedType *f = nullptr;
            {
                mutex_locks::ReadLock<decltype(_mtx)> r(_mtx);
                if (!_cache) {
                    return false;
                }
                f = _cache.get();
            }
            return f->get(key, out);
        }

        void put(const K& key, const V& val) override {
            FragmentedType *f = nullptr;
            {
                mutex_locks::WriteLock<decltype(_mtx)> w(_mtx);
                if (!_cache) {
                    return;
                }
                f = _cache.get();
            }
            f->put(key, val);
        }

        void clear() noexcept override {
            FragmentedType *f = nullptr;
            {
                mutex_locks::ReadLock<decltype(_mtx)> r(_mtx);
                f = _cache.get();
            }
            if (f) f->clear();
        }

        [[nodiscard]] std::size_t size() const noexcept override {
            FragmentedType *f = nullptr;
            {
                mutex_locks::ReadLock<decltype(_mtx)> r(_mtx);
                f = _cache.get();
            }
            return f ? f->size() : 0;
        }

        [[nodiscard]] std::size_t capacity() const noexcept override {
            FragmentedType *f = nullptr;
            {
                mutex_locks::ReadLock<decltype(_mtx)> r(_mtx);
                f = _cache.get();
            }
            return f ? f->capacity() : 0;
        }

        [[nodiscard]] bool isMtSafe() const noexcept override {
            if constexpr (std::is_same_v<WrapperMutex, mutex_locks::NoLock>) {
                return false;
            }
            return true;
        }

    private:
        explicit SharedFragmented() = default;

        mutable WrapperMutex _mtx;
        std::unique_ptr<FragmentedType> _cache;
    };

}
