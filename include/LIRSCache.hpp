#pragma once
#include "ICache.hpp"
#include <iostream>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <cstddef>

template <typename Key, typename Value>
class LIRSCache : public ICache<Key, Value> {
private:
    enum class Status { LIR, HIR_RESIDENT, HIR_NON_RESIDENT };

    struct Node {
        Key key{};
        Value value{};
        Status status = Status::LIR;
        bool in_stack = false;
        bool in_queue = false;
        typename std::list<Key>::iterator stack_pos{};
        typename std::list<Key>::iterator queue_pos{};

        Node() = default;
        Node(const Key& k, const Value& v, Status s)
            : key(k), value(v), status(s) {}
    };

    size_t cap;            
    size_t lir_capacity;
    size_t hir_capacity;

    size_t lir_size = 0;
    size_t hir_size = 0;
    size_t hitCount = 0;

    std::list<Key> stackS;
    std::list<Key> queueQ;
    std::unordered_map<Key, Node> registry;

    // --- вспомогательные методы ---

    void pushToStackFront(const Key& key, Node& node) {
        stackS.push_front(key);
        node.stack_pos = stackS.begin();
        node.in_stack = true;
    }

    void eraseFromStack(Node& node) {
        if (node.in_stack) {
            stackS.erase(node.stack_pos);
            node.in_stack = false;
        }
    }

    void pushToQueueFront(const Key& key, Node& node) {
        queueQ.push_front(key);
        node.queue_pos = queueQ.begin();
        node.in_queue = true;
    }

    void eraseFromQueue(Node& node) {
        if (node.in_queue) {
            queueQ.erase(node.queue_pos);
            node.in_queue = false;
        }
    }

    void evictOldestHIR() {
        if (queueQ.empty()) return;

        Key victim = queueQ.back();
        queueQ.pop_back();

        auto it = registry.find(victim);
        if (it == registry.end()) return;

        Node& node = it->second;
        node.in_queue = false;

        if (node.in_stack) {
            node.status = Status::HIR_NON_RESIDENT;
        } else {
            registry.erase(it);
        }

        if (hir_size > 0) hir_size--;
    }

    void stackPruning() {
        while (!stackS.empty()) {
            Key bottom = stackS.back();
            auto it = registry.find(bottom);
            if (it == registry.end()) {
                stackS.pop_back();
                continue;
            }
            if (it->second.status == Status::LIR) break;
            it->second.in_stack = false;
            stackS.pop_back();
        }
    }

    void demoteBottomLIR() {
        if (stackS.empty()) return;

        Key demoted_key = stackS.back();
        auto it = registry.find(demoted_key);
        if (it == registry.end()) {
            stackS.pop_back();
            return;
        }

        Node& demoted = it->second;
        stackS.pop_back();
        demoted.in_stack = false;
        demoted.status = Status::HIR_RESIDENT;
        if (lir_size > 0) lir_size--;

        while (hir_size >= hir_capacity && !queueQ.empty()) {
            evictOldestHIR();
        }

        pushToQueueFront(demoted_key, demoted);
        hir_size++;
    }

    void accessNode(const Key& key) {
        auto it = registry.find(key);
        if (it == registry.end()) return;

        Node& node = it->second;

        switch (node.status) {
        case Status::LIR:
            eraseFromStack(node);
            pushToStackFront(key, node);
            stackPruning();
            break;

        case Status::HIR_RESIDENT:
            if (node.in_stack) {
                eraseFromStack(node);
                eraseFromQueue(node);

                node.status = Status::LIR;
                lir_size++;
                if (hir_size > 0) hir_size--;

                pushToStackFront(key, node);

                if (lir_size > lir_capacity) {
                    demoteBottomLIR();
                }

                stackPruning();
            } else {
                pushToStackFront(key, node);
                eraseFromQueue(node);
                pushToQueueFront(key, node);
                stackPruning();
            }
            break;

        case Status::HIR_NON_RESIDENT:
            eraseFromStack(node);
            node.status = Status::LIR;
            lir_size++;

            pushToStackFront(key, node);

            if (lir_size > lir_capacity) {
                demoteBottomLIR();
            }

            stackPruning();
            break;
        }
    }

public:
    explicit LIRSCache(size_t cap_) : cap(cap_) {
        if (cap > 1) {
            lir_capacity = cap - 1;
            hir_capacity = 1;
        } else {
            lir_capacity = cap;
            hir_capacity = 0;
        }
    }

    bool get(const Key& key, Value& out) override {
        auto it = registry.find(key);
        if (it == registry.end() || it->second.status == Status::HIR_NON_RESIDENT) {
            return false;
        }

        hitCount++;
        out = it->second.value;
        accessNode(key);
        return true;
    }

    void put(const Key& key, const Value& value) override {
        if (cap == 0) return;

        auto it = registry.find(key);

        if (it != registry.end()) {
            Node& node = it->second;
            node.value = value; // Присваиваем новое актуальное значение

            if (node.status == Status::HIR_NON_RESIDENT) {
                node.status = Status::LIR;
                lir_size++;

                pushToStackFront(key, node);

                if (lir_size > lir_capacity) {
                    demoteBottomLIR();
                }
                stackPruning();
            } else {
                accessNode(key);
            }
            return;
        }

        // Если емкость для LIR еще не исчерпана
        if (lir_size < lir_capacity) {
            registry[key] = Node(key, value, Status::LIR);
            Node& stored = registry.at(key);
            pushToStackFront(key, stored);
            lir_size++;
        } else {
            while (hir_size >= hir_capacity && !queueQ.empty()) {
                evictOldestHIR();
            }

            if (hir_capacity == 0) return;

            registry[key] = Node(key, value, Status::HIR_RESIDENT);
            Node& stored = registry.at(key);
            pushToStackFront(key, stored);
            pushToQueueFront(key, stored);
            hir_size++;
        }
    }

    size_t getHitCount() const override { return hitCount; }
    size_t size() const override { return lir_size + hir_size; }
    size_t capacity() const override { return cap; }
    const char* name() const override { return "LIRS"; }

    void printState() const override {
        std::cout << "LIRS State (cap=" << cap
                  << ", LIR=" << lir_size << "/" << lir_capacity
                  << ", HIR=" << hir_size << "/" << hir_capacity << "):\n";
        std::cout << "  Stack S: ";
        for (auto k : stackS) {
            auto it = registry.find(k);
            const char* s = "?";
            if (it != registry.end()) {
                s = (it->second.status == Status::LIR) ? "LIR"
                  : (it->second.status == Status::HIR_RESIDENT) ? "HR"
                  : "HN";
            }
            std::cout << k << "(" << s << ") ";
        }
        std::cout << "\n  Queue Q: ";
        for (auto k : queueQ) std::cout << k << " ";
        std::cout << "\n  Хитов: " << hitCount << "\n";
    }

    // ДОБАВЛЕНО: Полная очистка состояния
    void clear() override {
        stackS.clear();
        queueQ.clear();
        registry.clear();
        lir_size = 0;
        hir_size = 0;
        hitCount = 0;
    }

    // ДОБАВЛЕНО: Сброс статистики
    void resetStats() override {
        hitCount = 0;
    }
};
