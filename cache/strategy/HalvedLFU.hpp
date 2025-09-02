#pragma once

#include <list>
#include <utility>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "interfaces/ACacheStrategy.hpp"

namespace cache::strategy {
    template<typename K, typename V>
    class HalvedLFU final: public ACacheStrategy<K, V> {
        public:
            HalvedLFU() = default;
            virtual ~HalvedLFU() noexcept override = default;

            virtual void onClear() noexcept override
            {
                _minFreq = 0;
                _keyToBucket.clear();
                _buckets.clear();
                _opsSinceHalving = 0;
            }

            [[nodiscard]] virtual bool onAccess(const K& key) override
            {
                checkHalving();
                auto keyIt = _keyToBucket.find(key);
                if (keyIt == _keyToBucket.end()) {
                    return false;
                }

                std::size_t freq = keyIt->second.first;
                auto listIt = keyIt->second.second;

                auto oldBucketIt = _buckets.find(freq);
                if (oldBucketIt == _buckets.end()) {
                    return false;
                }

                oldBucketIt->second.erase(listIt);
                std::size_t newFreq = freq + 1;
                if (oldBucketIt->second.empty()) {
                    _buckets.erase(oldBucketIt);
                    if (_minFreq == freq) {
                        _minFreq = newFreq;
                    }
                }
                auto& list = _buckets[newFreq];
                list.push_front(key);
                keyIt->second = {newFreq, list.begin()};
                return true;
            }

            [[nodiscard]] virtual bool onInsert(const K& key) override
            {
                checkHalving();
                auto& list = _buckets[1];
                list.push_front(key);
                _keyToBucket.emplace(key, PosType{1, list.begin()});
                _minFreq = 1;
                return true;
            }

            [[nodiscard]] virtual bool onRemove(const K& key) override
            {
                checkHalving();
                auto keyIt = _keyToBucket.find(key);
                if (keyIt == _keyToBucket.end()) {
                    return false;
                }

                std::size_t freq = keyIt->second.first;

                auto oldBucketIt = _buckets.find(freq);
                if (oldBucketIt != _buckets.end()) {
                    oldBucketIt->second.erase(keyIt->second.second);
                    if (oldBucketIt->second.empty()) {
                        _buckets.erase(oldBucketIt);
                    }
                }
                _keyToBucket.erase(key);
                if (_keyToBucket.empty() || _buckets.empty()) {
                    onClear();
                }
                return true;
            }

            [[nodiscard]] virtual std::optional<K> selectForEviction() override
            {
                if (_buckets.empty() || _minFreq == 0) {
                    return std::nullopt;
                }

                checkHalving();
                auto bucketIt = _buckets.find(_minFreq);
                if (bucketIt == _buckets.end() || bucketIt->second.empty()) {
                    std::optional<std::size_t> best = std::nullopt;
                    for (const auto& kv : _buckets) {
                        if (!kv.second.empty() && (!best || kv.first < *best)) {
                            best = kv.first;
                        }
                    }
                    if (!best) {
                        return std::nullopt;
                    }
                    _minFreq = *best;
                    bucketIt = _buckets.find(_minFreq);
                }
                return bucketIt->second.back();
            }

        protected:
            virtual void reserve_worker(std::size_t cap) override
            {
                if (cap > _capacity) {
                    _capacity = cap;
                    _keyToBucket.reserve(_capacity);
                    _buckets.reserve(_capacity);
                }
            }

        private:
            void checkHalving()
            {
                if (++_opsSinceHalving < _halvingPeriod) {
                    return;
                }

                if (_keyToBucket.empty()) {
                    onClear();
                    return;
                }

                std::vector<Move> moves;
                moves.reserve(_keyToBucket.size());

                for (auto& kv : _keyToBucket) {
                    const K& key = kv.first;
                    std::size_t freq = kv.second.first;
                    std::size_t newFreq = std::max<std::size_t>(1, freq / 2);
                    if (newFreq != freq) {
                        moves.push_back({ key, freq, newFreq });
                    }
                }

                for (const auto& m : moves) {
                    auto itKey = _keyToBucket.find(m.key);
                    if (itKey == _keyToBucket.end()) {
                        continue;
                    }

                    auto itOldBucket = _buckets.find(m.oldFreq);
                    if (itOldBucket != _buckets.end()) {
                        itOldBucket->second.erase(itKey->second.second);
                        if (itOldBucket->second.empty()) {
                            _buckets.erase(itOldBucket);
                        }
                    }

                    auto& list = _buckets[m.newFreq];
                    list.push_front(m.key);
                    itKey->second = PosType{ m.newFreq, list.begin() };
                }

                std::optional<std::size_t> best = std::nullopt;
                for (const auto& kv : _buckets) {
                    if (!kv.second.empty() && (!best || kv.first < *best)) {
                        best = kv.first;
                    }
                }
                _minFreq = best ? *best : 0;

                _opsSinceHalving = 0;
            }

            using ListType = std::list<K>;
            using PosType = std::pair<std::size_t, typename ListType::iterator>;
            using MapType = std::unordered_map<K, PosType>;
            using BucketType = std::unordered_map<std::size_t, ListType>;

            struct Move {
                K key;
                std::size_t oldFreq;
                std::size_t newFreq;
            };

            std::size_t _capacity = 0;
            std::size_t _minFreq = 0;

            MapType _keyToBucket;
            BucketType _buckets;

            static constexpr const std::size_t _halvingPeriod = 4 * (1024);
            std::size_t _opsSinceHalving = 0;
    };
}
