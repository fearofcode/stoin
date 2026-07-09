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
ODIN_SOURCE_DIR := odin/stoin
ODIN_CHECK_TARGETS ?= linux_arm64 linux_amd64 windows_amd64

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
ODIN_BUILD_DIR := $(BUILD_DIR)/odin
ODIN_TARGET := $(ODIN_BUILD_DIR)/stoin$(EXE_EXT)
ODIN_RELEASE_TARGET := $(RELEASE_DIR)/odin/stoin$(EXE_EXT)
ODIN_SOURCES := $(wildcard $(ODIN_SOURCE_DIR)/*.odin)

.PHONY: all clean go-test linux macos odin odin-check odin-release odin-test release run srs-web test windows

all: $(TARGET)

macos:
	$(MAKE) PLATFORM=macos

linux:
	$(MAKE) PLATFORM=linux

windows:
	$(MAKE) PLATFORM=windows

release: $(RELEASE_TARGET)

odin: $(ODIN_TARGET)

odin-release: $(ODIN_RELEASE_TARGET)

$(TARGET): $(ODIN_SOURCES) Makefile | $(BUILD_DIR)
	$(ODIN) build $(ODIN_SOURCE_DIR) -out:$@

$(RELEASE_TARGET): $(ODIN_SOURCES) Makefile | $(RELEASE_DIR)
	$(ODIN) build $(ODIN_SOURCE_DIR) -o:speed -out:$@

$(ODIN_TARGET): $(ODIN_SOURCES) Makefile | $(ODIN_BUILD_DIR)
	$(ODIN) build $(ODIN_SOURCE_DIR) -out:$@

$(ODIN_RELEASE_TARGET): $(ODIN_SOURCES) Makefile | $(RELEASE_DIR)/odin
	$(ODIN) build $(ODIN_SOURCE_DIR) -o:speed -out:$@

$(BUILD_DIR):
	mkdir -p $@

$(RELEASE_DIR):
	mkdir -p $@

$(ODIN_BUILD_DIR):
	mkdir -p $@

$(RELEASE_DIR)/odin:
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

srs-web:
	go run ./cmd/stoin-srs-web

test: odin-test go-test

odin-test:
	$(ODIN) test $(ODIN_SOURCE_DIR)

go-test:
	go test ./...

odin-check:
	$(ODIN) check $(ODIN_SOURCE_DIR)
	@for target in $(ODIN_CHECK_TARGETS); do \
		echo "$(ODIN) check $(ODIN_SOURCE_DIR) -target:$$target"; \
		$(ODIN) check $(ODIN_SOURCE_DIR) -target:$$target || exit $$?; \
	done

clean:
	rm -rf build
