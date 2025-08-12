#pragma once

#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "ACacheStrategy.hpp"

namespace data {
    template<typename K, typename V>
    class LRUCacheStrategy final: public ACacheStrategy<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            LRUCacheStrategy() = default;
            ~LRUCacheStrategy() noexcept = default;

            virtual void onClear() noexcept override
            {
                _accessOrder.clear();
                _keyToIterator.clear();
            }

            virtual void onAccess(const K& key) override
            {
                const auto it = _keyToIterator.find(key);
                if (it != _keyToIterator.end()) {
                    _accessOrder.erase(it->second);
                    _accessOrder.push_front(key);
                    _keyToIterator[key] = _accessOrder.begin();
                    return;
                }
                throw std::invalid_argument("Key not in cache");
            }

            virtual void onInsert(const K& key) override
            {
                _accessOrder.push_front(key);
                _keyToIterator[key] = _accessOrder.begin();
            }

            virtual void onRemove(const K& key) override
            {
                const auto it = _keyToIterator.find(key);
                if (it != _keyToIterator.end()) {
                    _accessOrder.erase(it->second);
                    _keyToIterator.erase(it);
                }
            }

            virtual std::optional<K> selectForEviction() override
            {
                if (_accessOrder.empty()) {
                    return std::nullopt;
                }
                return _accessOrder.back();
            }

        protected:
            virtual void reserve_worker(std::size_t cap) override
            {
                if (cap > _capacity) {
                    _capacity = cap;
                    _keyToIterator.reserve(_capacity);
                }
            }

        private:
            std::size_t _capacity = 0;
            std::list<K> _accessOrder;
            std::unordered_map<K, typename std::list<K>::iterator> _keyToIterator;
    };
}
