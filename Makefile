CC := /opt/amiga/bin/m68k-amigaos-gcc
CFLAGS ?= -Os -Wall -Wextra -fomit-frame-pointer -mcrt=nix13 -fno-builtin
LDFLAGS ?= -mcrt=nix13
BUILD_DIR := build
TARGET := $(BUILD_DIR)/Weather13
OBJS := \
	$(BUILD_DIR)/weather13.o \
	$(BUILD_DIR)/weather_data.o \
	$(BUILD_DIR)/weather_net.o \
	$(BUILD_DIR)/i18n.o \
	$(BUILD_DIR)/ui.o \
	$(BUILD_DIR)/windrose.o \
	$(BUILD_DIR)/weather_icons.o

CPPFLAGS += -I. -Isrc -Iinclude -DAMITCP13_OS13

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/weather_icons.o: weather_icons.c weather_icons.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ weather_icons.c

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
