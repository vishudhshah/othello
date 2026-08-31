# Compiler, flags, source files, header files, object/dep paths, output file
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O3 -ffast-math -march=native -flto
SRC = main.cpp constants.cpp board.cpp ai.cpp input.cpp ui.cpp zobrist.cpp tt.cpp bitboard.cpp book.cpp openings.cpp
HEADERS = constants.hpp board.hpp ai.hpp input.hpp ui.hpp zobrist.hpp tt.hpp bitboard.hpp book.hpp openings.hpp
OUT = main

# ncurses (wide-char) via Homebrew; keg-only, so not on default include/lib paths
NCURSES_PREFIX := $(shell brew --prefix ncurses 2>/dev/null)
ifeq ($(NCURSES_PREFIX),)
NCURSES_PREFIX := /opt/homebrew/opt/ncurses
endif
CXXFLAGS += -I$(NCURSES_PREFIX)/include
LDFLAGS := -L$(NCURSES_PREFIX)/lib -lncursesw

# Create a build directory for .o and .d files
BUILD_DIR = build
OBJ = $(addprefix $(BUILD_DIR)/, $(SRC:.cpp=.o))
DEPS = $(OBJ:.o=.d)

# Default target
all: $(OUT)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build the output file (linking)
$(OUT): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LDFLAGS) -o $(OUT)

# Compile source files to object files and generate dependencies
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include auto-generated dependencies
-include $(DEPS)

# Clean up generated files (objects, executable, and build dir)
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUT)

# Rebuild everything from scratch
rebuild: clean all

# Declare phony targets
.PHONY: all clean rebuild
