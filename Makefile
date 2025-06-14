# Compiler, flags, source files, header files, object/dep paths, output file
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
SRC = main.cpp Game.cpp Board.cpp Player.cpp HumanPlayer.cpp AIPlayer.cpp NegamaxStrategy.cpp
HEADERS = GameConstants.hpp Game.hpp Board.hpp Player.hpp HumanPlayer.hpp AIPlayer.hpp NegamaxStrategy.hpp AIStrategy.hpp
OUT = main

# Create a build directory for .o and .d files
BUILD_DIR = build
OBJ = $(addprefix $(BUILD_DIR)/, $(SRC:.cpp=.o))
DEPS = $(OBJ:.o=.d)

# Ensure the build directory exists
$(shell mkdir -p $(BUILD_DIR))

# Default target
all: $(OUT)

# Build the output file (linking)
$(OUT): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(OUT)

# Compile source files to object files and generate dependencies
$(BUILD_DIR)/%.o: %.cpp
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