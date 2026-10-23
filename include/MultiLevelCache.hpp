#pragma once

#include "ICache.hpp"
#include "LRUCache.hpp"
#include "LFUCache.hpp"
#include "ARCCache.hpp"
#include "TwoQueueCache.hpp"
#include "LIRSCache.hpp"

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <algorithm>

// ============================================================
//  MultiLevelCache — многоуровневая система кэширования.
//
//  Семантика: NINE / INCLUSIVE (направленная вверх).
//  Если данные найдены на уровне L_i, они поднимаются на все
//  уровни выше (L_0, ..., L_{i-1}) через put().
// ============================================================

template <typename Key, typename Value>
class MultiLevelCache {
public:
    // Описание одного уровня из конфигурации
    struct LevelConfig {
        std::string algo;    // "LRU", "LFU", "ARC", "2Q", "LIRS"
        size_t capacity;     // размер этого уровня
    };

private:
    std::vector<std::unique_ptr<ICache<Key, Value>>> levels;
    std::vector<std::string> algoNames;

    // Статистика по системе
    size_t totalAccesses = 0;
    size_t systemHits    = 0;
    size_t fullMisses    = 0;
    std::vector<size_t> hitsPerLevel;

    // Фабрика: создаёт кэш по имени алгоритма
    static std::unique_ptr<ICache<Key, Value>> makeCache(const std::string& algo, size_t cap) {
        if (algo == "LRU")  return std::make_unique<LRUCache<Key, Value>>(cap);
        if (algo == "LFU")  return std::make_unique<LFUCache<Key, Value>>(cap);
        if (algo == "ARC")  return std::make_unique<ARCCache<Key, Value>>(cap);
        if (algo == "2Q")   return std::make_unique<TwoQueueCache<Key, Value>>(cap);
        if (algo == "LIRS") return std::make_unique<LIRSCache<Key, Value>>(cap);
        throw std::runtime_error("Unknown cache algorithm: " + algo);
    }

public:
    MultiLevelCache() = default;

    explicit MultiLevelCache(const std::vector<LevelConfig>& configs) {
        for (const auto& cfg : configs) {
            addLevel(cfg.algo, cfg.capacity);
        }
    }

    void addLevel(const std::string& algo, size_t capacity) {
        levels.push_back(makeCache(algo, capacity));
        algoNames.push_back(algo);
        hitsPerLevel.push_back(0);
    }

    // --- Основные методы доступа ---

    // Вариант 1: Без загрузчика (только поиск по уровням).
    bool access(const Key& key, Value& out) {
        totalAccesses++;

        // Ищем по уровням сверху вниз
        for (size_t i = 0; i < levels.size(); ++i) {
            Value v;
            if (levels[i]->get(key, v)) {
                // Нашли на уровне i → поднимаем во все уровни выше (j < i)
                for (size_t j = 0; j < i; ++j) {
                    levels[j]->put(key, v);
                }
                out = v;
                systemHits++;
                hitsPerLevel[i]++;
                return true;
            }
        }

        // Полный промах во всей системе
        fullMisses++;
        return false;
    }

    // Вариант 2: С загрузчиком (вызывает loadFn при промахе).
    // Исправлено: логика изолирована, чтобы избежать дублирования инкремента totalAccesses.
    template <typename LoadFn>
    bool access(const Key& key, Value& out, LoadFn loadFn) {
        totalAccesses++;

        // Проверяем наличие в кэш-системе
        for (size_t i = 0; i < levels.size(); ++i) {
            Value v;
            if (levels[i]->get(key, v)) {
                // Нашли на уровне i → поднимаем во все уровни выше
                for (size_t j = 0; j < i; ++j) {
                    levels[j]->put(key, v);
                }
                out = v;
                systemHits++;
                hitsPerLevel[i]++;
                return true;
            }
        }

        // Полный промах: загружаем данные из внешнего источника
        fullMisses++;
        Value v = loadFn(key);
        
        // Записываем во ВСЕ уровни кэша
        for (auto& lvl : levels) {
            lvl->put(key, v);
        }
        out = v;
        return false;
    }

    // --- Статистика ---

    size_t getTotalAccesses() const { return totalAccesses; }
    size_t getSystemHits()    const { return systemHits; }
    size_t getFullMisses()    const { return fullMisses; }
    size_t getLevelCount()    const { return levels.size(); }

    size_t getHitsAtLevel(size_t i) const {
        return (i < hitsPerLevel.size()) ? hitsPerLevel[i] : 0;
    }

    size_t getTotalLevelHits() const {
        size_t s = 0;
        for (auto h : hitsPerLevel) s += h;
        return s;
    }

    const ICache<Key, Value>* getLevel(size_t i) const {
        return (i < levels.size()) ? levels[i].get() : nullptr;
    }

    ICache<Key, Value>* getLevel(size_t i) {
        return (i < levels.size()) ? levels[i].get() : nullptr;
    }

    // --- Отладочный вывод ---

    void printState() const {
        std::cout << "=== MultiLevelCache ===\n";
        std::cout << "  Всего обращений: " << totalAccesses << "\n";
        std::cout << "  Системных хитов: " << systemHits
                  << " (" << (totalAccesses ? (100.0 * systemHits / totalAccesses) : 0.0)
                  << "%)\n";
        std::cout << "  Полных промахов: " << fullMisses << "\n";
        std::cout << "  Уровней: " << levels.size() << "\n";
        for (size_t i = 0; i < levels.size(); ++i) {
            std::cout << "  --- Уровень " << i
                      << " (" << algoNames[i] << ", cap=" << levels[i]->capacity() << ") ---\n";
            std::cout << "    size=" << levels[i]->size()
                      << ", hits=" << levels[i]->getHitCount()
                      << ", попаданий на этом уровне=" << hitsPerLevel[i] << "\n";
        }
    }

    void printEachLevelState() const {
        for (size_t i = 0; i < levels.size(); ++i) {
            std::cout << "--- Level " << i << " (" << algoNames[i] << ") ---\n";
            levels[i]->printState();
        }
    }

    // --- Управление ---

    void resetStats() {
        totalAccesses = systemHits = fullMisses = 0;
        std::fill(hitsPerLevel.begin(), hitsPerLevel.end(), 0);
        for (auto& lvl : levels) lvl->resetStats();
    }

    void clear() {
        for (auto& lvl : levels) lvl->clear();
        resetStats();
    }
};
