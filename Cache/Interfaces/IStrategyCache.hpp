#pragma once

#include <cstddef>
#include <functional>

namespace cache
{
    template <typename K, typename V>
    class IStrategyCache
    {
      public:
        using KeyType = K;
        using ValType = V;

        virtual ~IStrategyCache() noexcept = default;

        [[nodiscard]] virtual bool        get(const K& key, V& cacheOut)                                                 = 0;
        [[nodiscard]] virtual bool        contains(const K& key, bool countAsAccess = false)                             = 0;
        [[nodiscard]] virtual bool        putIfAbsent(const K& key, const V& value)                                      = 0;
        [[nodiscard]] virtual bool        putIfPresent(const K& key, const V& value)                                     = 0;
        [[nodiscard]] virtual bool        putIf(const K& key, const V& value, std::function<bool(const K&, const V&)> f) = 0;
        virtual void                      put(const K& key, const V& value)                                              = 0;
        virtual void                      remove(const K& key)                                                           = 0;
        virtual void                      invalidateIf(std::function<bool(const K&, const V&)> predicate)                = 0;
        [[nodiscard]] virtual bool        hasInvalidationPredicate() const noexcept                                      = 0;
        virtual void                      clearInvalidationPredicate()                                                   = 0;
        virtual void                      clear() noexcept                                                               = 0;
        [[nodiscard]] virtual std::size_t size() const noexcept                                                          = 0;
        [[nodiscard]] virtual std::size_t capacity() const noexcept                                                      = 0;
        [[nodiscard]] virtual bool        isMtSafe() const noexcept                                                      = 0;

      protected:
        constexpr explicit IStrategyCache() = default;
    };
} // namespace cache
