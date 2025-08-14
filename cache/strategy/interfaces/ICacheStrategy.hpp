#pragma once

#include <optional>
#include "../../utils/NonCopyable.hpp"

namespace cache::strategy {
    template<typename K, typename V>
    class ICacheStrategy: public utils::NonCopyable {
        public:
            virtual ~ICacheStrategy() noexcept = default;

            virtual void onClear() noexcept = 0;
            [[nodiscard]] virtual bool onAccess(const K& key) = 0;
            [[nodiscard]] virtual bool onInsert(const K& key) = 0;
            [[nodiscard]] virtual bool onRemove(const K& key) = 0;
            virtual void reserve(std::size_t cap) = 0;
            [[nodiscard]] virtual std::optional<K> selectForEviction() = 0;

        protected:
            constexpr explicit ICacheStrategy() = default;
    };
}
