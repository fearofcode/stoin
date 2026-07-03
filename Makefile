HOST_PLATFORM := unsupported
ifeq ($(OS),Windows_NT)
UNAME_S := Windows_NT
HOST_PLATFORM := windows
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
HOST_PLATFORM := macos
else ifeq ($(UNAME_S),Linux)
HOST_PLATFORM := linux
endif
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
EXE_EXT :=

COMMON_SOURCES := \
	src/dictionary.c \
	src/dictionary_stack.c \
	src/format.c \
	src/gemini_pr.c \
	src/keymap.c \
	src/orthography.c \
	src/phrasing.c \
	src/raw_serial.c \
	src/retro.c \
	src/runtime_config.c \
	src/stentura.c \
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

VENDOR_SOURCES := \
	third_party/cjson/cJSON.c

ifeq ($(PLATFORM),macos)
PLATFORM_LDFLAGS += -framework ApplicationServices -framework CoreFoundation -framework IOKit
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
else ifeq ($(PLATFORM),windows)
PLATFORM_CFLAGS += -D_WIN32_WINNT=0x0601
PLATFORM_LDFLAGS += -luser32 -lsetupapi -ladvapi32
PLATFORM_SOURCES := \
	src/platform_windows.c
EXE_EXT := .exe
else
$(error Unsupported PLATFORM '$(PLATFORM)'; use PLATFORM=macos, PLATFORM=linux, or PLATFORM=windows)
endif

CFLAGS := $(BASE_CFLAGS) $(PLATFORM_CFLAGS)
RELEASE_CFLAGS := $(BASE_RELEASE_CFLAGS) $(PLATFORM_CFLAGS)
LDFLAGS := $(PLATFORM_LDFLAGS)

BUILD_DIR := build/$(PLATFORM)
RELEASE_DIR := $(BUILD_DIR)/release
TARGET := $(BUILD_DIR)/stoin$(EXE_EXT)
RELEASE_TARGET := $(RELEASE_DIR)/stoin$(EXE_EXT)
TEST_TARGET := $(BUILD_DIR)/test_steno$(EXE_EXT)

CORE_SOURCES := $(COMMON_SOURCES) $(PLATFORM_SOURCES)
CORE_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES)) \
	$(patsubst third_party/%.c,$(BUILD_DIR)/third_party/%.o,$(VENDOR_SOURCES))
APP_OBJECTS := $(BUILD_DIR)/main.o $(CORE_OBJECTS)
TEST_OBJECTS := $(BUILD_DIR)/test_steno.o $(CORE_OBJECTS)
RELEASE_CORE_OBJECTS := $(patsubst src/%.c,$(RELEASE_DIR)/%.o,$(CORE_SOURCES)) \
	$(patsubst third_party/%.c,$(RELEASE_DIR)/third_party/%.o,$(VENDOR_SOURCES))
RELEASE_APP_OBJECTS := $(RELEASE_DIR)/main.o $(RELEASE_CORE_OBJECTS)

.PHONY: all clean linux macos release run srs-web test windows

all: $(TARGET)

macos:
	$(MAKE) PLATFORM=macos

linux:
	$(MAKE) PLATFORM=linux

windows:
	$(MAKE) PLATFORM=windows

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

$(BUILD_DIR)/third_party/cjson:
	mkdir -p $@

$(RELEASE_DIR)/third_party/cjson:
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c src/*.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(RELEASE_DIR)/%.o: src/%.c src/*.h | $(RELEASE_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/third_party/cjson/%.o: third_party/cjson/%.c third_party/cjson/%.h | $(BUILD_DIR)/third_party/cjson
	$(CC) $(CFLAGS) -c $< -o $@

$(RELEASE_DIR)/third_party/cjson/%.o: third_party/cjson/%.c third_party/cjson/%.h | $(RELEASE_DIR)/third_party/cjson
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
