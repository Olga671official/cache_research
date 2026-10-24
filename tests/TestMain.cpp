#include "LRUCache.hpp"
#include "LFUCache.hpp"
#include "TwoQueueCache.hpp"
#include "ARCCache.hpp"
#include "LIRSCache.hpp"
#include "IdealCache.hpp"
#include "MultiLevelCache.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cassert>
#include <memory>

// Универсальная структура для хранения любого тест-кейса из файла
struct GenericTestCase {
    std::string mode;                  // "SINGLE" или "MULTI"
    std::string single_algo;           // для одиночного режима
    size_t single_capacity = 0;        // для одиночного режима
    
    // Для многоуровневого режима
    std::vector<MultiLevelCache<int, int>::LevelConfig> multi_configs;
    size_t total_multi_capacity = 0;

    std::vector<int> sequence;
    size_t expected_hits = 0;
};

// Функция парсинга любого текстового файла конфигурации тестов
GenericTestCase parseGenericFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ ОШИБКА ] Не удалось открыть файл теста: " << filepath << "\n";
        std::exit(1);
    }

    GenericTestCase tc;
    std::string line;

    // 1. Строка конфигурации архитектуры
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> tc.mode;
        if (tc.mode == "SINGLE") {
            iss >> tc.single_algo >> tc.single_capacity;
        } else if (tc.mode == "MULTI") {
            int num_levels;
            iss >> num_levels;
            for (int i = 0; i < num_levels; ++i) {
                std::string algo;
                size_t cap;
                iss >> algo >> cap;
                tc.multi_configs.push_back({algo, cap});
                tc.total_multi_capacity += cap;
            }
        }
    }

    // 2. Строка последовательности запросов
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        int key;
        while (iss >> key) {
            tc.sequence.push_back(key);
        }
    }

    // 3. Строка эталонного числа хитов
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> tc.expected_hits;
    }

    return tc;
}

// Фабрика для одиночных тестов
std::unique_ptr<ICache<int, int>> makeSingleCache(const std::string& algo, size_t cap) {
    if (algo == "LRU")  return std::make_unique<LRUCache<int, int>>(cap);
    if (algo == "LFU")  return std::make_unique<LFUCache<int, int>>(cap);
    if (algo == "2Q")   return std::make_unique<TwoQueueCache<int, int>>(cap);
    if (algo == "ARC")  return std::make_unique<ARCCache<int, int>>(cap);
    if (algo == "LIRS") return std::make_unique<LIRSCache<int, int>>(cap);
    return nullptr;
}

// Запуск универсального теста из файла
void executeFileTest(const std::string& filename) {
    std::string path = "test_cases/" + filename;
    GenericTestCase tc = parseGenericFile(path);
    size_t actual_hits = 0;

    std::cout << "[ RUN ] Тест из файла: " << filename << "\n";

    if (tc.mode == "SINGLE") {
        if (tc.single_algo == "OPT") {
            // Особая обработка для идеального кэша Белади
            PerfectCache<int, int> opt(tc.single_capacity);
            opt.initFuture(tc.sequence);
            int dummy;
            for (int key : tc.sequence) {
                if (!opt.get(key, dummy)) {
                    opt.put(key, key);
                }
            }
            actual_hits = opt.getHitCount();
        } else {
            // Обработка стандартных одиночных алгоритмов
            auto cache = makeSingleCache(tc.single_algo, tc.single_capacity);
            assert(cache != nullptr);
            int dummy;
            for (int key : tc.sequence) {
                if (!cache->get(key, dummy)) {
                    cache->put(key, key);
                }
            }
            actual_hits = cache->getHitCount();
        }
        std::cout << "        Режим: SINGLE (" << tc.single_algo << ", cap=" << tc.single_capacity << ")\n";
    } 
    else if (tc.mode == "MULTI") {
        // Тестирование каскадов L1 + L2 + ... из файла
        MultiLevelCache<int, int> multi;
        for (const auto& cfg : tc.multi_configs) {
            multi.addLevel(cfg.algo, cfg.capacity);
        }
        
        int value;
        auto loadFn = [](int k) { return k; };
        for (int key : tc.sequence) {
            multi.access(key, value, loadFn);
        }
        actual_hits = multi.getSystemHits();
        std::cout << "        Режим: MULTI (Уровней: " << tc.multi_configs.size() << ")\n";
    }

    std::cout << "        Результат -> Ожидалось хитов: " << tc.expected_hits << ", Получено: " << actual_hits << "\n";
    assert(actual_hits == tc.expected_hits);
    std::cout << "[ OK ] " << filename << " успешно пройден.\n\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << "  АВТОМАТИЧЕСКИЕ ТЕСТЫ: СЧИТЫВАНИЕ ВСЕХ СЦЕНАРИЕВ ИЗ ФАЙЛОВ \n";
    std::cout << "=======================================================\n\n";

    // Массив всех созданных тестовых файлов
    std::vector<std::string> test_files = {
        "01_lru.txt",
        "02_lfu.txt",
        "03_2q.txt",
        "04_arc.txt",
        "05_lirs.txt",
        "06_perfect.txt",
        "07_multilevel.txt",
        "lru_heavy.txt",
        "lfu_frequency.txt",
        "belady_slide1.txt"
    };

    for (const auto& file : test_files) {
        executeFileTest(file);
    }

    std::cout << "=======================================================\n";
    std::cout << " [ СТАТУС: УСПЕХ ] Абсолютно все тесты из файлов выполнены на 100%!\n";
    std::cout << "=======================================================\n";

    return 0;
}
