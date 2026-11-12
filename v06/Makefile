CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude
BUILD = build

.PHONY: all clean test arm-check arm-link-check footprint-check board-sanitize
all: $(BUILD)/openh910p-sim

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/openh910p-sim: src/core.c src/schema.c src/provider.c host/server.c include/openh910p.h include/ou_schema.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c host/server.c -o $@

$(BUILD)/test_core: src/core.c src/schema.c src/provider.c tests/test_core.c include/openh910p.h include/ou_schema.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c tests/test_core.c -o $@

$(BUILD)/test_provider: src/core.c src/schema.c src/provider.c tests/test_provider.c include/openh910p.h include/ou_schema.h include/ou_provider.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c tests/test_provider.c -o $@

$(BUILD)/test_board: board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c include/ou_ns7520_mem.h include/ou_ups_link.h include/ou_flash_policy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c -o $@

$(BUILD)/test_journal: board/config_journal.c tests/test_config_journal.c include/ou_config_journal.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/config_journal.c tests/test_config_journal.c -o $@

test: all $(BUILD)/test_core $(BUILD)/test_provider $(BUILD)/test_board $(BUILD)/test_journal
	./$(BUILD)/test_core
	./$(BUILD)/test_provider
	./$(BUILD)/test_board
	./$(BUILD)/test_journal
	python3 -m unittest discover -s tests -p 'test_*.py' -v

arm-check: | $(BUILD)
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/core.c -o $(BUILD)/core-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/schema.c -o $(BUILD)/schema-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/provider.c -o $(BUILD)/provider-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/core-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/schema-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/provider-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_mem.c -o $(BUILD)/ns7520-mem-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ups_link.c -o $(BUILD)/ups-link-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/flash_policy.c -o $(BUILD)/flash-policy-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-mem-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ups-link-armeb.o
	python3 tools/verify_arm.py $(BUILD)/flash-policy-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/config_journal.c -o $(BUILD)/config-journal-armeb.o
	python3 tools/verify_arm.py $(BUILD)/config-journal-armeb.o

clean:
	rm -rf $(BUILD)

# Research-only complete ELF, never a flashable or boot-qualified BIN.
arm-link-check: arm-check
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/runtime.c -o $(BUILD)/runtime-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/reset.S -o $(BUILD)/reset-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -c target/compiler_rt.c -o $(BUILD)/compiler-rt-armeb.o
	ld.lld -flavor gnu -m armelfb --build-id=none -T target/linker.ld $(BUILD)/reset-armeb.o $(BUILD)/runtime-armeb.o $(BUILD)/core-armeb-arm7tdmi.o $(BUILD)/schema-armeb-arm7tdmi.o $(BUILD)/provider-armeb-arm7tdmi.o $(BUILD)/ns7520-mem-armeb.o $(BUILD)/ups-link-armeb.o $(BUILD)/flash-policy-armeb.o $(BUILD)/config-journal-armeb.o $(BUILD)/compiler-rt-armeb.o -o $(BUILD)/native-research-NOT-FLASHABLE.elf
	llvm-objcopy -O binary $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat
	python3 tools/verify_native_image.py $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat

# Research size bounds are provisional until the actual H910P memory and flash
# layout is verified. This does NOT certify a deployable firmware image.
footprint-check: arm-link-check
	python3 tools/audit_footprint.py $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat --policy resource-policy.json --report $(BUILD)/resource-report.json

# Host-only memory safety instrumentation for the pure NS7520 and UPS link
# modules. This does not connect to physical hardware or flash firmware.
board-sanitize: | $(BUILD)
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c -o $(BUILD)/test_board_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/config_journal.c tests/test_config_journal.c -o $(BUILD)/test_journal_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_journal_sanitized
