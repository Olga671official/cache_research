#include "MultiLevelCache.hpp"
#include "IdealCache.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <iomanip>
#include <string>
#include <numeric>


// Повторяющиеся циклы (последовательное сканирование)
std::vector<int> generateLoopPattern(int numElements, int loops) {
    std::vector<int> pattern;
    pattern.reserve(numElements * loops);
    for (int l = 0; l < loops; ++l) {
        for (int i = 1; i <= numElements; ++i) {
            pattern.push_back(i);
        }
    }
    return pattern;
}

// Распределение Зипфа / Парето (Закон 80/20)
std::vector<int> generateZipfPattern(int numElements, int totalRequests, double alpha) {
    std::vector<int> pattern;
    pattern.reserve(totalRequests);

    std::random_device rd;
    std::mt19937 gen(rd());

    // Рассчитываем веса для элементов по закону Зипфа: p(i) ~ 1 / i^alpha
    std::vector<double> weights(numElements);
    for (int i = 0; i < numElements; ++i) {
        weights[i] = 1.0 / std::pow(i + 1, alpha);
    }

    std::discrete_distribution<int> dist(weights.begin(), weights.end());

    for (int r = 0; r < totalRequests; ++r) {
        // dist() возвращает индекс от 0 до numElements-1, прибавляем 1 для ключа
        pattern.push_back(dist(gen) + 1);
    }
    return pattern;
}

// Случайный шум с редкими всплесками горячих данных
std::vector<int> generateNoiseWithBursts(int numElements, int totalRequests, int burstElement, double burstProb) {
    std::vector<int> pattern;
    pattern.reserve(totalRequests);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> randDist(1, numElements);
    std::uniform_real_distribution<double> probDist(0.0, 1.0);

    for (int r = 0; r < totalRequests; ++r) {
        if (probDist(gen) < burstProb) {
            pattern.push_back(burstElement);
        } else {
            pattern.push_back(randDist(gen));
        }
    }
    return pattern;
}

// --- 2. Функция запуска симуляции ---

struct ResultMetrics {
    double multiHitRate;
    double idealHitRate;
    double efficiency;
};

ResultMetrics runExperiment(const std::vector<int>& data, size_t totalCapacity, const std::vector<std::pair<std::string, size_t>>& levelConfigs) {
    // 1. Инициализация многоуровневого кэша
    MultiLevelCache<int, int> cache;
    for (const auto& cfg : levelConfigs) {
        cache.addLevel(cfg.first, cfg.second);
    }

    // Прогон данных через многоуровневую систему
    auto loadFn = [](int k) { return k; };
    int value = 0;
    for (int key : data) {
        cache.access(key, value, loadFn);
    }

    // 2. Инициализация Идеального кэша Белади (OPT)
    PerfectCache<int, int> ideal(totalCapacity);
    ideal.initFuture(data);
    
    int dummy_out = 0;
    for (int key : data) {
        if (!ideal.get(key, dummy_out)) {
            ideal.put(key, key);
        }
    }

    // Расчет метрик
    double mlRate = (static_cast<double>(cache.getSystemHits()) / data.size()) * 100.0;
    double idRate = (static_cast<double>(ideal.getHitCount()) / data.size()) * 100.0;
    double eff = (ideal.getHitCount() > 0) ? (static_cast<double>(cache.getSystemHits()) / ideal.getHitCount()) * 100.0 : 0.0;

    return {mlRate, idRate, eff};
}

// --- 3. Главная функция проведения исследований ---

void printTableHead(const std::string& patternName, size_t reqSize) {
    std::cout << "ПАТТЕРН НАГРУЗКИ: " << patternName << "\n";
    std::cout << "Количество запросов в серии: " << reqSize << "\n";
    std::cout << std::string(77, '-') << "\n";
    std::cout << std::left << std::setw(28) << "Архитектура системы" << " | "
              << std::setw(13) << "MultiLevel HR" << " | "
              << std::setw(15) << "Belady (OPT) HR" << " | "
              << "Эффективность" << "\n";
    std::cout << std::string(77, '-') << "\n";
}

int main() {
    std::cout << "!!               ЗАПУСК ИССЛЕДОВАНИЯ ПАТТЕРНОВ КЭША (C++)                 !!\n";

    size_t cacheCapacity = 10;

    std::vector<std::pair<std::string, std::vector<std::pair<std::string, size_t>>>> testArchitectures = {
        {"Связка L1(LRU) + L2(LFU)", {{"LRU", 5}, {"LFU", 5}}},
        {"Связка L1(2Q)  + L2(ARC)", {{"2Q", 5}, {"ARC", 5}}},
        {"Чистый LIRS (1 уровень)",  {{"LIRS", 10}}}
    };

    // 1. Эксперимент: Повторяющиеся циклы (Сканирование)
    // Генерируем 12 уникальных элементов (больше размера кэша 10) на 15 повторений
    auto loopData = generateLoopPattern(12, 15);
    printTableHead("Повторяющиеся циклы", loopData.size());
    for (const auto& arch : testArchitectures) {
        auto metrics = runExperiment(loopData, cacheCapacity, arch.second);
        std::cout << std::left << std::setw(28) << arch.first << " | "
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1) << metrics.multiHitRate << "% | "
                  << std::setw(14) << metrics.idealHitRate << "% | "
                  << std::setw(12) << metrics.efficiency << "%\n";
    }
    std::cout << std::string(77, '=') << "\n\n";

    // 2. Эксперимент: Распределение Зипфа (Трафик БД / Веб)
    auto zipfData = generateZipfPattern(50, 500, 1.2);
    printTableHead("Частые запросы (Трафик 80/20)", zipfData.size());
    for (const auto& arch : testArchitectures) {
        auto metrics = runExperiment(zipfData, cacheCapacity, arch.second);
        std::cout << std::left << std::setw(28) << arch.first << " | "
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1) << metrics.multiHitRate << "% | "
                  << std::setw(14) << metrics.idealHitRate << "% | "
                  << std::setw(12) << metrics.efficiency << "%\n";
    }
    std::cout << std::string(77, '=') << "\n\n";

    // 3. Эксперимент: Шум со всплесками горячих данных
    auto noiseData = generateNoiseWithBursts(50, 500, 999, 0.15);
    printTableHead("Случайный ввод + один горячий элемент", noiseData.size());
    for (const auto& arch : testArchitectures) {
        auto metrics = runExperiment(noiseData, cacheCapacity, arch.second);
        std::cout << std::left << std::setw(28) << arch.first << " | "
                  << std::right << std::setw(12) << std::fixed << std::setprecision(1) << metrics.multiHitRate << "% | "
                  << std::setw(14) << metrics.idealHitRate << "% | "
                  << std::setw(12) << metrics.efficiency << "%\n";
    }
    std::cout << std::string(77, '=') << "\n";

    return 0;
}
