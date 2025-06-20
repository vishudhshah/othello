# Compiler, flags, source files, header files, object/dep paths, output file
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
SRC = main.cpp constants.cpp board.cpp ai.cpp input.cpp
HEADERS = constants.hpp board.hpp ai.hpp input.hpp
OUT = main

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
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(OUT)

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