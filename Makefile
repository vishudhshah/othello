CXX = clang++
CXXFLAGS = -std=c++20
SOURCES = main.cpp constants.cpp board.cpp ai.cpp input.cpp
OUT = main

$(OUT): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(OUT)

clean:
	rm -f $(OUT)