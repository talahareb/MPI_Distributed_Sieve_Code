CC = mpicc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
TARGET = pipelined_sieve

all: $(TARGET)

$(TARGET): pipelined_sieve.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean
