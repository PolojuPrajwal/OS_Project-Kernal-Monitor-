CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -Iinclude
LDFLAGS := -lpthread
SRC := src/main.c src/system_info.c src/process_info.c src/process_control.c \
       src/ipc_demo.c src/memory_mgmt.c src/file_io.c src/concurrency.c
OBJ := $(SRC:.c=.o)
TARGET := kernel_monitor

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
