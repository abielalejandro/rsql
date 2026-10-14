CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_GNU_SOURCE
TARGET = db
SRCS = main.c storage.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET) test.db

clean:
	rm -f $(TARGET) *.db

.PHONY: all run clean