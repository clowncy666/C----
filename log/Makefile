
CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -g


LDFLAGS = -lz


MAIN_SRC = 阶段整合.cpp


MAIN_OBJ = $(MAIN_SRC:.cpp=.o)


TARGET = log_processor 


all: $(TARGET)


$(TARGET): $(MAIN_OBJ)
	@echo "正在链接 $@..."
	$(CXX) $(MAIN_OBJ) -o $@ $(LDFLAGS)
	@echo "成功生成可执行文件 $(TARGET)"


%.o: %.cpp
	@echo "正在编译 $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "成功编译 $<"


clean:
	@echo "正在清理..."
	rm -f $(TARGET) $(MAIN_OBJ)
	@echo "清理完成。"

.PHONY: all clean

