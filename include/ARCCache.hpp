#pragma once
#include "ICache.hpp"
#include <iostream>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <vector>
#include <cstddef>

template <typename Key, typename Value>
class ARCCache : public ICache<Key, Value> {
private:
    enum class ListId { T1, T2, B1, B2 };

    struct Node {
        Key key;
        Value value;
        bool has_value;
        ListId list;
        typename std::list<Key>::iterator pos;
    };

    size_t c; // общая ёмкость кэша
    size_t p; // параметр адаптации

    std::list<Key> T1;
    std::list<Key> B1;
    std::list<Key> T2;
    std::list<Key> B2;

    std::unordered_map<Key, Node> registry;
    size_t hitCount = 0;

    std::list<Key>& list_of(ListId id) {
        switch (id) {
            case ListId::T1: return T1;
            case ListId::T2: return T2;
            case ListId::B1: return B1;
            case ListId::B2: return B2;
        }
        return T1;
    }

    void erase_from_list(Node& node) {
        list_of(node.list).erase(node.pos);
    }

    void replace(bool in_B2) {
        if (!T1.empty() && ((T1.size() > p) || (in_B2 && T1.size() == p))) {
            Key old_key = T1.back();
            T1.pop_back();

            B1.push_front(old_key);
            Node& n = registry[old_key];
            n.has_value = false;
            n.list = ListId::B1;
            n.pos = B1.begin();
        } else if (!T2.empty()) {
            Key old_key = T2.back();
            T2.pop_back();

            B2.push_front(old_key);
            Node& n = registry[old_key];
            n.has_value = false;
            n.list = ListId::B2;
            n.pos = B2.begin();
        }
    }

    void promote_to_T2(Node& node) {
        erase_from_list(node);
        T2.push_front(node.key);
        node.list = ListId::T2;
        node.pos = T2.begin();
        node.has_value = true;
    }

public:
    explicit ARCCache(size_t capacity_) : c(capacity_), p(0) {}

    bool get(const Key& key, Value& out) override {
        auto it = registry.find(key);
        if (it == registry.end() || !it->second.has_value) {
            return false;
        }

        hitCount++;
        Node& node = it->second;
        out = node.value;
        promote_to_T2(node);
        return true;
    }

    void put(const Key& key, const Value& value) override {
        if (c == 0) return;

        auto it = registry.find(key);

        // Случай 1: Элемент уже находится в кэше (T1 или T2)
        if (it != registry.end() && it->second.has_value) {
            it->second.value = value;
            promote_to_T2(it->second);
            return;
        }

        // Случай 2: Полный промах (нет даже в истории призраков)
        if (it == registry.end()) {
            if (T1.size() + B1.size() == c) {
                if (T1.size() < c) {
                    registry.erase(B1.back());
                    B1.pop_back();
                    replace(false);
                } else {
                    registry.erase(T1.back());
                    T1.pop_back();
                }
            } else if (T1.size() + B1.size() + T2.size() + B2.size() >= c) {
                if (T1.size() + B1.size() + T2.size() + B2.size() == 2 * c) {
                    registry.erase(B2.back());
                    B2.pop_back();
                }
                replace(false);
            }

            T1.push_front(key);
            registry[key] = Node{key, value, true, ListId::T1, T1.begin()};
            return;
        }

        // Случай 3: Элемент найден в истории призраков (B1 или B2)
        Node& node = it->second;
        node.value = value;

        if (node.list == ListId::B1) {
            // Адаптация цены в сторону увеличения T1 (рост p)
            size_t delta = (B1.size() >= B2.size()) ? 1 : (B2.size() / B1.size());
            p = std::min(c, p + delta);
            replace(true); // ИСПРАВЛЕНО: для B1 передаем true
            B1.erase(node.pos);
        } else {
            // Адаптация цены в сторону увеличения T2 (уменьшение p)
            size_t delta = (B2.size() >= B1.size()) ? 1 : (B1.size() / B2.size());
            p = (p > delta) ? p - delta : 0;
            replace(false); // ИСПРАВЛЕНО: для B2 передаем false
            B2.erase(node.pos);
        }

        T2.push_front(key);
        node.list = ListId::T2;
        node.pos = T2.begin();
        node.has_value = true;
    }

    size_t getHitCount() const override { return hitCount; }

    size_t size() const override { return T1.size() + T2.size(); }

    size_t capacity() const override { return c; }

    const char* name() const override { return "ARC"; }

    void printState() const override {
        std::cout << "ARC State (c=" << c << ", p=" << p << "):\n";
        std::cout << "  T1 (в кэше, недавние): "; for (auto k : T1) std::cout << k << " "; std::cout << "\n";
        std::cout << "  T2 (в кэше, частые):   "; for (auto k : T2) std::cout << k << " "; std::cout << "\n";
        std::cout << "  B1 (история недавних): "; for (auto k : B1) std::cout << k << " "; std::cout << "\n";
        std::cout << "  B2 (история частых):   "; for (auto k : B2) std::cout << k << " "; std::cout << "\n";
        std::cout << "  Хитов: " << hitCount << "\n";
    }

    // ДОБАВЛЕНО: Очистка состояния
    void clear() override {
        T1.clear();
        T2.clear();
        B1.clear();
        B2.clear();
        registry.clear();
        p = 0;
        hitCount = 0;
    }

    // ДОБАВЛЕНО: Сброс статистики
    void resetStats() override {
        hitCount = 0;
    }
};
