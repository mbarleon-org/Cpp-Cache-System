#pragma once

#include <string>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <shared_mutex>
#include <unordered_map>
#include "StrategyCache.hpp"
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
        template<typename K, typename V, typename Strategy = LRUCacheStrategy<K,V>,
            typename Hash = std::hash<K>, typename Eq = std::equal_to<K>, typename CacheMutex = std::shared_mutex>
        requires concepts::StrategyLike<Strategy, K, V> && concepts::MutexLike<CacheMutex>
        [[nodiscard]] StrategyCache<K, V, Strategy, Hash, Eq, CacheMutex>& getMethodCache(
            const std::string& className, const std::string& methodName, std::size_t capacity = 128)
        {
            CacheKey key{className, methodName, typeid(K), typeid(V)};

            {
                concepts::ReadLock<decltype(_mtx)> rlock(_mtx);
                auto it = _caches.find(key);
                if (it != _caches.end()) {
                    return *static_cast<StrategyCache<K, V, Strategy, Hash, Eq, CacheMutex>*>(it->second.get());
                }
            }

            concepts::WriteLock<decltype(_mtx)> wlock(_mtx);
            auto it = _caches.find(key);
            if (it == _caches.end()) {
                auto up = std::make_unique<StrategyCache<K, V, Strategy, Hash, Eq, CacheMutex>>(capacity);
                auto *raw = up.get();
                _caches.emplace(
                    std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(
                        std::shared_ptr<void>(
                            up.release(), [](void* p){
                                delete static_cast<StrategyCache<K, V, Strategy, Hash, Eq, CacheMutex>*>(p);
                            }
                        )
                    )
                );
                return *raw;
            }
            return *static_cast<StrategyCache<K, V, Strategy, Hash, Eq, CacheMutex>*>(it->second.get());
        }

        private:
            explicit MethodCacheManager() = default;
            virtual ~MethodCacheManager() noexcept override = default;

            mutable RegMutex _mtx;
            std::unordered_map<CacheKey, std::shared_ptr<void>, CacheKeyHash> _caches;
    };
}
