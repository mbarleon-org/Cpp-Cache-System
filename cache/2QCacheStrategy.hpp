#pragma once

#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "ACacheStrategy.hpp"

namespace data {
    template<typename K, typename V>
    class TwoQCacheStrategy final: public ACacheStrategy<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            TwoQCacheStrategy() = default;
            virtual ~TwoQCacheStrategy() noexcept override = default;

            virtual void onClear() noexcept override
            {
                _a1.clear();
                _am.clear();
                _posToA1.clear();
                _posToAm.clear();
            }

            virtual void onAccess(const K& key) override
            {
                if (const auto it = _posToAm.find(key); it != _posToAm.end()) {
                    _am.splice(_am.begin(), _am, it->second);
                    return;
                }
                if (const auto it = _posToA1.find(key); it != _posToA1.end()) {
                    _a1.erase(it->second);
                    _posToA1.erase(it);
                    _am.push_front(key);
                    _posToAm.emplace(key, _am.begin());
                    return;
                }
                throw std::invalid_argument("Key not in cache");
            }

            virtual void onInsert(const K& key) override
            {
                _a1.push_front(key);
                _posToA1.emplace(key, _a1.begin());
            }

            virtual void onRemove(const K& key) override
            {
                if (const auto it = _posToA1.find(key); it != _posToA1.end()) {
                    _a1.erase(it->second);
                    _posToA1.erase(it);
                    return;
                }
                if (const auto it = _posToAm.find(key); it != _posToAm.end()) {
                    _am.erase(it->second);
                    _posToAm.erase(it);
                }
            }

            [[nodiscard]] virtual std::optional<K> selectForEviction() override
            {
                if (!_a1.empty()) {
                    return _a1.back();
                }
                if (!_am.empty()) {
                    return _am.back();
                }
                return std::nullopt;
            }

        protected:
            virtual void reserve_worker(std::size_t cap) override
            {
                if (cap > _capacity) {
                    _capacity = cap;
                    _posToAm.reserve(_capacity);
                    _posToA1.reserve(_capacity);
                }
            }

        private:
            std::size_t _capacity = 0;
            std::list<K> _am;
            std::list<K> _a1;
            std::unordered_map<K, typename std::list<K>::iterator> _posToAm;
            std::unordered_map<K, typename std::list<K>::iterator> _posToA1;
    };
}
