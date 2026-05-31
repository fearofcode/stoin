CC := xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS := -framework ApplicationServices -framework CoreFoundation

TARGET := build/stoin
SOURCES := src/main.c src/platform_macos.c src/steno.c src/stb_ds_impl.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS) | build
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

build:
	mkdir -p $@

build/%.o: src/%.c src/platform.h src/steno.h | build
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
