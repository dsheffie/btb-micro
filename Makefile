OBJ = cycle_1024.o cycle_128.o cycle_16384.o cycle_16.o cycle_2048.o cycle_256.o cycle_32.o cycle_4096.o cycle_4.o cycle_512.o cycle_64.o cycle_8192.o cycle_8.o btb.o
CC = gcc
CXX = g++
EXE = btbtest
OPT = -O2 
CXXFLAGS = -std=c++11 -g $(OPT)
DEP = $(OBJ:.o=.d)

.PHONY: all clean

all: $(EXE)

$(EXE) : $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIBS) -o $(EXE)

%.o: %.cc
	$(CXX) -MMD $(CXXFLAGS) -c $< 

%.o: %.s
	$(CC) -c $<

-include $(DEP)

clean:
	rm -rf $(EXE) $(OBJ) $(DEP)
