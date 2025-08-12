#pragma once

#include <mutex>
#include <string>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <shared_mutex>
#include <unordered_map>
#include "StrategyCache.hpp"
#include "LRUCacheStrategy.hpp"
#include "../utils/Singleton.hpp"

struct CacheKey {
    std::string cls;
    std::string method;
    std::type_index ktype;
    std::type_index vtype;

    bool operator==(const CacheKey& o) const {
        return cls==o.cls && method==o.method && ktype==o.ktype && vtype==o.vtype;
    }
};

struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
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
    class MethodCacheManager : public utils::Singleton<MethodCacheManager<RegMutex>> {
        friend class utils::Singleton<MethodCacheManager<RegMutex>>;

    public:
        template<typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>, typename Mutex = std::mutex>
        StrategyCache<K, V, Hash, Eq, Mutex>& getMethodCache( const std::string& className,
            const std::string& methodName, std::size_t capacity = 128,
            std::unique_ptr<ICacheStrategy<K, V>> strategy = nullptr)
        {
            CacheKey key{className, methodName, typeid(K), typeid(V)};

            {
                std::shared_lock<std::shared_mutex> rlock(_mtx);
                auto it = _caches.find(key);
                if (it != _caches.end()) {
                    return *static_cast<StrategyCache<K,V,Hash,Eq,Mutex>*>(it->second.get());
                }
            }

            std::unique_lock<std::shared_mutex> wlock(_mtx);
            auto it = _caches.find(key);
            if (it == _caches.end()) {
                if (!strategy) {
                    strategy = std::make_unique<LRUCacheStrategy<K,V>>();
                }
                auto up = std::make_unique<StrategyCache<K,V,Hash,Eq,Mutex>>(std::move(strategy), capacity);
                auto *raw = up.get();
                _caches.emplace(
                    std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(
                        std::shared_ptr<void>(
                            up.release(), [](void* p){
                                delete static_cast<StrategyCache<K,V,Hash,Eq,Mutex>*>(p);
                            }
                        )
                    )
                );
                return *raw;
            }
            return *static_cast<StrategyCache<K,V,Hash,Eq,Mutex>*>(it->second.get());
        }

        private:
            explicit MethodCacheManager() = default;
            ~MethodCacheManager() noexcept = default;

            mutable RegMutex _mtx;
            std::unordered_map<CacheKey, std::shared_ptr<void>, CacheKeyHash> _caches;
    };
}
