CXX = clang++
CXXFLAGS = -std=c++20
SRC = main.cpp constants.cpp board.cpp ai.cpp input.cpp
OUT = main

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)