# 用法:
#   make              本机编译
#   make cross        交叉编译 (aarch64-linux-gnu-gcc)
#   make CC=aarch64-linux-gnu-gcc

CC      ?= gcc
PKG_CONFIG ?= pkg-config
CFLAGS  = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread

GST_CFLAGS := $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-app-1.0 2>/dev/null)
GST_LIBS   := $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-app-1.0 2>/dev/null)

ifneq ($(GST_CFLAGS),)
CFLAGS += $(GST_CFLAGS)
LDFLAGS += $(GST_LIBS)
else
$(error GStreamer dev not found. Run: ./scripts/install_deps.sh  OR  sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev)
endif

SRC_DIR = src
BUILD   = build

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/v4l2_capture.c \
       $(SRC_DIR)/monitor.c \
       $(SRC_DIR)/http_server.c \
       $(SRC_DIR)/gst_recorder.c \
       $(SRC_DIR)/mediamtx_manager.c \
       $(SRC_DIR)/app_state.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS))

TARGET = capture

.PHONY: all cross clean

all: $(TARGET)

cross:
	$(MAKE) CC=aarch64-linux-gnu-gcc PKG_CONFIG=aarch64-linux-gnu-pkg-config all

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET)
