CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -march=native
INCLUDES := -I./include -I./src
LDFLAGS := -lz -lpthread

SRC_DIR := src
BUILD_DIR := build
LIB_DIR := lib
OBJ_DIR := $(BUILD_DIR)/obj

# Logger 库的所有源文件
LOGGER_SRCS := $(shell find $(SRC_DIR) -name "*.cpp" -not -path "*test/*")
# Logger 库的目标文件 (.o)，放在 build/obj 目录下
LOGGER_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(LOGGER_SRCS))

# 测试程序的源文件
TEST_SRCS := test_logger.cpp
# 测试程序的目标文件 (.o)，可以放在 build/obj 目录下，或者直接编译链接
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))

# 最终的可执行程序名
TARGET := test_logger

.PHONY: all clean run test

# 默认目标：编译测试程序并运行
all: $(TARGET)

# 编译并运行测试程序
run: $(TARGET)
	@echo ""
	@echo "🚀 Running $(TARGET)..."
	@./$(TARGET)

# 编译测试程序
$(TARGET): $(LOGGER_OBJS) $(TEST_OBJS)
	@echo ""
	@echo "🔗 Linking executable: $@"
	@mkdir -p $(LIB_DIR) $(OBJ_DIR) # 确保目录存在
	@$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "✅ Executable $(TARGET) generated"

# 编译 Logger 库的 .cpp 文件到 .o 文件
# 模式匹配：build/obj/core/LoggerCore.o from src/core/LoggerCore.cpp
$(LOGGER_OBJS): $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "📦 Compiling Logger source: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 编译 test_logger.cpp 到 .o 文件
$(TEST_OBJS): $(OBJ_DIR)/%.o: %.cpp
	@echo "📦 Compiling Test source: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理生成的文件
clean:
	@echo "🧹 Cleaning build files..."
	@rm -rf $(BUILD_DIR) $(LIB_DIR) $(TARGET) $(OBJ_DIR)
	@echo "✅ Clean complete"