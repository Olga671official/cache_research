CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Iinclude
TARGET   := cache_simulator
TEST_TARGET     := run_tests
RESEARCH_TARGET := run_research
COMPARE_TARGET  := run_comparison

SRC_DIR   := src
BUILD_DIR := build
TEST_DIR  := tests

all: $(TARGET)

# 1. Основной симулятор
$(TARGET): $(BUILD_DIR)/main.o
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 2. Модульные тесты
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(BUILD_DIR)/TestMain.o
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/TestMain.o: $(TEST_DIR)/TestMain.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 3. Творческое исследование (Шаг 5)
research: $(RESEARCH_TARGET)
	./$(RESEARCH_TARGET)

$(RESEARCH_TARGET): $(BUILD_DIR)/research.o
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/research.o: $(SRC_DIR)/research.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
compare: $(COMPARE_TARGET)
	./$(COMPARE_TARGET)

$(COMPARE_TARGET): $(BUILD_DIR)/compare_pairs.o
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/compare_pairs.o: $(SRC_DIR)/compare_pairs.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET) $(RESEARCH_TARGET)

.PHONY: all clean test research
