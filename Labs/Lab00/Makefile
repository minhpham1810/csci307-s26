# Variables for compiler and flags
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -pedantic -g

# The target executable name
TARGET = dynamic_array

# The default rule to build the program
all: $(TARGET)

$(TARGET): dynamic_array.c
	$(CC) $(CFLAGS) dynamic_array.c -o $(TARGET)

# Rule to clean up the directory
clean:
	rm -f $(TARGET)