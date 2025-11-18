# ============================================
# 多模块日志系统 Makefile（增强版）
# 支持 Debug/Release 模式、并行编译、安装
# ============================================

# 编译器配置
CXX := g++
AR := ar
RM := rm -rf

# 编译模式（可通过 make MODE=debug 切换）
MODE ?= release

# 根据模式设置编译选项
ifeq ($(MODE),debug)
    CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0 -DDEBUG -pthread
    BUILD_SUFFIX := _debug
else
    CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -DNDEBUG -pthread
    BUILD_SUFFIX :=
endif

LDFLAGS := -pthread -lz

# 目录配置
SRC_DIR := src
INC_DIR := include
TEST_DIR := test
BUILD_DIR := build$(BUILD_SUFFIX)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
LIB_DIR := $(BUILD_DIR)/lib

# 安装目录（可通过 make install PREFIX=/usr/local 指定）
PREFIX ?= /usr/local
INSTALL_LIB_DIR := $(PREFIX)/lib
INSTALL_INC_DIR := $(PREFIX)/include/multi_logger

# 源文件
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
TEST_SOURCES := $(wildcard $(TEST_DIR)/*.cpp)

# 目标文件
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
TEST_OBJECTS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(TEST_SOURCES))

# 依赖文件（自动生成）
DEPENDS := $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

# 目标库和可执行文件
STATIC_LIB := $(LIB_DIR)/libmulti_logger.a
TEST_BIN := $(BIN_DIR)/test_multi_module

# 颜色输出
COLOR_RESET := \033[0m
COLOR_GREEN := \033[32m
COLOR_YELLOW := \033[33m
COLOR_BLUE := \033[34m

# 默认目标
.PHONY: all
all: banner $(STATIC_LIB) $(TEST_BIN)
	@echo "$(COLOR_GREEN)✓ Build complete ($(MODE) mode)$(COLOR_RESET)"

# 显示构建信息
.PHONY: banner
banner:
	@echo "$(COLOR_BLUE)======================================$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)  Multi-Module Logger Build System$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)  Mode: $(MODE)$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)======================================$(COLOR_RESET)"

# 创建必要的目录
$(OBJ_DIR) $(BIN_DIR) $(LIB_DIR):
	@mkdir -p $@

# 编译源文件（自动生成依赖）
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "$(COLOR_YELLOW)Compiling$(COLOR_RESET) $<"
	@$(CXX) $(CXXFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@

# 编译测试文件
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR)
	@echo "$(COLOR_YELLOW)Compiling$(COLOR_RESET) $<"
	@$(CXX) $(CXXFLAGS) -I$(INC_DIR) -MMD -MP -c $< -o $@

# 创建静态库
$(STATIC_LIB): $(OBJECTS) | $(LIB_DIR)
	@echo "$(COLOR_GREEN)Creating$(COLOR_RESET) $@"
	@$(AR) rcs $@ $^

# 链接测试程序
$(TEST_BIN): $(TEST_OBJECTS) $(STATIC_LIB) | $(BIN_DIR)
	@echo "$(COLOR_GREEN)Linking$(COLOR_RESET) $@"
	@$(CXX) $^ -o $@ $(LDFLAGS)

# 运行测试
.PHONY: test
test: $(TEST_BIN)
	@echo "$(COLOR_BLUE)Running tests...$(COLOR_RESET)"
	@cd $(BIN_DIR) && ./test_multi_module
	@echo "$(COLOR_GREEN)✓ Tests completed$(COLOR_RESET)"

# 安装库和头文件
.PHONY: install
install: $(STATIC_LIB)
	@echo "$(COLOR_YELLOW)Installing to $(PREFIX)...$(COLOR_RESET)"
	@mkdir -p $(INSTALL_LIB_DIR)
	@mkdir -p $(INSTALL_INC_DIR)
	@cp $(STATIC_LIB) $(INSTALL_LIB_DIR)/
	@cp $(INC_DIR)/*.h $(INSTALL_INC_DIR)/
	@echo "$(COLOR_GREEN)✓ Installation complete$(COLOR_RESET)"
	@echo "  Library: $(INSTALL_LIB_DIR)/libmulti_logger.a"
	@echo "  Headers: $(INSTALL_INC_DIR)/*.h"

# 卸载
.PHONY: uninstall
uninstall:
	@echo "$(COLOR_YELLOW)Uninstalling from $(PREFIX)...$(COLOR_RESET)"
	@$(RM) $(INSTALL_LIB_DIR)/libmulti_logger.a
	@$(RM) $(INSTALL_INC_DIR)
	@echo "$(COLOR_GREEN)✓ Uninstall complete$(COLOR_RESET)"

# 清理构建文件
.PHONY: clean
clean:
	@echo "$(COLOR_YELLOW)Cleaning build files...$(COLOR_RESET)"
	@$(RM) $(BUILD_DIR)
	@$(RM) build_debug
	@$(RM) logs
	@echo "$(COLOR_GREEN)✓ Clean complete$(COLOR_RESET)"

# 只清理日志
.PHONY: clean-logs
clean-logs:
	@echo "$(COLOR_YELLOW)Cleaning log files...$(COLOR_RESET)"
	@$(RM) logs
	@echo "$(COLOR_GREEN)✓ Logs cleaned$(COLOR_RESET)"

# 只编译库
.PHONY: lib
lib: $(STATIC_LIB)
	@echo "$(COLOR_GREEN)✓ Library built$(COLOR_RESET)"

# Debug 模式
.PHONY: debug
debug:
	@$(MAKE) MODE=debug all

# Release 模式（显式）
.PHONY: release
release:
	@$(MAKE) MODE=release all

# 重新编译
.PHONY: rebuild
rebuild: clean all

# 检查代码风格（需要 clang-format）
.PHONY: format
format:
	@echo "$(COLOR_YELLOW)Formatting code...$(COLOR_RESET)"
	@find $(SRC_DIR) $(INC_DIR) $(TEST_DIR) -name "*.cpp" -o -name "*.h" | xargs clang-format -i
	@echo "$(COLOR_GREEN)✓ Format complete$(COLOR_RESET)"

# 静态分析（需要 cppcheck）
.PHONY: analyze
analyze:
	@echo "$(COLOR_YELLOW)Running static analysis...$(COLOR_RESET)"
	@cppcheck --enable=all --std=c++17 -I$(INC_DIR) $(SRC_DIR) $(TEST_DIR)

# 显示目录树
.PHONY: tree
tree:
	@tree -I 'build*|logs' || ls -R

# 显示帮助信息
.PHONY: help
help:
	@echo "$(COLOR_BLUE)多模块日志系统 Makefile 使用指南$(COLOR_RESET)"
	@echo ""
	@echo "$(COLOR_GREEN)构建目标：$(COLOR_RESET)"
	@echo "  make              - 编译库和测试程序（release 模式）"
	@echo "  make debug        - 以 debug 模式编译"
	@echo "  make release      - 以 release 模式编译"
	@echo "  make lib          - 只编译静态库"
	@echo "  make test         - 编译并运行测试程序"
	@echo "  make rebuild      - 清理后重新编译"
	@echo ""
	@echo "$(COLOR_GREEN)安装/卸载：$(COLOR_RESET)"
	@echo "  make install      - 安装库和头文件到 $(PREFIX)"
	@echo "  make install PREFIX=/path - 安装到指定路径"
	@echo "  make uninstall    - 卸载已安装的文件"
	@echo ""
	@echo "$(COLOR_GREEN)清理：$(COLOR_RESET)"
	@echo "  make clean        - 清理所有构建文件和日志"
	@echo "  make clean-logs   - 只清理日志文件"
	@echo ""
	@echo "$(COLOR_GREEN)代码质量：$(COLOR_RESET)"
	@echo "  make format       - 格式化代码（需要 clang-format）"
	@echo "  make analyze      - 静态分析（需要 cppcheck）"
	@echo ""
	@echo "$(COLOR_GREEN)其他：$(COLOR_RESET)"
	@echo "  make info         - 显示构建配置信息"
	@echo "  make tree         - 显示目录树"
	@echo "  make help         - 显示此帮助信息"
	@echo ""
	@echo "$(COLOR_YELLOW)示例：$(COLOR_RESET)"
	@echo "  make MODE=debug   - Debug 模式编译"
	@echo "  make -j4          - 4 线程并行编译"
	@echo "  make test ARGS=-v - 向测试程序传递参数"

# 调试信息
.PHONY: info
info:
	@echo "$(COLOR_BLUE)构建配置信息：$(COLOR_RESET)"
	@echo "  编译器: $(CXX)"
	@echo "  编译模式: $(MODE)"
	@echo "  编译选项: $(CXXFLAGS)"
	@echo "  链接选项: $(LDFLAGS)"
	@echo "  构建目录: $(BUILD_DIR)"
	@echo ""
	@echo "$(COLOR_BLUE)文件统计：$(COLOR_RESET)"
	@echo "  源文件数量: $(words $(SOURCES))"
	@echo "  测试文件数量: $(words $(TEST_SOURCES))"
	@echo "  头文件数量: $(words $(wildcard $(INC_DIR)/*.h))"
	@echo ""
	@echo "$(COLOR_BLUE)输出文件：$(COLOR_RESET)"
	@echo "  静态库: $(STATIC_LIB)"
	@echo "  测试程序: $(TEST_BIN)"

# 包含自动生成的依赖文件
-include $(DEPENDS)

# 伪目标声明
.PHONY: all banner lib debug release rebuild test install uninstall \
        clean clean-logs format analyze tree help info