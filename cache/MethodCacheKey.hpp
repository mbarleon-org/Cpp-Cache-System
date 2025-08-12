#pragma once

#include <tuple>
#include <functional>

namespace data {
    template<typename... Args>
    struct MethodCacheKey {
        std::tuple<Args...> args;

        MethodCacheKey(Args... arguments): args(std::forward<Args>(arguments)...) {}

        [[nodiscard]] bool operator==(const MethodCacheKey& other) const
        {
            return args == other.args;
        }
    };

    namespace detail {
        template<typename Tuple, std::size_t Index = std::tuple_size_v<Tuple>>
        struct tuple_hash_impl {
            [[nodiscard]] static std::size_t apply(const Tuple& tuple)
            {
                if constexpr (Index == 0) {
                    return 0;
                } else {
                    auto rest_hash = tuple_hash_impl<Tuple, Index - 1>::apply(tuple);
                    auto element_hash = std::hash<std::tuple_element_t<Index - 1, Tuple>>{}(std::get<Index - 1>(tuple));
                    return rest_hash ^ element_hash + 0x9e3779b97f4a7c15ull + (rest_hash << 6) + (rest_hash >> 2);
                }
            }
        };
    }
}

template<typename... Args>
struct std::hash<data::MethodCacheKey<Args...>> {
    [[nodiscard]] std::size_t operator()(const data::MethodCacheKey<Args...>& key) const
    {
        return data::detail::tuple_hash_impl<std::tuple<Args...>>::apply(key.args);
    }
};
