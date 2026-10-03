CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_GNU_SOURCE
TARGET = db
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
