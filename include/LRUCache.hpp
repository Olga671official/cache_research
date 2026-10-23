#pragma once
#include "ICache.hpp"
#include <list>
#include <unordered_map>
#include <iostream>

template <typename Key, typename Value>
class LRUCache : public ICache<Key, Value> {
private:
    struct Node {
        Value value;
        typename std::list<Key>::iterator pos;
    };

    size_t cap;
    size_t hitCount = 0;
    std::list<Key> order;
    std::unordered_map<Key, Node> registry;

public:
    explicit LRUCache(size_t capacity_) : cap(capacity_) {}

    bool get(const Key& key, Value& out) override {
        auto it = registry.find(key);
        if (it == registry.end()) return false;

        hitCount++;
        out = it->second.value;

        order.erase(it->second.pos);
        order.push_front(key);
        it->second.pos = order.begin();
        return true;
    }

    void put(const Key& key, const Value& value) override {
        if (cap == 0) return;

        auto it = registry.find(key);
        if (it != registry.end()) {
            it->second.value = value;
            order.erase(it->second.pos);
            order.push_front(key);
            it->second.pos = order.begin();
            return;
        }

        if (registry.size() >= cap) {
            Key victim = order.back();
            order.pop_back();
            registry.erase(victim);
        }

        order.push_front(key);
        registry[key] = Node{value, order.begin()};
    }

    size_t getHitCount() const override { return hitCount; }
    size_t size() const override { return registry.size(); }
    size_t capacity() const override { return cap; }
    const char* name() const override { return "LRU"; }

    void printState() const override {
        std::cout << "LRU (cap=" << cap << ", size=" << registry.size()
                  << ", hits=" << hitCount << "): ";
        for (auto& k : order) std::cout << k << " ";
        std::cout << "\n";
    }
};