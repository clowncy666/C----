# ==== Project ====
TARGET       := logger_merged_demo
SRC_DIR      := src
INC_DIR      := include
BUILD_DIR    := build
OBJ_DIR      := $(BUILD_DIR)/obj
BIN_DIR      := $(BUILD_DIR)/bin

CXX          ?= g++
CXX_STD      ?= c++20

# Build type: release (default) or debug (run: `make debug`)
BUILD        ?= release

COMMON_FLAGS := -std=$(CXX_STD) -Wall -Wextra -Wpedantic -I$(INC_DIR) -MMD -MP
ifeq ($(BUILD),debug)
  CXXFLAGS   := $(COMMON_FLAGS) -O0 -g
  TARGET     := $(TARGET)_dbg
else
  CXXFLAGS   := $(COMMON_FLAGS) -O3 -DNDEBUG
endif

LDFLAGS      :=
LDLIBS       := -lz -pthread

# For very old GCC (< 9), link stdc++fs (modern compilers don't need it)
GCC_MAJ := $(shell $(CXX) -dumpversion 2>/dev/null | cut -d. -f1)
ifeq ($(strip $(GCC_MAJ)),)
  GCC_MAJ := 0
endif
ifeq ($(shell [ $(GCC_MAJ) -lt 9 ] && echo yes),yes)
  LDLIBS += -lstdc++fs
endif

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all release debug run clean

all: release

release: $(BIN_DIR)/$(TARGET)

debug:
	$(MAKE) BUILD=debug

$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

run: $(BIN_DIR)/$(TARGET)
	@./$<

clean:
	@rm -rf $(BUILD_DIR)

-include $(DEPS)
