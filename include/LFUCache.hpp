#pragma once
#include "ICache.hpp"
#include <iostream>
#include <unordered_map>
#include <list>
#include <memory>
#include <cstddef>

template <typename Key, typename Value>
class LFUCache : public ICache<Key, Value> {
private:
    struct Node {
        Key key;
        Value value;
        int freq;
        typename std::list<Node*>::iterator pos;
    };

    size_t cap;         
    int minFreq;
    size_t hitCount = 0;

    std::unordered_map<Key, std::unique_ptr<Node>> keyToNode;
    std::unordered_map<int, std::list<Node*>> freqToList;

    void updateFrequency(Node* node) {
        int oldFreq = node->freq;
        auto& oldList = freqToList[oldFreq];
        oldList.erase(node->pos);

        if (oldList.empty()) {
            freqToList.erase(oldFreq);
            if (minFreq == oldFreq) {
                minFreq = oldFreq + 1;
            }
        }

        node->freq = oldFreq + 1;
        auto& newList = freqToList[node->freq];
        newList.push_front(node);
        node->pos = newList.begin();
    }

    void evictLFU() {
        if (freqToList.empty()) return;

        auto it = freqToList.find(minFreq);
        if (it == freqToList.end() || it->second.empty()) return;

        auto& minList = it->second;
        Node* victim = minList.back();
        minList.pop_back();

        if (minList.empty()) {
            freqToList.erase(it);
        }

        keyToNode.erase(victim->key);
    }

    void addNewNode(const Key& key, const Value& value) {
        if (keyToNode.size() >= cap) {
            evictLFU();
        }

        auto node = std::make_unique<Node>();
        node->key = key;
        node->value = value;
        node->freq = 1;

        Node* raw = node.get();
        auto& list = freqToList[1];
        list.push_front(raw);
        raw->pos = list.begin();

        keyToNode[key] = std::move(node);
        minFreq = 1;
    }

public:
    explicit LFUCache(size_t cap_) : cap(cap_), minFreq(0) {}

    LFUCache(const LFUCache&) = delete;
    LFUCache& operator=(const LFUCache&) = delete;
    LFUCache(LFUCache&&) = default;
    LFUCache& operator=(LFUCache&&) = default;

    ~LFUCache() override = default;

    bool get(const Key& key, Value& out) override {
        auto it = keyToNode.find(key);
        if (it == keyToNode.end()) {
            return false;
        }

        hitCount++;
        Node* node = it->second.get();
        out = node->value;
        updateFrequency(node);
        return true;
    }

    void put(const Key& key, const Value& value) override {
        if (cap == 0) return;

        auto it = keyToNode.find(key);
        if (it != keyToNode.end()) {
            Node* node = it->second.get();
            node->value = value;
            updateFrequency(node);
        } else {
            addNewNode(key, value);
        }
    }

    size_t getHitCount() const override { return hitCount; }

    size_t size() const override { return keyToNode.size(); }

    size_t capacity() const override { return cap; }

    const char* name() const override { return "LFU"; }

    void printState() const override {
        std::cout << "LFU Cache State (capacity=" << cap
                  << ", minFreq=" << minFreq << "):\n";
        for (auto& [freq, list] : freqToList) {
            std::cout << "  freq=" << freq << ": ";
            for (auto* node : list) {
                std::cout << node->key << " ";
            }
            std::cout << "\n";
        }
        std::cout << "  Хитов: " << hitCount << "\n";
    }

    // ДОБАВЛЕНО: Полная очистка кэша
    void clear() override {
        keyToNode.clear();
        freqToList.clear();
        minFreq = 0;
        hitCount = 0;
    }

    // ДОБАВЛЕНО: Сброс только статистики хитов
    void resetStats() override {
        hitCount = 0;
    }
};
