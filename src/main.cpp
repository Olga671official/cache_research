#include "MultiLevelCache.hpp"
#include "ICache.hpp"
#include "IdealCache.hpp" // Проверьте имя заголовочного файла: PerfectCache.hpp или IdealCache.hpp
#include <numeric>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cctype>

struct Config {
    std::vector<MultiLevelCache<int, int>::LevelConfig> levels;
    std::vector<int> data;
    size_t cacheSize = 0;
};

// --- Утилиты ---

static std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

static bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (start == s.size()) return false;
    for (size_t i = start; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

static bool isAlgoName(const std::string& s) {
    static const char* names[] = {"LRU", "LFU", "ARC", "2Q", "LIRS"};
    for (auto* n : names) if (s == n) return true;
    return false;
}

// --- Парсер конфига ---

static Config parseConfig(std::istream& in) {
    Config cfg;

    std::string firstLine;
    while (std::getline(in, firstLine)) {
        if (!firstLine.empty() && firstLine.find_first_not_of(" \t\r\n") != std::string::npos)
            break;
    }
    if (firstLine.empty()) throw std::runtime_error("Config is empty");

    auto firstTokens = splitTokens(firstLine);

    if (firstTokens.empty())
        throw std::runtime_error("First line is empty");

    // --- A) "<numLevels> <algo1> <algo2> ..." ---
    if (firstTokens.size() >= 2 && isNumber(firstTokens[0])) {
        int n = std::stoi(firstTokens[0]);

        bool allAlgos = (firstTokens.size() == static_cast<size_t>(n) + 1);
        for (size_t i = 1; i < firstTokens.size(); ++i) {
            if (!isAlgoName(firstTokens[i])) { allAlgos = false; break; }
        }

        if (allAlgos && n > 0) {
            std::string secondLine;
            if (!std::getline(in, secondLine))
                throw std::runtime_error("Expected second line after level/algo list");
            auto second = splitTokens(secondLine);
            if (second.size() < 2)
                throw std::runtime_error("Second line must contain: <cacheSize> <n> <data...>");

            cfg.cacheSize = std::stoul(second[0]);
            size_t dataSize = std::stoul(second[1]);

            // ИСПРАВЛЕНО: Честно делим общий размер кэша между всеми уровнями
            size_t cap_per_level = cfg.cacheSize / n;
            size_t remainder = cfg.cacheSize % n;

            for (int i = 0; i < n; ++i) {
                size_t current_cap = cap_per_level + (i == 0 ? remainder : 0);
                cfg.levels.push_back({firstTokens[i + 1], current_cap});
            }

            for (size_t i = 2; i < second.size(); ++i)
                cfg.data.push_back(std::stoi(second[i]));
            while (cfg.data.size() < dataSize) {
                std::string line;
                if (!std::getline(in, line)) break;
                for (auto& t : splitTokens(line))
                    cfg.data.push_back(std::stoi(t));
            }
            return cfg;
        }

        // --- B) "<numLevels> <algo>" (несколько строк) ---
        if (firstTokens.size() == 2 && isAlgoName(firstTokens[1])) {
            int n = std::stoi(firstTokens[0]);
            std::vector<std::string> algos;
            algos.push_back(firstTokens[1]);

            while (static_cast<int>(algos.size()) < n) {
                std::string line;
                if (!std::getline(in, line)) break;
                for (auto& t : splitTokens(line)) {
                    if (isAlgoName(t)) algos.push_back(t);
                }
            }

            std::string dataLine;
            if (!std::getline(in, dataLine))
                throw std::runtime_error("Expected data line");

            auto dataToks = splitTokens(dataLine);
            if (dataToks.size() < 2)
                throw std::runtime_error("Data line must contain: <cacheSize> <n> <data...>");

            cfg.cacheSize = std::stoul(dataToks[0]);
            size_t dataSize = std::stoul(dataToks[1]);

            // ИСПРАВЛЕНО: Честно делим общий размер кэша между всеми уровнями
            size_t cap_per_level = cfg.cacheSize / n;
            size_t remainder = cfg.cacheSize % n;

            for (size_t i = 0; i < algos.size(); ++i) {
                size_t current_cap = cap_per_level + (i == 0 ? remainder : 0);
                cfg.levels.push_back({algos[i], current_cap});
            }

            for (size_t i = 2; i < dataToks.size(); ++i)
                cfg.data.push_back(std::stoi(dataToks[i]));

            while (cfg.data.size() < dataSize) {
                std::string line;
                if (!std::getline(in, line)) break;
                for (auto& t : splitTokens(line))
                    cfg.data.push_back(std::stoi(t));
            }
            return cfg;
        }

        // --- C) "<cacheSize>" — все данные в последующих строках ---
        if (firstTokens.size() == 1) {
            cfg.cacheSize = std::stoul(firstTokens[0]);
            cfg.levels.push_back({"LRU", cfg.cacheSize});

            std::string tok;
            while (in >> tok) {
                if (isNumber(tok)) cfg.data.push_back(std::stoi(tok));
            }
            return cfg;
        }

        // --- D) "<cacheSize> <n> <data...>" в одной строке ---
        if (firstTokens.size() >= 2 && isNumber(firstTokens[1])) {
            cfg.cacheSize = std::stoul(firstTokens[0]);
            size_t dataSize = std::stoul(firstTokens[1]);
            cfg.levels.push_back({"LRU", cfg.cacheSize});

            for (size_t i = 2; i < firstTokens.size(); ++i)
                cfg.data.push_back(std::stoi(firstTokens[i]));

            while (cfg.data.size() < dataSize) {
                std::string line;
                if (!std::getline(in, line)) break;
                for (auto& t : splitTokens(line))
                    cfg.data.push_back(std::stoi(t));
            }
            return cfg;
        }
    }

    throw std::runtime_error("Cannot parse config: unrecognized format");
}

// --- main ---

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.txt> [--verbose] [--levels]\n";
        return 1;
    }

    bool verbose = false;
    bool printLevels = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--verbose") verbose = true;
        else if (a == "--levels") printLevels = true;
    }

    std::ifstream fin(argv[1]);
    if (!fin) {
        std::cerr << "Cannot open config file: " << argv[1] << "\n";
        return 1;
    }

    Config cfg;
    try {
        cfg = parseConfig(fin);
    } catch (const std::exception& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    if (cfg.levels.empty()) {
        std::cerr << "No cache levels configured\n";
        return 1;
    }

    MultiLevelCache<int, int> cache;
    for (auto& lvl : cfg.levels) {
        cache.addLevel(lvl.algo, lvl.capacity);
    }

    // Обработка потока запросов через MultiLevelCache
    auto loadFn = [](int k) { return k; };
    for (int key : cfg.data) {
        int value = 0;
        cache.access(key, value, loadFn);
    }

    // Требование слайда: "на выход только число хитов"
    std::cout << cache.getSystemHits() << "\n";

    if (verbose) {
        std::cout << "\n";
        cache.printState();
    }
    if (printLevels) {
        std::cout << "\n";
        cache.printEachLevelState();
    }
    
    // Считаем суммарную емкость всех уровней для честного сравнения с OPT
    size_t sumCap = 0;
    for (const auto& lvl : cfg.levels) sumCap += lvl.capacity;

    // ВАЖНО: Убедитесь, что класс называется PerfectCache (или замените на IdealCache, если переименовали файл)
    PerfectCache<int, int> ideal(sumCap);
    ideal.initFuture(cfg.data);
    
    int dummy_out = 0;
    for (int key : cfg.data) {
        if (!ideal.get(key, dummy_out)) {
            ideal.put(key, key);
        }
    }

    if (verbose || printLevels) {
        std::cout << "\n================================================\n";
        std::cout << "MultiLevel hits: " << cache.getSystemHits() 
                  << " / " << cfg.data.size() << "\n";
        std::cout << "IdealCache hits: " << ideal.getHitCount() 
                  << " / " << cfg.data.size() << "\n";
        std::cout << "Efficiency: " 
                  << (ideal.getHitCount() ? 100.0 * cache.getSystemHits() / ideal.getHitCount() : 0.0)
                  << "% of ideal\n";
        std::cout << "================================================\n";
    }

    return 0;
}
