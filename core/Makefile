CP=cp
CC=g++ -Wall -std=c++17 
RM=rm 
PROG=core
SRC=$(wildcard *.cpp)
HEAD=$(SRC:.cpp=.h)
OBJ=$(SRC:.cpp=.o)
LSTDC-FLAG=-lstdc++fs

all: run

$(PROG) : $(OBJ)
	$(CC) -o $@ $^ $(LSTDC-FLAG)

%.o : %.cpp
	$(CC) -c $^ -o $@ 

run: $(PROG)

clean :
	$(RM) *o 
	$(RM) $(PROG)