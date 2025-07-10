CC = aarch64-none-linux-gnu-gcc

CFLAGS = -Wall -Wextra -pthread -std=gnu99 -g
TARGET = test

SOURCES = main.c motor.c task.c serial.c

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean

debug: CFLAGS += -DDEBUG -O0
debug:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)