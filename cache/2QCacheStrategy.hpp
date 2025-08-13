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

            virtual bool onAccess(const K& key) override
            {
                if (auto it = _posToAm.find(key); it != _posToAm.end()) {
                    _am.splice(_am.begin(), _am, it->second);
                    return true;
                }
                if (auto it = _posToA1.find(key); it != _posToA1.end()) {
                    _a1.erase(it->second);
                    _posToA1.erase(it);
                    _am.push_front(key);
                    _posToAm.emplace(key, _am.begin());
                    return true;
                }
                return false;
            }

            virtual void onInsert(const K& key) override
            {
                _a1.push_front(key);
                _posToA1.emplace(key, _a1.begin());
            }

            virtual void onRemove(const K& key) override
            {
                if (auto it = _posToA1.find(key); it != _posToA1.end()) {
                    _a1.erase(it->second);
                    _posToA1.erase(it);
                    return;
                }
                if (auto it = _posToAm.find(key); it != _posToAm.end()) {
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
            using ListType = std::list<K>;
            using MapType = std::unordered_map<K, typename ListType::iterator>;

            std::size_t _capacity = 0;
            ListType _am;
            ListType _a1;
            MapType _posToAm;
            MapType _posToA1;
    };
}
