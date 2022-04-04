CC = g++
CXX_FLAGS := -w -Wall -Wextra -std=c++17 -ggdb
GDB = -g
REMOVE = -rf
CPP_FILE = mysh.cpp
EXE_FILE = mysh

all:
	$(CC) $(GDB) $(CXX_FLAGS) $(CPP_FILE) -o $(EXE_FILE) 
clean_e:
	rm $(REMOVE) $(EXE_FILE)
