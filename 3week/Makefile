# Makefile for RollingFileManager

# —— 编译器与通用参数 ——
CXX       ?= g++
CPPFLAGS  := -Iinclude
CXXFLAGS  := -std=c++17 -Wall -Wextra -pedantic -pthread
LDFLAGS   :=
LDLIBS    := -pthread -lz         # zlib 用于 gzopen/gzwrite/gzclose

# —— 可配置的默认日志目录（传给源码的宏 DEFAULT_LOG_DIR）——
# 可用：make LOG_DIR=/some/dir
LOG_DIR   ?= ./logs/day1
CPPFLAGS  += -DDEFAULT_LOG_DIR=\"$(LOG_DIR)\"

# 如遇到非常老的 GCC/GLIBC 组合需要 filesystem 额外库，请手动取消下一行注释
# LDLIBS    += -lstdc++fs

# —— 目录与文件 ——
SRCDIR    := src
OBJDIR    := obj
BINDIR    := bin

SOURCES   := $(SRCDIR)/main.cpp $(SRCDIR)/RollingFileManager.cpp
OBJECTS   := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
TARGET    := $(BINDIR)/main

# —— 目标 —— 
all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BINDIR)
	@echo "链接: $@"
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "编译: $<"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BINDIR):
	@echo "创建目录: $@"
	@mkdir -p $(BINDIR)

$(OBJDIR):
	@echo "创建目录: $@"
	@mkdir -p $(OBJDIR)

clean:
	@echo "清理构建产物..."
	@rm -rf $(OBJDIR) $(BINDIR)

rebuild: clean all

.PHONY: all clean rebuild
