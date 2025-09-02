#pragma once

#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include "interfaces/ACacheStrategy.hpp"

namespace cache::strategy {
    template<typename K, typename V>
    class SLRU final : public ACacheStrategy<K, V> {
        public:
            SLRU() = default;
            ~SLRU() noexcept override = default;

            virtual void onClear() noexcept override
            {
                _prob.clear();
                _prot.clear();
                _posProb.clear();
                _posProt.clear();
            }

            [[nodiscard]] virtual bool onInsert(const K& key) override
            {
                _prob.push_front(key);
                _posProb.emplace(key, _prob.begin());
                return true;
            }

            [[nodiscard]] virtual bool onAccess(const K& key) override
            {
                if (auto it = _posProt.find(key); it != _posProt.end()) {
                    _prot.splice(_prot.begin(), _prot, it->second);
                    return true;
                }
                if (auto it = _posProb.find(key); it != _posProb.end()) {
                    _prob.erase(it->second);
                    _posProb.erase(it);
                    _prot.push_front(key);
                    _posProt.emplace(key, _prot.begin());
                    enforceProtectedCap();
                    return true;
                }
                return false;
            }

            [[nodiscard]] virtual bool onRemove(const K& key) override
            {
                if (auto it = _posProb.find(key); it != _posProb.end()) {
                    _prob.erase(it->second);
                    _posProb.erase(it);
                    return true;
                }
                if (auto it = _posProt.find(key); it != _posProt.end()) {
                    _prot.erase(it->second);
                    _posProt.erase(it);
                }
                return true;
            }

            [[nodiscard]] virtual std::optional<K> selectForEviction() override
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
            virtual void reserve_worker(std::size_t cap) override
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
                    auto it = _posProt.find(demoteKey);
                    _prot.pop_back();
                    _posProt.erase(it);
                    _prob.push_front(demoteKey);
                    _posProb.emplace(demoteKey, _prob.begin());
                }
            }

            using ListType = std::list<K>;
            using MapType = std::unordered_map<K, typename ListType::iterator>;

            std::size_t _capacity = 0;
            std::size_t _protCap = 0;
            const double _protRatio = 0.67;
            ListType _prob;
            ListType _prot;
            MapType _posProb;
            MapType _posProt;
    };
}
