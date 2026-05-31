CC := xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS := -framework ApplicationServices -framework CoreFoundation

TARGET := build/stoin
SOURCES := \
	src/main.c \
	src/dictionary.c \
	src/platform_macos.c \
	src/steno.c \
	src/steno_stroke.c \
	src/stb_ds_impl.c \
	src/util.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS) | build
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

build:
	mkdir -p $@

build/%.o: src/%.c src/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
