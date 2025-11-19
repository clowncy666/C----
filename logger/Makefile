# ============================================
# Logger Library Makefile
# ============================================

# 编译器设置
CXX := g++
AR := ar
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -march=native
INCLUDES := -I./include -I./src
LDFLAGS := -lz -lpthread

# 目录设置
SRC_DIR := src
BUILD_DIR := build
LIB_DIR := lib
OBJ_DIR := $(BUILD_DIR)/obj

# 源文件
CORE_SRCS := $(SRC_DIR)/core/LoggerCore.cpp
SINK_SRCS := $(wildcard $(SRC_DIR)/sinks/*.cpp)
MANAGER_SRCS := $(wildcard $(SRC_DIR)/manager/*.cpp)
LOGGER_SRCS := $(SRC_DIR)/Logger.cpp

ALL_SRCS := $(CORE_SRCS) $(SINK_SRCS) $(MANAGER_SRCS) $(LOGGER_SRCS)

# 目标文件
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(ALL_SRCS))

# 库文件
STATIC_LIB := $(LIB_DIR)/liblogger.a
SHARED_LIB := $(LIB_DIR)/liblogger.so

# 测试程序
TEST_PROG := test_logger

# ============================================
# 主目标
# ============================================

.PHONY: all clean test install help static shared

# 默认编译静态库
all: static

# 编译静态库
static: $(STATIC_LIB)

# 编译动态库
shared: $(SHARED_LIB)

# 编译并运行测试
test: $(TEST_PROG)
	@echo "🧪 运行测试..."
	@./$(TEST_PROG)

# 清理
clean:
	@echo "🧹 清理编译文件..."
	@rm -rf $(BUILD_DIR) $(LIB_DIR) $(TEST_PROG)
	@rm -rf ./logs ./test_logs
	@echo "✅ 清理完成"

# 帮助
help:
	@echo "可用目标:"
	@echo "  make          - 编译静态库 (默认)"
	@echo "  make static   - 编译静态库"
	@echo "  make shared   - 编译动态库"
	@echo "  make test     - 编译并运行测试"
	@echo "  make install  - 安装到系统"
	@echo "  make clean    - 清理编译文件"

# ============================================
# 编译规则
# ============================================

# 创建目录
$(OBJ_DIR) $(LIB_DIR):
	@mkdir -p $@
	@mkdir -p $(OBJ_DIR)/core
	@mkdir -p $(OBJ_DIR)/sinks
	@mkdir -p $(OBJ_DIR)/manager

# 编译目标文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "📦 编译: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 静态库
$(STATIC_LIB): $(OBJS) | $(LIB_DIR)
	@echo "📚 生成静态库: $@"
	@$(AR) rcs $@ $^
	@echo "✅ 静态库编译完成: $(STATIC_LIB)"

# 动态库
$(SHARED_LIB): $(OBJS) | $(LIB_DIR)
	@echo "📚 生成动态库: $@"
	@$(CXX) -shared -o $@ $^ $(LDFLAGS)
	@echo "✅ 动态库编译完成: $(SHARED_LIB)"

# 测试程序
$(TEST_PROG): test_logger.cpp $(STATIC_LIB)
	@echo "🔨 编译测试程序..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -L$(LIB_DIR) -llogger $(LDFLAGS)
	@echo "✅ 测试程序编译完成: $(TEST_PROG)"

# ============================================
# 安装规则（可选）
# ============================================

PREFIX ?= /usr/local
INSTALL_INC_DIR := $(PREFIX)/include
INSTALL_LIB_DIR := $(PREFIX)/lib

install: $(STATIC_LIB)
	@echo "📥 安装到 $(PREFIX)..."
	@mkdir -p $(INSTALL_INC_DIR)/logger
	@mkdir -p $(INSTALL_LIB_DIR)
	@cp -r include/logger/* $(INSTALL_INC_DIR)/logger/
	@cp $(STATIC_LIB) $(INSTALL_LIB_DIR)/
	@echo "✅ 安装完成"

uninstall:
	@echo "🗑️  卸载..."
	@rm -rf $(INSTALL_INC_DIR)/logger
	@rm -f $(INSTALL_LIB_DIR)/liblogger.a
	@echo "✅ 卸载完成"

# ============================================
# 依赖关系（可选，用于增量编译）
# ============================================

-include $(OBJS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(patsubst %.d,%.o,$@) $< > $@