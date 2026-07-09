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
ODIN ?= odin
SOURCE_DIR := src
CHECK_TARGETS ?= linux_arm64 linux_amd64 windows_amd64
RELEASE_FLAGS ?= -o:speed -disable-assert

EXE_EXT :=
ifeq ($(PLATFORM),windows)
EXE_EXT := .exe
else ifeq ($(PLATFORM),macos)
else ifeq ($(PLATFORM),linux)
else
$(error Unsupported PLATFORM '$(PLATFORM)'; use PLATFORM=macos, PLATFORM=linux, or PLATFORM=windows)
endif

BUILD_DIR := build/$(PLATFORM)
RELEASE_DIR := $(BUILD_DIR)/release
TARGET := $(BUILD_DIR)/stoin$(EXE_EXT)
RELEASE_TARGET := $(RELEASE_DIR)/stoin$(EXE_EXT)
SOURCES := $(wildcard $(SOURCE_DIR)/*.odin)

.PHONY: all check clean go-test linux macos release run srs-web test windows

all: $(TARGET)

macos:
	$(MAKE) PLATFORM=macos

linux:
	$(MAKE) PLATFORM=linux

windows:
	$(MAKE) PLATFORM=windows

release: $(RELEASE_TARGET)

$(TARGET): $(SOURCES) Makefile | $(BUILD_DIR)
	$(ODIN) build $(SOURCE_DIR) -out:$@

$(RELEASE_TARGET): $(SOURCES) Makefile | $(RELEASE_DIR)
	$(ODIN) build $(SOURCE_DIR) $(RELEASE_FLAGS) -out:$@

$(BUILD_DIR):
	mkdir -p $@

$(RELEASE_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

srs-web:
	go run ./cmd/stoin-srs-web

test:
	$(ODIN) test $(SOURCE_DIR)
	$(MAKE) go-test

go-test:
	go test ./...

check:
	$(ODIN) check $(SOURCE_DIR)
	@for target in $(CHECK_TARGETS); do \
		echo "$(ODIN) check $(SOURCE_DIR) -target:$$target"; \
		$(ODIN) check $(SOURCE_DIR) -target:$$target || exit $$?; \
	done

clean:
	rm -rf build
