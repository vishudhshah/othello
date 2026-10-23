cmake_minimum_required(VERSION 3.16)
project(Othello LANGUAGES CXX)

# Default build type to Release (optimized)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Set source files and create target executable
add_executable(main
  main.cpp
  constants.cpp
  board.cpp
  ai.cpp
  input.cpp
  ui.cpp
  zobrist.cpp
  tt.cpp
  bitboard.cpp
  book.cpp
  openings.cpp
)

# Set C++ standard and compile options
target_compile_features(main PRIVATE cxx_std_20)
target_compile_options(main PRIVATE -Wall -Wextra)
if(NOT WIN32)
  target_compile_options(main PRIVATE -ffast-math -march=native)
endif()
set_property(TARGET main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)  # Enable LTO

# Homebrew ncurses is keg-only; teach CMake where it is on Apple
if(APPLE)
  execute_process(COMMAND brew --prefix ncurses
                  OUTPUT_VARIABLE _ncurses_prefix
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  if(_ncurses_prefix)
    list(APPEND CMAKE_PREFIX_PATH "${_ncurses_prefix}")
  endif()
endif()
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
# FindCurses (CMake 4.x) provides variables, not an imported target — wrap them
add_library(Curses::Curses INTERFACE IMPORTED)
set_target_properties(Curses::Curses PROPERTIES
  INTERFACE_LINK_LIBRARIES "${CURSES_LIBRARIES}"
  INTERFACE_INCLUDE_DIRECTORIES "${CURSES_INCLUDE_DIRS}")
target_link_libraries(main PRIVATE Curses::Curses)

# libatomic probe (for TT)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
  struct S { unsigned long long a, b; };
  int main() { volatile S s{}; S r; __atomic_load((const S*)&s, &r, 5); return (int)r.a; }
" HAVE_ATOMIC_128BIT)
if(NOT HAVE_ATOMIC_128BIT)
  target_link_libraries(main PRIVATE atomic)
endif()
