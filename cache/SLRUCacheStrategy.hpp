#pragma once
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "ACacheStrategy.hpp"

namespace data {
    template<typename K, typename V>
    class SLRUCacheStrategy final : public ACacheStrategy<K, V> {
        public:
            using KeyType = K;
            using ValType = V;

            SLRUCacheStrategy() = default;
            ~SLRUCacheStrategy() noexcept override = default;

            void onClear() noexcept override
            {
                _prob.clear();
                _prot.clear();
                _posProb.clear();
                _posProt.clear();
            }

            void onInsert(const K& key) override
            {
                _prob.push_front(key);
                _posProb.emplace(key, _prob.begin());
            }

            void onAccess(const K& key) override
            {
                if (auto it = _posProt.find(key); it != _posProt.end()) {
                    _prot.splice(_prot.begin(), _prot, it->second);
                    return;
                }
                if (auto it = _posProb.find(key); it != _posProb.end()) {
                    _prob.erase(it->second);
                    _posProb.erase(it);
                    _prot.push_front(key);
                    _posProt.emplace(key, _prot.begin());
                    enforceProtectedCap();
                    return;
                }
                throw std::invalid_argument("Key not in cache");
            }

            void onRemove(const K& key) override
            {
                if (auto it = _posProb.find(key); it != _posProb.end()) {
                    _prob.erase(it->second);
                    _posProb.erase(it);
                    return;
                }
                if (auto it = _posProt.find(key); it != _posProt.end()) {
                    _prot.erase(it->second);
                    _posProt.erase(it);
                }
            }

            [[nodiscard]] std::optional<K> selectForEviction() override
            {
                if (!_prob.empty()) {
                    return _prob.back();
                }
                if (!_prot.empty()) {
                    return _prot.back();
                }
                return std::nullopt;
            }

        protected:
            void reserve_worker(std::size_t cap) override
            {
                if (cap > _capacity) {
                    _capacity = cap;
                    _posProb.reserve(_capacity);
                    _posProt.reserve(_capacity);
                }
                _protCap = (_capacity == 0) ? 0 :
                    std::max<std::size_t>(1, static_cast<std::size_t>(_protRatio * static_cast<double>(_capacity)));
            }

        private:
            void enforceProtectedCap()
            {
                while (_protCap > 0 && _prot.size() > _protCap) {
                    const K& demoteKey = _prot.back();
                    const auto it = _posProt.find(demoteKey);
                    _prot.pop_back();
                    _posProt.erase(it);
                    _prob.push_front(demoteKey);
                    _posProb.emplace(demoteKey, _prob.begin());
                }
            }

            std::size_t _capacity = 0;
            std::size_t _protCap = 0;
            const double _protRatio = 0.67;
            std::list<K> _prob;
            std::list<K> _prot;
            std::unordered_map<K, typename std::list<K>::iterator> _posProb;
            std::unordered_map<K, typename std::list<K>::iterator> _posProt;
    };
}
