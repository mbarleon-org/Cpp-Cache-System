#pragma once

#include <optional>
#include "../utils/NonCopyable.hpp"

namespace data {
    template<typename K, typename V>
    class ICacheStrategy: public utils::NonCopyable {
        public:
            virtual ~ICacheStrategy() noexcept = default;

            virtual void onClear() noexcept = 0;
            virtual void onAccess(const K& key) = 0;
            virtual void onInsert(const K& key) = 0;
            virtual void onRemove(const K& key) = 0;
            virtual void reserve(std::size_t cap) = 0;
            [[nodiscard]] virtual std::optional<K> selectForEviction() = 0;

        protected:
            constexpr explicit ICacheStrategy() = default;
    };
}
