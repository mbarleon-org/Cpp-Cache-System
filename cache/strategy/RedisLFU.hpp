#pragma once

#include <list>
#include <chrono>
#include <random>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "interfaces/ACacheStrategy.hpp"

namespace cache::strategy {
    template<typename K, typename V>
    class RedisLFU final : public ACacheStrategy<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            explicit RedisLFU(): _rng(std::random_device{}()){}
            virtual ~RedisLFU() noexcept override = default;

            virtual void onClear() noexcept override
            {
                _meta.clear();
                _pos.clear();
                _index.clear();
            }

            [[nodiscard]] virtual bool onInsert(const K& key) override
            {
                if (_pos.find(key) != _pos.end()) {
                    return false;
                }
                _index.push_front(key);
                _pos.emplace(key, _index.begin());
                _meta[key] = LFUMeta{0, currentMinutes()};
                return true;
            }

            [[nodiscard]] virtual bool onAccess(const K& key) override
            {
                auto pit = _pos.find(key);
                if (pit == _pos.end()) {
                    return false;
                }

                lfuDecayOnAccess(key);

                std::uint32_t rnd = _dist(_rng);
                lfuMaybeIncrement(key, rnd);

                _index.splice(_index.begin(), _index, pit->second);
                pit->second = _index.begin();
                return true;
            }

            [[nodiscard]] virtual bool onRemove(const K& key) override
            {
                if (auto it = _pos.find(key); it != _pos.end()) {
                    _index.erase(it->second);
                    _pos.erase(it);
                }
                _meta.erase(key);
                if (_pos.empty() != _meta.empty()) {
                    return false;
                }
                return true;
            }

            [[nodiscard]] virtual std::optional<K> selectForEviction() override
            {
                if (_index.empty()) {
                    return std::nullopt;
                }

                std::optional<Candidate> worst;
                auto it = _index.begin();

                for (std::size_t i = 0; i < _sampleSize && it != _index.end(); ++i) {
                    const K& key = *it;

                    lfuDecayOnAccess(key);

                    const auto& m = _meta[key];
                    Candidate c{ key, m.hits, m.ldt, it };

                    if (!worst || isWorse(c, *worst)) {
                        worst = c;
                    }

                    std::size_t jumps = 1 + (_dist(_rng) % 7);
                    while (jumps-- && it != _index.end()) {
                        ++it;
                    }
                }

                if (!worst) {
                    return std::nullopt;
                }
                return worst->key;
            }

        protected:
            virtual void reserve_worker(std::size_t cap) override
            {
                _meta.reserve(cap);
                _pos.reserve(cap);
            }

        private:
            struct LFUMeta {
                std::uint8_t hits = 0;
                std::uint16_t ldt  = 0;
            };

            struct Candidate {
                K key;
                std::uint8_t hits;
                std::uint16_t ldt;
                typename std::list<K>::iterator idxIt;
            };

            static bool isWorse(const Candidate& a, const Candidate& b)
            {
                if (a.hits != b.hits) {
                    return a.hits < b.hits;
                }
                return a.ldt < b.ldt;
            }

            static std::uint16_t currentMinutes()
            {
                return static_cast<std::uint16_t>(
                    std::chrono::time_point_cast<std::chrono::minutes>(
                        std::chrono::steady_clock::now()
                    ).time_since_epoch().count()
                );
            }

            void lfuDecayOnAccess(const K& key)
            {
                auto it = _meta.find(key);
                if (it == _meta.end()) {
                    return;
                }
                auto& m = it->second;

                std::uint16_t now = currentMinutes();
                std::uint16_t elapsed = static_cast<std::uint16_t>(now - m.ldt);
                if (elapsed == 0) {
                    return;
                }

                std::uint16_t decrements = (_lfuDecayTime == 0) ? 0 : (elapsed / _lfuDecayTime);
                if (decrements > 0) {
                    m.hits = (decrements >= m.hits) ? 0 : static_cast<std::uint8_t>(m.hits - decrements);
                    m.ldt  = now;
                }
            }

            void lfuMaybeIncrement(const K& key, std::uint32_t rnd32)
            {
                auto& m = _meta[key];
                if (m.hits == 255) {
                    return;
                }

                std::uint32_t denom = static_cast<std::uint32_t>(m.hits) * _lfuLogFactor + 1u;
                std::uint32_t r = (denom > 0) ? (rnd32 % denom) : 0u;
                if (r == 0) {
                    ++m.hits;
                }
            }

            static constexpr const std::size_t _sampleSize   = 5;
            static constexpr const std::uint8_t _lfuLogFactor = 10;
            static constexpr const std::uint16_t _lfuDecayTime = 1;

            std::unordered_map<K, LFUMeta> _meta;

            std::list<K> _index;
            std::unordered_map<K, typename std::list<K>::iterator> _pos;

            std::mt19937 _rng;
            std::uniform_int_distribution<std::uint32_t> _dist;
    };
}
