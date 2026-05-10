CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -Iexternal/compiler_ir/include
TARGET := cmmc
SRCS := $(wildcard src/*.cpp)
EXT_SRCS := $(wildcard external/compiler_ir/src/*.cpp)
OBJ_DIR := build/obj
OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/src/%.o,$(SRCS))
EXT_OBJS := $(patsubst external/compiler_ir/src/%.cpp,$(OBJ_DIR)/compiler_ir/%.o,$(EXT_SRCS))

.PHONY: all clean test demo ui cmake-build cmake-test

all: $(TARGET)

$(TARGET): $(OBJS) $(EXT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/src/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/compiler_ir/%.o: external/compiler_ir/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	./scripts/run_tests.sh

demo: $(TARGET)
	rm -rf build/out
	./$(TARGET) --dump-all tests/valid/functions.sy -o build/out
	./$(TARGET) --lex tests/valid/basic.sy -o build/out/basic.tokens
	./$(TARGET) --parse tests/valid/basic.sy -o build/out/basic.parse
	./$(TARGET) --ast tests/valid/if_else.sy -o build/out/if_else.ast
	./$(TARGET) --ir tests/ir/simple.sy -o build/out/simple.ll

ui: $(TARGET)
	python3 ui/server.py

cmake-build:
	cmake -S . -B build/cmake
	cmake --build build/cmake

cmake-test:
	cmake --build build/cmake --target run_tests

clean:
	rm -rf $(OBJ_DIR) build/cmake $(TARGET)
	rm -f src/*.o
