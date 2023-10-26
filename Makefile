CC := gcc
SRC := $(shell find src -type f)
CFLAGS := -Wall -Wextra --pedantic
INCLUDE := -Iinclude

all:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) -o main


