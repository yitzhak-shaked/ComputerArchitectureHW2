# # Compiler
# CXX = g++
# # Linker
# CXXLINK = $(CXX)
# # Compiler flags
# # CXXFLAGS = -std=c++11 -Wall -Werror -pedantic-errors -DNDEBUG -pthread  # Required flags
CXXFLAGS = -g -std=c++11 -Wall -pedantic-errors -DNDEBUG -pthread  # Debug flags

# # Source files
# SRCS = $(wildcard *.cpp)
# # Header files
# HEADERS = $(wildcard *.hpp)
# # Object files
# OBJS = $(SRCS:.cpp=.o)
# # Executable name
# EXEC = cacheSim

cacheSim: cacheSim.cpp
	g++ $(CXXFLAGS) -o cacheSim cacheSim.cpp
	# g++ -o cacheSim cacheSim.cpp

.PHONY: clean
clean:
	rm -f *.o
	rm -f cacheSim
