CC := xcrun clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS := -framework ApplicationServices -framework CoreFoundation

TARGET := build/stoin
TEST_TARGET := build/test_steno
CORE_OBJECTS := \
	build/dictionary.o \
	build/dictionary_stack.o \
	build/format.o \
	build/gemini_pr.o \
	build/keymap.o \
	build/orthography.o \
	build/platform_macos_file_watcher.o \
	build/platform_macos_output.o \
	build/platform_macos_serial.o \
	build/platform_macos.o \
	build/retro.o \
	build/steno.o \
	build/steno_stroke.o \
	build/stb_ds_impl.o \
	build/stitch.o \
	build/text_util.o \
	build/translation_history.o \
	build/tx_bolt.o \
	build/util.o
APP_OBJECTS := build/main.o $(CORE_OBJECTS)
TEST_OBJECTS := build/test_steno.o $(CORE_OBJECTS)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(APP_OBJECTS) | build
	$(CC) $(CFLAGS) $(APP_OBJECTS) $(LDFLAGS) -o $@

$(TEST_TARGET): $(TEST_OBJECTS) | build
	$(CC) $(CFLAGS) $(TEST_OBJECTS) $(LDFLAGS) -o $@

build:
	mkdir -p $@

build/%.o: src/%.c src/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: tests/%.c src/*.h | build
	$(CC) $(CFLAGS) -I src -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf build
