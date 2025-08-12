#pragma once

#include <string>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <shared_mutex>
#include <unordered_map>
#include "StrategyCache.hpp"
#include "IStrategyCache.hpp"
#include "LRUCacheStrategy.hpp"
#include "../utils/Concepts.hpp"
#include "../utils/Singleton.hpp"

struct CacheKey {
    std::string cls;
    std::string method;
    std::type_index ktype;
    std::type_index vtype;

    [[nodiscard]] bool operator==(const CacheKey& o) const {
        return cls==o.cls && method==o.method && ktype==o.ktype && vtype==o.vtype;
    }
};

struct CacheKeyHash {
    [[nodiscard]] size_t operator()(const CacheKey& k) const {
        size_t h = 0;
        auto mix=[&](size_t x){ h ^= x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >>2 ); };
        mix(std::hash<std::string>{}(k.cls));
        mix(std::hash<std::string>{}(k.method));
        mix(k.ktype.hash_code());
        mix(k.vtype.hash_code());
        return h;
    }
};

namespace data {
    template<typename RegMutex = std::shared_mutex>
    requires concepts::MutexLike<RegMutex>
    class MethodCacheManager : public utils::Singleton<MethodCacheManager<RegMutex>> {
        friend class utils::Singleton<MethodCacheManager<RegMutex>>;

    public:
        template<typename K, typename V, typename Strategy = LRUCacheStrategy<K,V>, typename Hash = std::hash<K>,
            typename Eq = std::equal_to<K>, typename CacheMutex = std::shared_mutex,
            typename CacheType = StrategyCache<K, V, Strategy, Hash, Eq, CacheMutex>>
        requires concepts::StrategyLike<Strategy, K, V> && concepts::MutexLike<CacheMutex> && concepts::CacheLike<CacheType, K, V>
        [[nodiscard]] IStrategyCache<K, V>& getMethodCache(const std::string& className, const std::string& methodName,
            std::size_t capacity = 128, std::size_t fragments = 1)
        {
            CacheKey key{className, methodName, typeid(K), typeid(V)};

            {
                concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                if (auto it = _caches.find(key); it != _caches.end()) {
                    return *static_cast<IStrategyCache<K, V>*>(it->second.get());
                }
            }

            concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
            if (auto it = _caches.find(key); it != _caches.end()) {
                return *static_cast<IStrategyCache<K, V>*>(it->second.get());
            }

            using CacheT = CacheType;

            auto sp = allocateCache<CacheT, K, V>(fragments, capacity);
            CacheT* raw = sp.get();
            _caches.emplace(key, std::shared_ptr<void>(std::move(sp)));
            return *raw;
        }

        private:
            explicit MethodCacheManager() = default;
            ~MethodCacheManager() noexcept = default;

            template<typename CacheT>
            struct NoDelete {
                void operator()(CacheT*) const noexcept {}
            };

            template<typename CacheT, typename K, typename V>
            std::shared_ptr<CacheT> allocateCache(std::size_t fragments, std::size_t capacity) {
                if constexpr (concepts::SharedCacheLike<CacheT, K, V>) {
                    auto* p = std::addressof(CacheT::getInstance());
                    return std::shared_ptr<CacheT>(p, NoDelete<CacheT>{});
                } else if constexpr (concepts::FragmentedCacheLike<CacheT, K, V>) {
                    return std::make_shared<CacheT>(fragments, capacity);
                } else {
                    return std::make_shared<CacheT>(capacity);
                }
            }

            mutable RegMutex _mtx;
            std::unordered_map<CacheKey, std::shared_ptr<void>, CacheKeyHash> _caches;
    };
}
