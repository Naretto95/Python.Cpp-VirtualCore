CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -O2
PROG     := core
SRC      := $(wildcard *.cpp)
OBJ      := $(SRC:.cpp=.o)

.PHONY: all clean

all: $(PROG)

$(PROG): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(PROG)
