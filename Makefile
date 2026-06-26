UNAME_S := $(shell uname -s)
HOST_PLATFORM := unsupported
ifeq ($(UNAME_S),Darwin)
HOST_PLATFORM := macos
else ifeq ($(UNAME_S),Linux)
HOST_PLATFORM := linux
endif

PLATFORM ?= $(HOST_PLATFORM)

ifeq ($(origin CC),default)
ifeq ($(PLATFORM),macos)
CC := xcrun clang
else
CC := cc
endif
endif

BASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
BASE_RELEASE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O3 -DNDEBUG
PLATFORM_CFLAGS :=
PLATFORM_LDFLAGS :=

COMMON_SOURCES := \
	src/dictionary.c \
	src/dictionary_stack.c \
	src/format.c \
	src/gemini_pr.c \
	src/keymap.c \
	src/orthography.c \
	src/raw_serial.c \
	src/retro.c \
	src/runtime_config.c \
	src/steno.c \
	src/steno_stroke.c \
	src/stroke_merge.c \
	src/stb_ds_impl.c \
	src/stitch.c \
	src/text_util.c \
	src/translation_history.c \
	src/translation_match.c \
	src/tx_bolt.c \
	src/tx_bolt_multiple.c \
	src/util.c

ifeq ($(PLATFORM),macos)
PLATFORM_LDFLAGS += -framework ApplicationServices -framework CoreFoundation
PLATFORM_SOURCES := \
	src/platform_macos.c \
	src/platform_macos_file_watcher.c \
	src/platform_macos_output.c \
	src/platform_posix_serial.c
else ifeq ($(PLATFORM),linux)
PLATFORM_CFLAGS += -D_DEFAULT_SOURCE
PLATFORM_SOURCES := \
	src/platform_linux.c \
	src/platform_linux_file_watcher.c \
	src/platform_linux_output.c \
	src/platform_posix_serial.c
else
$(error Unsupported PLATFORM '$(PLATFORM)'; use PLATFORM=macos or PLATFORM=linux)
endif

CFLAGS := $(BASE_CFLAGS) $(PLATFORM_CFLAGS)
RELEASE_CFLAGS := $(BASE_RELEASE_CFLAGS) $(PLATFORM_CFLAGS)
LDFLAGS := $(PLATFORM_LDFLAGS)

BUILD_DIR := build/$(PLATFORM)
RELEASE_DIR := $(BUILD_DIR)/release
TARGET := $(BUILD_DIR)/stoin
RELEASE_TARGET := $(RELEASE_DIR)/stoin
TEST_TARGET := $(BUILD_DIR)/test_steno

CORE_SOURCES := $(COMMON_SOURCES) $(PLATFORM_SOURCES)
CORE_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
APP_OBJECTS := $(BUILD_DIR)/main.o $(CORE_OBJECTS)
TEST_OBJECTS := $(BUILD_DIR)/test_steno.o $(CORE_OBJECTS)
RELEASE_CORE_OBJECTS := $(patsubst src/%.c,$(RELEASE_DIR)/%.o,$(CORE_SOURCES))
RELEASE_APP_OBJECTS := $(RELEASE_DIR)/main.o $(RELEASE_CORE_OBJECTS)

.PHONY: all clean linux macos release run srs-web test

all: $(TARGET)

macos:
	$(MAKE) PLATFORM=macos

linux:
	$(MAKE) PLATFORM=linux

release: $(RELEASE_TARGET)

$(TARGET): $(APP_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(APP_OBJECTS) $(LDFLAGS) -o $@

$(RELEASE_TARGET): $(RELEASE_APP_OBJECTS) | $(RELEASE_DIR)
	$(CC) $(RELEASE_CFLAGS) $(RELEASE_APP_OBJECTS) $(LDFLAGS) -o $@

$(TEST_TARGET): $(TEST_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR):
	mkdir -p $@

$(RELEASE_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c src/*.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(RELEASE_DIR)/%.o: src/%.c src/*.h | $(RELEASE_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: tests/%.c src/*.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I src -c $< -o $@

run: $(TARGET)
	./$(TARGET)

srs-web:
	go run ./cmd/stoin-srs-web

test: $(TEST_TARGET)
	./$(TEST_TARGET)
	go test ./...

clean:
	rm -rf build
