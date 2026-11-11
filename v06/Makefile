CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude
BUILD = build

.PHONY: all clean test arm-check arm-link-check
all: $(BUILD)/openh910p-sim

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/openh910p-sim: src/core.c src/schema.c src/provider.c host/server.c include/openh910p.h include/ou_schema.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c host/server.c -o $@

$(BUILD)/test_core: src/core.c src/schema.c src/provider.c tests/test_core.c include/openh910p.h include/ou_schema.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c tests/test_core.c -o $@

$(BUILD)/test_provider: src/core.c src/schema.c src/provider.c tests/test_provider.c include/openh910p.h include/ou_schema.h include/ou_provider.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c tests/test_provider.c -o $@

test: all $(BUILD)/test_core $(BUILD)/test_provider
	./$(BUILD)/test_core
	./$(BUILD)/test_provider
	python3 -m unittest discover -s tests -p 'test_*.py' -v

arm-check: | $(BUILD)
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/core.c -o $(BUILD)/core-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/schema.c -o $(BUILD)/schema-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/provider.c -o $(BUILD)/provider-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/core-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/schema-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/provider-armeb-arm7tdmi.o

clean:
	rm -rf $(BUILD)

# Research-only complete ELF, never a flashable or boot-qualified BIN.
arm-link-check: arm-check
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/runtime.c -o $(BUILD)/runtime-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/reset.S -o $(BUILD)/reset-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -c target/compiler_rt.c -o $(BUILD)/compiler-rt-armeb.o
	ld.lld -flavor gnu -m armelfb --build-id=none -T target/linker.ld $(BUILD)/reset-armeb.o $(BUILD)/runtime-armeb.o $(BUILD)/core-armeb-arm7tdmi.o $(BUILD)/schema-armeb-arm7tdmi.o $(BUILD)/provider-armeb-arm7tdmi.o $(BUILD)/compiler-rt-armeb.o -o $(BUILD)/native-research-NOT-FLASHABLE.elf
	llvm-objcopy -O binary $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat
	python3 tools/verify_native_image.py $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat
