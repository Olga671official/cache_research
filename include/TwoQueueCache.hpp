#pragma once
#include "ICache.hpp"
#include <iostream>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <cstddef>

template <typename Key, typename Value>
class TwoQueueCache : public ICache<Key, Value> {
private:
    enum class Location { In, Out, Main };

    struct Node {
        Key key;
        Value value;
        Location loc;
        bool has_value;
        typename std::list<Key>::iterator pos;
    };

    size_t cap;           
    size_t in_capacity;
    size_t out_capacity;
    size_t main_capacity;

    std::list<Key> AmIn;
    std::list<Key> AmOut;
    std::list<Key> AmMain;

    std::unordered_map<Key, Node> registry;
    size_t hitCount = 0;

    void evictFromIn() {
        if (AmIn.empty()) return;

        Key victim = AmIn.back();
        AmIn.pop_back();

        Node& node = registry.at(victim);
        node.loc = Location::Out;
        node.has_value = false; // Данные выгружаются, остаётся только призрак

        AmOut.push_front(victim);
        node.pos = AmOut.begin();

        // Ограничиваем размер очереди призраков
        if (AmOut.size() > out_capacity) {
            Key oldest = AmOut.back();
            AmOut.pop_back();
            registry.erase(oldest);
        }
    }

    void evictFromMain() {
        if (AmMain.empty()) return;

        Key victim = AmMain.back();
        AmMain.pop_back();

        // ИСПРАВЛЕНО: Вытесненный из Main элемент становится призраком в Out,
        // а не удаляется бесследно!
        Node& node = registry.at(victim);
        node.loc = Location::Out;
        node.has_value = false;

        AmOut.push_front(victim);
        node.pos = AmOut.begin();

        if (AmOut.size() > out_capacity) {
            Key oldest = AmOut.back();
            AmOut.pop_back();
            registry.erase(oldest);
        }
    }

public:
    explicit TwoQueueCache(size_t cap_) : cap(cap_) {
        if (cap == 0) {
            in_capacity = main_capacity = out_capacity = 0;
        } else {
            // Каноничное распределение: 25% под новые элементы, 75% под горячие
            in_capacity   = std::max<size_t>(1, cap / 4);
            main_capacity = cap - in_capacity;
            out_capacity  = cap; // Размер очереди истории равен размеру кэша
        }
    }

    bool get(const Key& key, Value& out) override {
        auto it = registry.find(key);
        // Элемент считается Hit, только если он найден и содержит реальное значение (In или Main)
        if (it == registry.end() || !it->second.has_value) {
            return false;
        }

        hitCount++;
        Node& node = it->second;
        out = node.value;

        // Попадание в AmIn не перемещает элемент в Main (он должен сначала умереть до Out)
        // Попадание в AmMain обновляет его "свежесть" внутри Main
        if (node.loc == Location::Main) {
            AmMain.erase(node.pos);
            AmMain.push_front(key);
            node.pos = AmMain.begin();
        }
        return true;
    }

    void put(const Key& key, const Value& value) override {
        if (cap == 0) return;

        auto it = registry.find(key);

        // СЛУЧАЙ 1: Элемент уже активен (в In или Main) — просто обновляем значение
        if (it != registry.end() && it->second.has_value) {
            Node& node = it->second;
            node.value = value;
            if (node.loc == Location::Main) {
                AmMain.erase(node.pos);
                AmMain.push_front(key);
                node.pos = AmMain.begin();
            }
            return;
        }

        // СЛУЧАЙ 2: Элемент запрашивается ПОВТОРНО (найден как призрак в очереди Out)
        // Переводим его в разряд горячих (в очередь Main)
        if (it != registry.end() && it->second.loc == Location::Out) {
            Node& node = it->second;
            AmOut.erase(node.pos);

            // Если Main переполнен, освобождаем место
            while (AmMain.size() >= main_capacity && !AmMain.empty()) {
                evictFromMain();
            }

            node.value = value;
            node.loc = Location::Main;
            node.has_value = true;
            AmMain.push_front(key);
            node.pos = AmMain.begin();
            return;
        }

        // СЛУЧАЙ 3: Абсолютно новый элемент (отсутствует в системе)
        // ИСПРАВЛЕНО: Перед вставкой контролируем размеры очередей
        if (AmIn.size() >= in_capacity) {
            evictFromIn();
        } else if ((AmIn.size() + AmMain.size()) >= cap) {
            // Если In ещё не полон, но суммарный лимит живых страниц превышен,
            // вытесняем из Main, чтобы освободить физическое место
            evictFromMain();
        }

        AmIn.push_front(key);
        registry[key] = Node{key, value, Location::In, true, AmIn.begin()};
    }

    size_t getHitCount() const override { return hitCount; }
    size_t size() const override { return AmIn.size() + AmMain.size(); }
    size_t capacity() const override { return cap; }
    const char* name() const override { return "2Q"; }

    void printState() const override {
        std::cout << "2Q Cache State (capacity=" << cap
                  << ", in=" << in_capacity
                  << ", main=" << main_capacity
                  << ", out=" << out_capacity << "):\n";
        std::cout << "  In   (новые):   "; for (auto k : AmIn)   std::cout << k << " "; std::cout << "\n";
        std::cout << "  Out  (призраки): "; for (auto k : AmOut)  std::cout << k << " "; std::cout << "\n";
        std::cout << "  Main (горячие):  "; for (auto k : AmMain) std::cout << k << " "; std::cout << "\n";
        std::cout << "  Хитов: " << hitCount << "\n";
    }

    void clear() override {
        AmIn.clear();
        AmOut.clear();
        AmMain.clear();
        registry.clear();
        hitCount = 0;
    }

    void resetStats() override {
        hitCount = 0;
    }
};
