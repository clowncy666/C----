# -----------------------------------------------------------------------------
# Makefile for your logging project (focusing on 阶段整合.cpp)
# -----------------------------------------------------------------------------

# ---- 变量定义 ----

# C++ 编译器: 使用 g++ 来编译 C++ 代码
# (在 WSL 中，g++ 通常会自动链接 C++ 标准库)
CXX = g++

# 编译器选项 (Compiler Flags)
# -std=c++17: 指定使用 C++17 标准
# -Wall -Wextra: 开启所有常用的警告，有助于编写更健壮的代码
# -pthread: 链接 pthreads 库，因为代码中可能使用了 std::thread, std::mutex 等
# -g: 包含调试信息，方便你在 Visual Studio 的 Debugger 中进行调试
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -g

# 链接器选项 (Linker Flags)
# -lz: 链接 zlib 库。如果你的 阶段整合.cpp 依赖 zlib，就需要它。
LDFLAGS = -lz

# ---- 源文件和目标 ----

# 我们主要关注的源文件
MAIN_SRC = 阶段整合.cpp

# 对应的目标对象文件 (将 .cpp 替换为 .o)
MAIN_OBJ = $(MAIN_SRC:.cpp=.o)

# 最终生成的可执行程序名称
TARGET = log_processor # 你可以根据需要修改这个名字，例如 'my_logger_app'

# ---- 规则定义 ----

# 默认目标: 当你直接输入 'make' 命令时，执行这个规则
# 这个规则会先编译 MAIN_SRC，然后链接生成 TARGET
all: $(TARGET)

# 规则：链接对象文件以生成最终的可执行程序
# $@: 代表规则的目标 (例如 $(TARGET))
# $<: 代表规则的第一个依赖项 (在这个例子里，它会是 $(MAIN_OBJ))
$(TARGET): $(MAIN_OBJ)
	@echo "正在链接 $@..."
	$(CXX) $(MAIN_OBJ) -o $@ $(LDFLAGS)
	@echo "成功生成可执行文件 $(TARGET)"

# 规则：编译 .cpp 源文件到 .o 对象文件
# %.o:%.cpp 这是一个模式规则，表示如何将任意一个 .cpp 文件编译成对应的 .o 文件
# $<: 代表依赖项 (源 .cpp 文件)
# $@: 代表目标 (.o 文件)
# -c 选项表示只编译，不链接
%.o: %.cpp
	@echo "正在编译 $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "成功编译 $<"

# 规则：清理生成的文件
# 这个命令会删除可执行文件和所有的 .o 文件
clean:
	@echo "正在清理..."
	rm -f $(TARGET) $(MAIN_OBJ)
	@echo "清理完成。"

# .PHONY: 声明这些目标不是实际存在的文件，只是命令标签
.PHONY: all clean

# ---- Makefile 结束 ----