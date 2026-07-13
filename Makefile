# 用法:
#   make              本机编译
#   make cross        交叉编译 (aarch64-linux-gnu-gcc)
#   make CC=aarch64-linux-gnu-gcc

CC      ?= gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread

SRC_DIR = src
BUILD   = build

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/v4l2_capture.c \
       $(SRC_DIR)/monitor.c \
       $(SRC_DIR)/http_server.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

TARGET = capture

.PHONY: all cross clean

all: $(TARGET)

cross:
	$(MAKE) CC=aarch64-linux-gnu-gcc all

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET)
