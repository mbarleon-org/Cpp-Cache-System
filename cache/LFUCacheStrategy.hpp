#pragma once

#include <list>
#include <utility>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "ACacheStrategy.hpp"

namespace data {
    template<typename K, typename V>
    class LFUCacheStrategy final: public ACacheStrategy<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            LFUCacheStrategy() = default;
            virtual ~LFUCacheStrategy() noexcept override = default;

            virtual void onClear() noexcept override
            {
                _minFreq = 0;
                _keyToBucket.clear();
                _buckets.clear();
            }

            virtual bool onAccess(const K& key) override
            {
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

            virtual void onInsert(const K& key) override
            {
                auto& list = _buckets[1];
                list.push_front(key);
                _keyToBucket.emplace(key, PosType{1, list.begin()});
                _minFreq = 1;
            }

            virtual void onRemove(const K& key) override
            {
                auto keyIt = _keyToBucket.find(key);
                if (keyIt == _keyToBucket.end()) {
                    return;
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
            }

            [[nodiscard]] virtual std::optional<K> selectForEviction() override
            {
                if (_buckets.empty() || _minFreq == 0) {
                    return std::nullopt;
                }

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
            using ListType = std::list<K>;
            using PosType = std::pair<std::size_t, typename ListType::iterator>;
            using MapType = std::unordered_map<K, PosType>;
            using BucketType = std::unordered_map<std::size_t, ListType>;

            std::size_t _capacity = 0;
            std::size_t _minFreq = 0;

            MapType _keyToBucket;
            BucketType _buckets;
    };
}
