#pragma once

#include <Cache/Interfaces/IStrategyCache.hpp>

namespace cache
{
    template <typename K, typename V>
    class AStrategyCache : public IStrategyCache<K, V>
    {
      public:
        using KeyType = K;
        using ValType = V;

        virtual ~AStrategyCache() noexcept = default;

        [[nodiscard]] virtual bool        get(const K& key, V& cacheOut)                                  = 0;
        virtual void                      put(const K& key, const V& value)                               = 0;
        virtual void                      remove(const K& key)                                            = 0;
        virtual void                      invalidateIf(std::function<bool(const K&, const V&)> predicate) = 0;
        [[nodiscard]] virtual bool        hasInvalidationPredicate() const noexcept                       = 0;
        virtual void                      clearInvalidationPredicate()                                    = 0;
        virtual void                      clear() noexcept                                                = 0;
        [[nodiscard]] virtual std::size_t size() const noexcept                                           = 0;
        [[nodiscard]] virtual std::size_t capacity() const noexcept                                       = 0;
        [[nodiscard]] virtual bool        isMtSafe() const noexcept                                       = 0;

        [[nodiscard]] bool contains(const K& key, bool countAsAccess = false) final override
        {
            return checkContains(key, countAsAccess);
        }

        [[nodiscard]] bool putIfAbsent(const K& key, const V& value) final override
        {
            return putConditional(key, value, PutRequirement::ABSENT);
        }

        [[nodiscard]] bool putIfPresent(const K& key, const V& value) final override
        {
            return putConditional(key, value, PutRequirement::PRESENT);
        }

        [[nodiscard]] bool putIf(const K& key, const V& value, std::function<bool(const K&, const V&)> f) final override
        {
            if (!f(key, value))
            {
                return false;
            }
            put(key, value);
            return true;
        }

      protected:
        constexpr explicit AStrategyCache() = default;

        enum class PutRequirement
        {
            ABSENT,
            PRESENT
        };

        [[nodiscard]] virtual bool putConditional(const K& key, const V& value, PutRequirement req) = 0;
        [[nodiscard]] virtual bool checkContains(const K& key, bool countAsAccess)                  = 0;

      private:
        AStrategyCache(const AStrategyCache&)            = delete;
        AStrategyCache& operator=(const AStrategyCache&) = delete;
    };
} // namespace cache
