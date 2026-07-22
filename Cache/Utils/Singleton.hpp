#pragma once

#include <Cache/Utils/NonCopyable.hpp>
#include <type_traits>

namespace utils
{
    template <class C>
    class Singleton : public NonCopyable
    {
      public:
        static C& getInstance() noexcept(std::is_nothrow_default_constructible_v<C>)
        {
            static C instance;
            return instance;
        }

      protected:
        constexpr explicit Singleton() noexcept = default;
        ~Singleton() noexcept                   = default;
    };
} // namespace utils
