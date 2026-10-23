#include "MultiLevelCache.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <random>
#include <cmath>
#include <algorithm>

struct PairResult {
    std::string l1_name;
    std::string l2_name;
    size_t total_hits;
    double avg_hit_rate;
};

// Генератор трафика по закону Парето (Зипфа) — имитация реальной работы БД/сайтов
std::vector<int> generateZipfRequests(int numElements, int totalRequests, double alpha) {
    std::vector<int> pattern;
    pattern.reserve(totalRequests);
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<double> weights(numElements);
    for (int i = 0; i < numElements; ++i) {
        weights[i] = 1.0 / std::pow(i + 1, alpha);
    }
    std::discrete_distribution<int> dist(weights.begin(), weights.end());

    for (int r = 0; r < totalRequests; ++r) {
        pattern.push_back(dist(gen) + 1);
    }
    return pattern;
}

int main() {
    std::cout << "  ТЕСТИРОВАНИЕ ВСЕХ КОМБИНАЦИЙ L1 + L2 ДЛЯ ВЫБОРА ЛУЧШЕЙ АРХИТЕКТУРЫ \n";
    std::cout << "====================================================================\n\n";

    // Набор доступных алгоритмов для перебора
    std::vector<std::string> algos = {"LRU", "LFU", "2Q", "ARC", "LIRS"};
    
    // Генерируем тестовый трафик (50 уникальных элементов, 1000 запросов)
    auto requests = generateZipfRequests(50, 1000, 1.2);
    size_t total_cap = 10; // Общая память системы
    size_t l1_cap = 4;     // Память под быстрый L1
    size_t l2_cap = 6;     // Память под объемный L2

    std::vector<PairResult> leaderboard;
    auto loadFn = [](int k) { return k; };

    // Перебираем все возможные пары L1 и L2
    for (const auto& l1 : algos) {
        for (const auto& l2 : algos) {
            MultiLevelCache<int, int> cache;
            cache.addLevel(l1, l1_cap);
            cache.addLevel(l2, l2_cap);

            int value = 0;
            for (int key : requests) {
                cache.access(key, value, loadFn);
            }

            double hr = (static_cast<double>(cache.getSystemHits()) / requests.size()) * 100.0;
            leaderboard.push_back({l1, l2, cache.getSystemHits(), hr});
        }
    }

    std::sort(leaderboard.begin(), leaderboard.end(), [](const PairResult& a, const PairResult& b) {
        return a.total_hits > b.total_hits;
    });

    std::cout << std::left << std::setw(6) << "Место" << " | "
              << std::setw(10) << "Уровень L1" << " | "
              << std::setw(10) << "Уровень L2" << " | "
              << std::setw(12) << "Всего хитов" << " | "
              << "Hit Rate (%)\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t i = 0; i < leaderboard.size(); ++i) {
        std::cout << std::left << std::setw(6) << (i + 1) << " | "
                  << std::setw(10) << leaderboard[i].l1_name << " | "
                  << std::setw(10) << leaderboard[i].l2_name << " | "
                  << std::setw(12) << leaderboard[i].total_hits << " | "
                  << std::fixed << std::setprecision(1) << leaderboard[i].avg_hit_rate << "%\n";
        
        if (i == 9) {
            std::cout << "... [остальные результаты скрыты] ...\n";
            break;
        }
    }

    std::cout << "\nВывод для отчета: Лучшей комбинацией признана пара L1(" 
              << leaderboard[0].l1_name << ") + L2(" << leaderboard[0].l2_name << ").\n";
    
    return 0;
}
