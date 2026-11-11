CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude
BUILD = build

.PHONY: all clean test arm-check
all: $(BUILD)/openh910p-sim

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/openh910p-sim: src/core.c host/server.c include/openh910p.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c host/server.c -o $@

$(BUILD)/test_core: src/core.c tests/test_core.c include/openh910p.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c tests/test_core.c -o $@

test: all $(BUILD)/test_core
	./$(BUILD)/test_core
	python3 -m unittest discover -s tests -p 'test_*.py' -v

arm-check: | $(BUILD)
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/core.c -o $(BUILD)/core-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/core-armeb-arm7tdmi.o

clean:
	rm -rf $(BUILD)
