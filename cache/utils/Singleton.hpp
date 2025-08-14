#pragma once

#include "NonCopyable.hpp"

namespace utils {
    template<class C>
    class Singleton: public NonCopyable {
        public:
            static C& getInstance() noexcept {
                static C instance;
                return instance;
            }

        protected:
            constexpr explicit Singleton() = default;
            ~Singleton() noexcept = default;
    };
}
