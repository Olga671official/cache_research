#pragma once
#include "ICache.hpp"
#include <vector>
#include <unordered_map>
#include <set>
#include <iostream>
#include <limits>

template <typename Key, typename Value>
class PerfectCache : public ICache<Key, Value> {
private:
    size_t cap;
    size_t hitCount = 0;
    
    // Хранилище данных кеша: ключ -> значение
    std::unordered_map<Key, Value> registry;
    
    // Глобальная история: хранит индексы всех появлений для каждого ключа
    // Ключ -> список его индексов во входном векторе
    std::unordered_map<Key, std::vector<size_t>> future_map;
    
    // Текущий индекс в потоке запросов
    size_t current_step = 0;

    // Вспомогательная функция: находит индекс следующего появления ключа
    size_t getNextOccurrence(const Key& key) const {
        auto it = future_map.find(key);
        if (it == future_map.end()) {
            return std::numeric_limits<size_t>::max(); // Никогда больше не встретится
        }
        
        const auto& indices = it->second;
        // Ищем первое появление, которое строго больше или равно текущему шагу
        for (size_t idx : indices) {
            if (idx >= current_step) {
                return idx;
            }
        }
        return std::numeric_limits<size_t>::max(); // Больше не встретится
    }

public:
    explicit PerfectCache(size_t capacity_) : cap(capacity_) {}

    // Метод для инициализации будущего (вызывается ОДИН раз перед симуляцией)
    void initFuture(const std::vector<Key>& sequence) {
        future_map.clear();
        current_step = 0;
        hitCount = 0;
        registry.clear();
        
        for (size_t i = 0; i < sequence.size(); ++i) {
            future_map[sequence[i]].push_back(i);
        }
    }

    bool get(const Key& key, Value& out) override {
        // Идеальный кеш должен знать, на каком он шаге
        // Поскольку при вызове get/put мы обрабатываем очередной элемент,
        // шаг инкрементируется в конце операций или в вызывающем коде.
        
        auto it = registry.find(key);
        if (it == registry.end()) {
            // При промахе текущий шаг все равно сдвигается вперед
            // Но инкремент шага лучше делать согласованно. 
            // Для простоты сдвигаем шаг при каждом обращении к get/put извне
            return false;
        }

        hitCount++;
        out = it->second;
        current_step++; // Переходим к следующему запросу
        return true;
    }

    void put(const Key& key, const Value& value) override {
        if (cap == 0) {
            current_step++;
            return;
        }

        // Если элемент уже в кеше (обновление значения)
        auto it = registry.find(key);
        if (it != registry.end()) {
            it->second = value;
            current_step++;
            return;
        }

        // Если кеш заполнен, нужно выбрать "жертву" (victim)
        if (registry.size() >= cap) {
            Key victim;
            size_t farthest_dist = 0;

            // Ищем элемент в кеше, который встретится позже всех
            for (const auto& pair : registry) {
                size_t next_pos = getNextOccurrence(pair.first);
                if (next_pos > farthest_dist) {
                    farthest_dist = next_pos;
                    victim = pair.first;
                }
            }

            // Вытесняем самую дальнюю или ненужную страницу
            registry.erase(victim);
        }

        // Вставляем новый элемент
        registry[key] = value;
        current_step++; // Переходим к следующему запросу
    }

    size_t getHitCount() const override { return hitCount; }
    size_t size() const override { return registry.size(); }
    size_t capacity() const override { return cap; }
    const char* name() const override { return "OPT"; }

    void printState() const override {
        std::cout << "PerfectCache (cap=" << cap << ", size=" << registry.size()
                  << ", hits=" << hitCount << ", step=" << current_step << "): ";
        for (auto& pair : registry) std::cout << pair.first << " ";
        std::cout << "\n";
    }
    
    void clear() override {
        registry.clear();
        future_map.clear();
        current_step = 0;
        hitCount = 0;
    }
};
