CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -O0 -g
LIBS = -ldl -lbacktrace
TARGET = memory_test
SOURCES = memory_tracker.cpp test.cpp
HEADERS = memory_tracker.h

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean