CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = kernel
OBJS = kernel.o machine.o process.o clock_sys.o timer.o memory.o loader.o

# Default target
all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile each module
kernel.o: kernel.c machine.h process.h clock_sys.h timer.h memory.h loader.h
	$(CC) $(CFLAGS) -c kernel.c

machine.o: machine.c machine.h process.h
	$(CC) $(CFLAGS) -c machine.c

process.o: process.c process.h clock_sys.h
	$(CC) $(CFLAGS) -c process.c

clock_sys.o: clock_sys.c clock_sys.h
	$(CC) $(CFLAGS) -c clock_sys.c

timer.o: timer.c timer.h clock_sys.h
	$(CC) $(CFLAGS) -c timer.c

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c memory.c

loader.o: loader.c loader.h memory.h process.h
	$(CC) $(CFLAGS) -c loader.c

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET) *.o

# Phony targets
.PHONY: all clean
