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

$(BUILD)/test_compiler_rt: target/compiler_rt.c tests/test_compiler_rt.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) target/compiler_rt.c tests/test_compiler_rt.c -o $@

$(BUILD)/test_board: board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c include/ou_ns7520_mem.h include/ou_ups_link.h include/ou_flash_policy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c -o $@

$(BUILD)/test_ipv4_wire: src/ipv4_wire.c tests/test_ipv4_wire.c include/ou_ipv4_wire.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/ipv4_wire.c tests/test_ipv4_wire.c -o $@

$(BUILD)/test_vlan: src/vlan.c src/ipv4_wire.c tests/test_vlan.c include/ou_vlan.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/vlan.c src/ipv4_wire.c tests/test_vlan.c -o $@

$(BUILD)/test_ethernet_manager: board/ns7520_dma.c board/ns7520_mac.c src/ipv4_wire.c src/vlan.c target/ethernet_path.c target/ethernet_manager.c tests/test_ethernet_manager.c include/ou_ethernet_manager.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_dma.c board/ns7520_mac.c src/ipv4_wire.c src/vlan.c target/ethernet_path.c target/ethernet_manager.c tests/test_ethernet_manager.c -o $@

$(BUILD)/test_ns7520_mac: board/ns7520_mac.c tests/test_ns7520_mac.c include/ou_ns7520_mac.h include/ou_ns7520_dma.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_mac.c tests/test_ns7520_mac.c -o $@

$(BUILD)/test_ethernet_path: board/ns7520_dma.c src/ipv4_wire.c src/vlan.c target/ethernet_path.c tests/test_ethernet_path.c include/ou_ethernet_path.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_dma.c src/ipv4_wire.c src/vlan.c target/ethernet_path.c tests/test_ethernet_path.c -o $@

$(BUILD)/test_ns7520_dma: board/ns7520_dma.c tests/test_ns7520_dma.c include/ou_ns7520_dma.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_dma.c tests/test_ns7520_dma.c -o $@

$(BUILD)/test_ns7520_phy: board/ns7520_phy.c tests/test_ns7520_phy.c include/ou_ns7520_phy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_phy.c tests/test_ns7520_phy.c -o $@

$(BUILD)/test_board_probe: board/board_probe.c board/ns7520_mem.c tests/test_board_probe.c include/ou_board_probe.h include/ou_ns7520_mem.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/board_probe.c board/ns7520_mem.c tests/test_board_probe.c -o $@

$(BUILD)/test_http_wire: src/core.c src/schema.c src/provider.c src/http_wire.c tests/test_http_wire.c include/ou_http_wire.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/http_wire.c tests/test_http_wire.c -o $@

$(BUILD)/test_native_service: src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c target/service.c tests/test_native_service.c include/ou_native_service.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c target/service.c tests/test_native_service.c -o $@

$(BUILD)/test_telemetry_guard: src/core.c src/schema.c src/provider.c src/telemetry_guard.c tests/test_telemetry_guard.c include/ou_telemetry_guard.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c tests/test_telemetry_guard.c -o $@

$(BUILD)/test_journal: board/config_journal.c tests/test_config_journal.c include/ou_config_journal.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/config_journal.c tests/test_config_journal.c -o $@

test: all $(BUILD)/test_core $(BUILD)/test_provider $(BUILD)/test_board $(BUILD)/test_journal $(BUILD)/test_telemetry_guard $(BUILD)/test_board_probe $(BUILD)/test_http_wire $(BUILD)/test_native_service $(BUILD)/test_compiler_rt $(BUILD)/test_ns7520_phy $(BUILD)/test_ipv4_wire $(BUILD)/test_ns7520_dma $(BUILD)/test_ethernet_path $(BUILD)/test_ns7520_mac $(BUILD)/test_vlan $(BUILD)/test_ethernet_manager
	./$(BUILD)/test_core
	./$(BUILD)/test_provider
	./$(BUILD)/test_board
	./$(BUILD)/test_journal
	./$(BUILD)/test_telemetry_guard
	./$(BUILD)/test_board_probe
	./$(BUILD)/test_http_wire
	./$(BUILD)/test_native_service
	./$(BUILD)/test_compiler_rt
	./$(BUILD)/test_ns7520_phy
	./$(BUILD)/test_ipv4_wire
	./$(BUILD)/test_ns7520_dma
	./$(BUILD)/test_ethernet_path
	./$(BUILD)/test_ns7520_mac
	./$(BUILD)/test_vlan
	./$(BUILD)/test_ethernet_manager
	python3 -m unittest discover -s tests -p 'test_*.py' -v

arm-check: | $(BUILD)
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/core.c -o $(BUILD)/core-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/schema.c -o $(BUILD)/schema-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/provider.c -o $(BUILD)/provider-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/core-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/schema-armeb-arm7tdmi.o
	python3 tools/verify_arm.py $(BUILD)/provider-armeb-arm7tdmi.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/telemetry_guard.c -o $(BUILD)/telemetry-guard-armeb.o
	python3 tools/verify_arm.py $(BUILD)/telemetry-guard-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_mem.c -o $(BUILD)/ns7520-mem-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ups_link.c -o $(BUILD)/ups-link-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/flash_policy.c -o $(BUILD)/flash-policy-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-mem-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ups-link-armeb.o
	python3 tools/verify_arm.py $(BUILD)/flash-policy-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/config_journal.c -o $(BUILD)/config-journal-armeb.o
	python3 tools/verify_arm.py $(BUILD)/config-journal-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/board_probe.c -o $(BUILD)/board-probe-armeb.o
	python3 tools/verify_arm.py $(BUILD)/board-probe-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_phy.c -o $(BUILD)/ns7520-phy-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-phy-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/ipv4_wire.c -o $(BUILD)/ipv4-wire-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ipv4-wire-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_dma.c -o $(BUILD)/ns7520-dma-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-dma-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/ethernet_path.c -o $(BUILD)/ethernet-path-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ethernet-path-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_mac.c -o $(BUILD)/ns7520-mac-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-mac-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/vlan.c -o $(BUILD)/vlan-armeb.o
	python3 tools/verify_arm.py $(BUILD)/vlan-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/ethernet_manager.c -o $(BUILD)/ethernet-manager-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ethernet-manager-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/http_wire.c -o $(BUILD)/http-wire-armeb.o
	python3 tools/verify_arm.py $(BUILD)/http-wire-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/service.c -o $(BUILD)/native-service-armeb.o
	python3 tools/verify_arm.py $(BUILD)/native-service-armeb.o

clean:
	rm -rf $(BUILD)

# Research-only complete ELF, never a flashable or boot-qualified BIN.
arm-link-check: arm-check
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/runtime.c -o $(BUILD)/runtime-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/reset.S -o $(BUILD)/reset-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -c target/compiler_rt.c -o $(BUILD)/compiler-rt-armeb.o
	ld.lld -flavor gnu -m armelfb --build-id=none -T target/linker.ld $(BUILD)/reset-armeb.o $(BUILD)/runtime-armeb.o $(BUILD)/core-armeb-arm7tdmi.o $(BUILD)/schema-armeb-arm7tdmi.o $(BUILD)/provider-armeb-arm7tdmi.o $(BUILD)/telemetry-guard-armeb.o $(BUILD)/ns7520-mem-armeb.o $(BUILD)/ups-link-armeb.o $(BUILD)/flash-policy-armeb.o $(BUILD)/config-journal-armeb.o $(BUILD)/board-probe-armeb.o $(BUILD)/ns7520-phy-armeb.o $(BUILD)/ipv4-wire-armeb.o $(BUILD)/ns7520-dma-armeb.o $(BUILD)/ethernet-path-armeb.o $(BUILD)/ns7520-mac-armeb.o $(BUILD)/vlan-armeb.o $(BUILD)/ethernet-manager-armeb.o $(BUILD)/http-wire-armeb.o $(BUILD)/native-service-armeb.o $(BUILD)/compiler-rt-armeb.o -o $(BUILD)/native-research-NOT-FLASHABLE.elf
	llvm-objcopy -O binary $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat
	python3 tools/verify_native_image.py $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat

.PHONY: alpha-lab-image
# Build an OEM-envelope alpha research BIN for separate recovery-hardware tests.
# This has NO hardware drivers and is not a safe live-card update. Not default.
alpha-lab-image: footprint-check
	python3 tools/package_alpha.py $(BUILD)/native-research-NOT-FLASHABLE.dat --output $(BUILD)/H910P-0.6.0-alpha.1-RESEARCH-DO-NOT-FLASH.bin --manifest $(BUILD)/alpha-research-manifest.json --acknowledge-research-only

# Research size bounds are provisional until the actual H910P memory and flash
# layout is verified. This does NOT certify a deployable firmware image.
footprint-check: arm-link-check
	python3 tools/audit_footprint.py $(BUILD)/native-research-NOT-FLASHABLE.elf $(BUILD)/native-research-NOT-FLASHABLE.dat --policy resource-policy.json --report $(BUILD)/resource-report.json

# Host-only memory safety instrumentation for native telemetry, NS7520 and UPS link
# modules. This does not connect to physical hardware or flash firmware.
board-sanitize: | $(BUILD)
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c -o $(BUILD)/test_board_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/config_journal.c tests/test_config_journal.c -o $(BUILD)/test_journal_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_journal_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c tests/test_telemetry_guard.c -o $(BUILD)/test_telemetry_guard_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_telemetry_guard_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/board_probe.c board/ns7520_mem.c tests/test_board_probe.c -o $(BUILD)/test_board_probe_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_probe_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/http_wire.c tests/test_http_wire.c -o $(BUILD)/test_http_wire_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_http_wire_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c target/service.c tests/test_native_service.c -o $(BUILD)/test_native_service_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_native_service_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined target/compiler_rt.c tests/test_compiler_rt.c -o $(BUILD)/test_compiler_rt_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_compiler_rt_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_phy.c tests/test_ns7520_phy.c -o $(BUILD)/test_ns7520_phy_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_phy_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/ipv4_wire.c tests/test_ipv4_wire.c -o $(BUILD)/test_ipv4_wire_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ipv4_wire_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_dma.c tests/test_ns7520_dma.c -o $(BUILD)/test_ns7520_dma_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_dma_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_dma.c src/ipv4_wire.c src/vlan.c target/ethernet_path.c tests/test_ethernet_path.c -o $(BUILD)/test_ethernet_path_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ethernet_path_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_mac.c tests/test_ns7520_mac.c -o $(BUILD)/test_ns7520_mac_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_mac_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/vlan.c src/ipv4_wire.c tests/test_vlan.c -o $(BUILD)/test_vlan_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_vlan_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_dma.c board/ns7520_mac.c src/ipv4_wire.c src/vlan.c target/ethernet_path.c target/ethernet_manager.c tests/test_ethernet_manager.c -o $(BUILD)/test_ethernet_manager_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ethernet_manager_sanitized
