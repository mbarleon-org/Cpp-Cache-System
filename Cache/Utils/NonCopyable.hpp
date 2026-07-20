#pragma once

namespace utils {
    class NonCopyable {
        public:
            NonCopyable(const NonCopyable& other) = delete;
            NonCopyable& operator=(const NonCopyable& rhs) = delete;
            NonCopyable(NonCopyable&& other) = delete;
            NonCopyable& operator=(NonCopyable&& rhs) = delete;

        protected:
            constexpr explicit NonCopyable() = default;
            ~NonCopyable() noexcept = default;
    };
}
