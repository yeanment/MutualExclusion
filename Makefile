#CC = g++
CC = g++

CFLAGS = -Wall -g -std=c++11
LDFLAGS = -pthread

SRC = $(shell ls *.cpp)
OBJ = $(SRC:.cpp=.o)
BIN = mutex

RM = rm -f

.PHONY: all clean clear debug

all: clean build

build: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

debug: $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

clean: 
	$(RM) *.o $(BIN)

clear:
	$(RM) *.csv

