#pragma once

#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "interfaces/ACacheStrategy.hpp"

namespace cache::strategy {
    template<typename K, typename V>
    class FIFO final: public ACacheStrategy<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            FIFO() = default;
            virtual ~FIFO() noexcept override = default;

            virtual void onClear() noexcept override
            {
                _accessOrder.clear();
                _keyToIterator.clear();
            }

            [[nodiscard]] virtual bool onAccess(const K& key) override
            {
                auto it = _keyToIterator.find(key);
                if (it == _keyToIterator.end()) {
                    return false;
                }
                return true;
            }

            [[nodiscard]] virtual bool onInsert(const K& key) override
            {
                _accessOrder.push_front(key);
                _keyToIterator.emplace(key, _accessOrder.begin());
                return true;
            }

            [[nodiscard]] virtual bool onRemove(const K& key) override
            {
                auto it = _keyToIterator.find(key);
                if (it != _keyToIterator.end()) {
                    _accessOrder.erase(it->second);
                    _keyToIterator.erase(it);
                }
                return true;
            }

            [[nodiscard]] virtual std::optional<K> selectForEviction() override
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
            using ListType = std::list<K>;
            using MapType = std::unordered_map<K, typename ListType::iterator>;

            std::size_t _capacity = 0;
            ListType _accessOrder;
            MapType _keyToIterator;
    };
}
