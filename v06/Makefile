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

$(BUILD)/test_shut_request: board/shut_request.c tests/test_shut_request.c include/ou_shut_request.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/shut_request.c tests/test_shut_request.c -o $@

$(BUILD)/test_hid_telemetry: board/hid_telemetry.c board/hid_report.c src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_hid_telemetry.c include/ou_hid_telemetry.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/hid_telemetry.c board/hid_report.c src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_hid_telemetry.c -o $@

$(BUILD)/test_hid_report: board/hid_report.c tests/test_hid_report.c include/ou_hid_report.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/hid_report.c tests/test_hid_report.c -o $@

$(BUILD)/test_shut_session: board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_session.c include/ou_shut_session.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_session.c -o $@

$(BUILD)/test_ups_monitor: board/ups_monitor.c board/hid_telemetry.c board/hid_report.c board/shut_uart_link.c board/shut_uart_tx.c board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_ups_monitor.c include/ou_ups_monitor.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ups_monitor.c board/hid_telemetry.c board/hid_report.c board/shut_uart_link.c board/shut_uart_tx.c board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_ups_monitor.c -o $@

$(BUILD)/test_shut_uart_link: board/shut_uart_link.c board/shut_uart_tx.c board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_link.c include/ou_shut_uart_link.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/shut_uart_link.c board/shut_uart_tx.c board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_link.c -o $@

$(BUILD)/test_shut_uart_tx: board/shut_uart_tx.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_tx.c include/ou_shut_uart_tx.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/shut_uart_tx.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_tx.c -o $@

$(BUILD)/test_shut_uart_bridge: board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_bridge.c include/ou_shut_uart_bridge.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_bridge.c -o $@

$(BUILD)/test_ns7520_serial_rx: board/ns7520_serial_rx.c tests/test_ns7520_serial_rx.c include/ou_ns7520_serial_rx.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_serial_rx.c tests/test_ns7520_serial_rx.c -o $@

$(BUILD)/test_shut_rx: board/shut_rx.c board/ups_link.c tests/test_shut_rx.c include/ou_shut_rx.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/shut_rx.c board/ups_link.c tests/test_shut_rx.c -o $@

$(BUILD)/test_board: board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c include/ou_ns7520_mem.h include/ou_ups_link.h include/ou_flash_policy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c -o $@

$(BUILD)/test_ipv4_wire: src/ipv4_wire.c tests/test_ipv4_wire.c include/ou_ipv4_wire.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/ipv4_wire.c tests/test_ipv4_wire.c -o $@

$(BUILD)/test_vlan_record: src/vlan_record.c src/vlan.c src/ipv4_wire.c board/config_journal.c tests/test_vlan_record.c include/ou_vlan_record.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/vlan_record.c src/vlan.c src/ipv4_wire.c board/config_journal.c tests/test_vlan_record.c -o $@

$(BUILD)/test_ns7520_timer: board/ns7520_timer.c tests/test_ns7520_timer.c include/ou_ns7520_timer.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_timer.c tests/test_ns7520_timer.c -o $@

$(BUILD)/test_phy_link: board/ns7520_phy.c tests/test_phy_link.c include/ou_ns7520_phy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_phy.c tests/test_phy_link.c -o $@

$(BUILD)/test_phy_discover: board/ns7520_phy.c tests/test_phy_discover.c include/ou_ns7520_phy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_phy.c tests/test_phy_discover.c -o $@

$(BUILD)/test_hal_timer: board/ns7520_hal.c board/ns7520_timer.c tests/test_hal_timer.c include/ou_ns7520_hal.h include/ou_ns7520_timer.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_hal.c board/ns7520_timer.c tests/test_hal_timer.c -o $@

$(BUILD)/test_ns7520_hal: board/ns7520_hal.c tests/test_ns7520_hal.c include/ou_ns7520_hal.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_hal.c tests/test_ns7520_hal.c -o $@

$(BUILD)/test_board_runtime: board/ns7520_hal.c board/ns7520_phy.c board/board_probe.c board/ns7520_mem.c board/board_runtime.c tests/test_board_runtime.c include/ou_board_runtime.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_hal.c board/ns7520_phy.c board/board_probe.c board/ns7520_mem.c board/board_runtime.c tests/test_board_runtime.c -o $@

$(BUILD)/test_tcp_native_http: src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/tcp_engine.c src/tcp_ingress.c src/vlan.c src/ipv4_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c target/tcp_adapter.c tests/test_tcp_native_http.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/tcp_engine.c src/tcp_ingress.c src/vlan.c src/ipv4_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c target/tcp_adapter.c tests/test_tcp_native_http.c -o $@

$(BUILD)/test_tcp_wire_integration: src/tcp_engine.c src/tcp_ingress.c src/vlan.c src/ipv4_wire.c tests/test_tcp_wire_integration.c include/ou_tcp_engine.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/tcp_engine.c src/tcp_ingress.c src/vlan.c src/ipv4_wire.c tests/test_tcp_wire_integration.c -o $@

$(BUILD)/test_tcp_engine: src/tcp_engine.c tests/test_tcp_engine.c include/ou_tcp_engine.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/tcp_engine.c tests/test_tcp_engine.c -o $@

$(BUILD)/test_tcp_ingress: src/tcp_ingress.c src/vlan.c src/ipv4_wire.c tests/test_tcp_ingress.c include/ou_tcp_ingress.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/tcp_ingress.c src/vlan.c src/ipv4_wire.c tests/test_tcp_ingress.c -o $@

$(BUILD)/test_vlan: src/vlan.c src/ipv4_wire.c tests/test_vlan.c include/ou_vlan.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/vlan.c src/ipv4_wire.c tests/test_vlan.c -o $@

$(BUILD)/test_ethernet_manager: board/ns7520_dma.c board/ns7520_mac.c src/ipv4_wire.c src/vlan.c src/tcp_ingress.c src/tcp_engine.c target/ethernet_path.c target/ethernet_manager.c tests/test_ethernet_manager.c include/ou_ethernet_manager.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_dma.c board/ns7520_mac.c src/ipv4_wire.c src/vlan.c src/tcp_ingress.c src/tcp_engine.c target/ethernet_path.c target/ethernet_manager.c tests/test_ethernet_manager.c -o $@

$(BUILD)/test_mac_duplex: board/ns7520_mac.c tests/test_mac_duplex.c include/ou_ns7520_mac.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_mac.c tests/test_mac_duplex.c -o $@

$(BUILD)/test_ns7520_mac: board/ns7520_mac.c tests/test_ns7520_mac.c include/ou_ns7520_mac.h include/ou_ns7520_dma.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_mac.c tests/test_ns7520_mac.c -o $@

$(BUILD)/test_ethernet_path: board/ns7520_dma.c src/ipv4_wire.c src/vlan.c src/tcp_ingress.c src/tcp_engine.c target/ethernet_path.c tests/test_ethernet_path.c include/ou_ethernet_path.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_dma.c src/ipv4_wire.c src/vlan.c src/tcp_ingress.c src/tcp_engine.c target/ethernet_path.c tests/test_ethernet_path.c -o $@

$(BUILD)/test_ns7520_dma: board/ns7520_dma.c tests/test_ns7520_dma.c include/ou_ns7520_dma.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_dma.c tests/test_ns7520_dma.c -o $@

$(BUILD)/test_ns7520_phy: board/ns7520_phy.c tests/test_ns7520_phy.c include/ou_ns7520_phy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_phy.c tests/test_ns7520_phy.c -o $@

$(BUILD)/test_board_mmio: board/ns7520_board_mmio.c board/board_probe.c board/ns7520_mem.c tests/test_board_mmio.c include/ou_ns7520_board_mmio.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/ns7520_board_mmio.c board/board_probe.c board/ns7520_mem.c tests/test_board_mmio.c -o $@

$(BUILD)/test_board_probe: board/board_probe.c board/ns7520_mem.c tests/test_board_probe.c include/ou_board_probe.h include/ou_ns7520_mem.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/board_probe.c board/ns7520_mem.c tests/test_board_probe.c -o $@

$(BUILD)/test_http_wire: src/core.c src/schema.c src/provider.c src/http_wire.c tests/test_http_wire.c include/ou_http_wire.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/http_wire.c tests/test_http_wire.c -o $@

$(BUILD)/test_event_archive: src/event_archive.c src/events.c board/config_journal.c tests/test_event_archive.c include/ou_event_archive.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/event_archive.c src/events.c board/config_journal.c tests/test_event_archive.c -o $@

$(BUILD)/test_native_event_archive: src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_event_archive.c include/ou_event_archive.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_event_archive.c -o $@

$(BUILD)/test_events: src/events.c tests/test_events.c include/ou_events.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/events.c tests/test_events.c -o $@

$(BUILD)/test_native_events: src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_events.c include/ou_native_service.h include/ou_events.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_events.c -o $@

$(BUILD)/test_native_service: src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_service.c include/ou_native_service.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_service.c -o $@

$(BUILD)/test_telemetry_guard: src/core.c src/schema.c src/provider.c src/telemetry_guard.c tests/test_telemetry_guard.c include/ou_telemetry_guard.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/core.c src/schema.c src/provider.c src/telemetry_guard.c tests/test_telemetry_guard.c -o $@

$(BUILD)/test_journal: board/config_journal.c tests/test_config_journal.c include/ou_config_journal.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) board/config_journal.c tests/test_config_journal.c -o $@

test: all $(BUILD)/test_events $(BUILD)/test_native_events $(BUILD)/test_event_archive $(BUILD)/test_native_event_archive $(BUILD)/test_core $(BUILD)/test_provider $(BUILD)/test_board $(BUILD)/test_journal $(BUILD)/test_telemetry_guard $(BUILD)/test_board_probe $(BUILD)/test_board_mmio $(BUILD)/test_ns7520_serial_rx $(BUILD)/test_shut_uart_bridge $(BUILD)/test_shut_uart_tx $(BUILD)/test_shut_uart_link $(BUILD)/test_ups_monitor $(BUILD)/test_shut_rx $(BUILD)/test_shut_request $(BUILD)/test_shut_session $(BUILD)/test_hid_report $(BUILD)/test_hid_telemetry $(BUILD)/test_http_wire $(BUILD)/test_native_service $(BUILD)/test_compiler_rt $(BUILD)/test_ns7520_phy $(BUILD)/test_ipv4_wire $(BUILD)/test_ns7520_dma $(BUILD)/test_ethernet_path $(BUILD)/test_ns7520_mac $(BUILD)/test_vlan $(BUILD)/test_ethernet_manager $(BUILD)/test_vlan_record $(BUILD)/test_tcp_ingress $(BUILD)/test_tcp_engine $(BUILD)/test_tcp_wire_integration $(BUILD)/test_tcp_native_http $(BUILD)/test_ns7520_hal $(BUILD)/test_board_runtime $(BUILD)/test_phy_discover $(BUILD)/test_ns7520_timer $(BUILD)/test_mac_duplex $(BUILD)/test_hal_timer $(BUILD)/test_phy_link
	./$(BUILD)/test_events
	./$(BUILD)/test_native_events
	./$(BUILD)/test_event_archive
	./$(BUILD)/test_native_event_archive
	./$(BUILD)/test_core
	./$(BUILD)/test_provider
	./$(BUILD)/test_board
	./$(BUILD)/test_journal
	./$(BUILD)/test_telemetry_guard
	./$(BUILD)/test_board_probe
	./$(BUILD)/test_board_mmio
	./$(BUILD)/test_ns7520_serial_rx
	./$(BUILD)/test_shut_uart_bridge
	./$(BUILD)/test_shut_uart_tx
	./$(BUILD)/test_shut_uart_link
	./$(BUILD)/test_ups_monitor
	./$(BUILD)/test_shut_rx
	./$(BUILD)/test_shut_request
	./$(BUILD)/test_shut_session
	./$(BUILD)/test_hid_report
	./$(BUILD)/test_hid_telemetry
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
	./$(BUILD)/test_vlan_record
	./$(BUILD)/test_tcp_ingress
	./$(BUILD)/test_tcp_engine
	./$(BUILD)/test_tcp_wire_integration
	./$(BUILD)/test_tcp_native_http
	./$(BUILD)/test_ns7520_hal
	./$(BUILD)/test_board_runtime
	./$(BUILD)/test_phy_discover
	./$(BUILD)/test_ns7520_timer
	./$(BUILD)/test_mac_duplex
	./$(BUILD)/test_hal_timer
	./$(BUILD)/test_phy_link
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
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/events.c -o $(BUILD)/events-armeb.o
	python3 tools/verify_arm.py $(BUILD)/events-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/event_archive.c -o $(BUILD)/event-archive-armeb.o
	python3 tools/verify_arm.py $(BUILD)/event-archive-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_mem.c -o $(BUILD)/ns7520-mem-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ups_link.c -o $(BUILD)/ups-link-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/flash_policy.c -o $(BUILD)/flash-policy-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-mem-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ups-link-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/shut_rx.c -o $(BUILD)/shut-rx-armeb.o
	python3 tools/verify_arm.py $(BUILD)/shut-rx-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_serial_rx.c -o $(BUILD)/serial-rx-armeb.o
	python3 tools/verify_arm.py $(BUILD)/serial-rx-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/shut_uart_bridge.c -o $(BUILD)/shut-uart-bridge-armeb.o
	python3 tools/verify_arm.py $(BUILD)/shut-uart-bridge-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/shut_uart_tx.c -o $(BUILD)/shut-uart-tx-armeb.o
	python3 tools/verify_arm.py $(BUILD)/shut-uart-tx-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/shut_uart_link.c -o $(BUILD)/shut-uart-link-armeb.o
	python3 tools/verify_arm.py $(BUILD)/shut-uart-link-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ups_monitor.c -o $(BUILD)/ups-monitor-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ups-monitor-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/shut_request.c -o $(BUILD)/shut-request-armeb.o
	python3 tools/verify_arm.py $(BUILD)/shut-request-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/shut_session.c -o $(BUILD)/shut-session-armeb.o
	python3 tools/verify_arm.py $(BUILD)/shut-session-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/hid_report.c -o $(BUILD)/hid-report-armeb.o
	python3 tools/verify_arm.py $(BUILD)/hid-report-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/hid_telemetry.c -o $(BUILD)/hid-telemetry-armeb.o
	python3 tools/verify_arm.py $(BUILD)/hid-telemetry-armeb.o
	python3 tools/verify_arm.py $(BUILD)/flash-policy-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/config_journal.c -o $(BUILD)/config-journal-armeb.o
	python3 tools/verify_arm.py $(BUILD)/config-journal-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/board_probe.c -o $(BUILD)/board-probe-armeb.o
	python3 tools/verify_arm.py $(BUILD)/board-probe-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_board_mmio.c -o $(BUILD)/board-mmio-armeb.o
	python3 tools/verify_arm.py $(BUILD)/board-mmio-armeb.o
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
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/tcp_ingress.c -o $(BUILD)/tcp-ingress-armeb.o
	python3 tools/verify_arm.py $(BUILD)/tcp-ingress-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/tcp_engine.c -o $(BUILD)/tcp-engine-armeb.o
	python3 tools/verify_arm.py $(BUILD)/tcp-engine-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c target/tcp_adapter.c -o $(BUILD)/tcp-adapter-armeb.o
	python3 tools/verify_arm.py $(BUILD)/tcp-adapter-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_hal.c -o $(BUILD)/ns7520-hal-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-hal-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/board_runtime.c -o $(BUILD)/board-runtime-armeb.o
	python3 tools/verify_arm.py $(BUILD)/board-runtime-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c board/ns7520_timer.c -o $(BUILD)/ns7520-timer-armeb.o
	python3 tools/verify_arm.py $(BUILD)/ns7520-timer-armeb.o
	clang --target=armeb-none-eabi -mcpu=arm7tdmi -marm -ffreestanding -fno-builtin -fno-stack-protector -Os -Wall -Wextra -Werror -Iinclude -c src/vlan_record.c -o $(BUILD)/vlan-record-armeb.o
	python3 tools/verify_arm.py $(BUILD)/vlan-record-armeb.o
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
	ld.lld -flavor gnu -m armelfb --build-id=none -T target/linker.ld $(BUILD)/reset-armeb.o $(BUILD)/runtime-armeb.o $(BUILD)/core-armeb-arm7tdmi.o $(BUILD)/schema-armeb-arm7tdmi.o $(BUILD)/provider-armeb-arm7tdmi.o $(BUILD)/telemetry-guard-armeb.o $(BUILD)/events-armeb.o $(BUILD)/event-archive-armeb.o $(BUILD)/ns7520-mem-armeb.o $(BUILD)/ups-link-armeb.o $(BUILD)/shut-rx-armeb.o $(BUILD)/serial-rx-armeb.o $(BUILD)/shut-uart-bridge-armeb.o $(BUILD)/shut-uart-tx-armeb.o $(BUILD)/shut-uart-link-armeb.o $(BUILD)/ups-monitor-armeb.o $(BUILD)/shut-request-armeb.o $(BUILD)/shut-session-armeb.o $(BUILD)/hid-report-armeb.o $(BUILD)/hid-telemetry-armeb.o $(BUILD)/flash-policy-armeb.o $(BUILD)/config-journal-armeb.o $(BUILD)/board-probe-armeb.o $(BUILD)/board-mmio-armeb.o $(BUILD)/ns7520-phy-armeb.o $(BUILD)/ipv4-wire-armeb.o $(BUILD)/ns7520-dma-armeb.o $(BUILD)/ethernet-path-armeb.o $(BUILD)/ns7520-mac-armeb.o $(BUILD)/vlan-armeb.o $(BUILD)/tcp-ingress-armeb.o $(BUILD)/tcp-engine-armeb.o $(BUILD)/tcp-adapter-armeb.o $(BUILD)/ns7520-hal-armeb.o $(BUILD)/board-runtime-armeb.o $(BUILD)/ns7520-timer-armeb.o $(BUILD)/vlan-record-armeb.o $(BUILD)/ethernet-manager-armeb.o $(BUILD)/http-wire-armeb.o $(BUILD)/native-service-armeb.o $(BUILD)/compiler-rt-armeb.o -o $(BUILD)/native-research-NOT-FLASHABLE.elf
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
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/event_archive.c src/events.c board/config_journal.c tests/test_event_archive.c -o $(BUILD)/test_event_archive_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_event_archive_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_event_archive.c -o $(BUILD)/test_native_event_archive_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_native_event_archive_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/events.c tests/test_events.c -o $(BUILD)/test_events_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_events_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_events.c -o $(BUILD)/test_native_events_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_native_events_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_mem.c board/ups_link.c board/flash_policy.c tests/test_board.c -o $(BUILD)/test_board_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/shut_rx.c board/ups_link.c tests/test_shut_rx.c -o $(BUILD)/test_shut_rx_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_shut_rx_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_serial_rx.c tests/test_ns7520_serial_rx.c -o $(BUILD)/test_serial_rx_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_serial_rx_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_bridge.c -o $(BUILD)/test_shut_uart_bridge_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_shut_uart_bridge_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/shut_uart_tx.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_tx.c -o $(BUILD)/test_shut_uart_tx_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_shut_uart_tx_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/shut_uart_link.c board/shut_uart_tx.c board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_uart_link.c -o $(BUILD)/test_shut_uart_link_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_shut_uart_link_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ups_monitor.c board/hid_telemetry.c board/hid_report.c board/shut_uart_link.c board/shut_uart_tx.c board/shut_uart_bridge.c board/ns7520_serial_rx.c board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_ups_monitor.c -o $(BUILD)/test_ups_monitor_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ups_monitor_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/shut_request.c tests/test_shut_request.c -o $(BUILD)/test_shut_request_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_shut_request_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/shut_session.c board/shut_request.c board/shut_rx.c board/ups_link.c tests/test_shut_session.c -o $(BUILD)/test_shut_session_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_shut_session_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/hid_report.c tests/test_hid_report.c -o $(BUILD)/test_hid_report_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_hid_report_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/hid_telemetry.c board/hid_report.c src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_hid_telemetry.c -o $(BUILD)/test_hid_telemetry_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_hid_telemetry_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/config_journal.c tests/test_config_journal.c -o $(BUILD)/test_journal_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_journal_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c tests/test_telemetry_guard.c -o $(BUILD)/test_telemetry_guard_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_telemetry_guard_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/board_probe.c board/ns7520_mem.c tests/test_board_probe.c -o $(BUILD)/test_board_probe_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_probe_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_board_mmio.c board/board_probe.c board/ns7520_mem.c tests/test_board_mmio.c -o $(BUILD)/test_board_mmio_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_mmio_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/http_wire.c tests/test_http_wire.c -o $(BUILD)/test_http_wire_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_http_wire_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c tests/test_native_service.c -o $(BUILD)/test_native_service_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_native_service_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined target/compiler_rt.c tests/test_compiler_rt.c -o $(BUILD)/test_compiler_rt_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_compiler_rt_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_phy.c tests/test_ns7520_phy.c -o $(BUILD)/test_ns7520_phy_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_phy_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/ipv4_wire.c tests/test_ipv4_wire.c -o $(BUILD)/test_ipv4_wire_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ipv4_wire_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_dma.c tests/test_ns7520_dma.c -o $(BUILD)/test_ns7520_dma_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_dma_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_dma.c src/ipv4_wire.c src/vlan.c src/tcp_ingress.c src/tcp_engine.c target/ethernet_path.c tests/test_ethernet_path.c -o $(BUILD)/test_ethernet_path_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ethernet_path_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_mac.c tests/test_ns7520_mac.c -o $(BUILD)/test_ns7520_mac_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_mac_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/vlan.c src/ipv4_wire.c tests/test_vlan.c -o $(BUILD)/test_vlan_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_vlan_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_dma.c board/ns7520_mac.c src/ipv4_wire.c src/vlan.c src/tcp_ingress.c src/tcp_engine.c target/ethernet_path.c target/ethernet_manager.c tests/test_ethernet_manager.c -o $(BUILD)/test_ethernet_manager_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ethernet_manager_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/vlan_record.c src/vlan.c src/ipv4_wire.c board/config_journal.c tests/test_vlan_record.c -o $(BUILD)/test_vlan_record_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_vlan_record_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/tcp_ingress.c src/vlan.c src/ipv4_wire.c tests/test_tcp_ingress.c -o $(BUILD)/test_tcp_ingress_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_tcp_ingress_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/tcp_engine.c tests/test_tcp_engine.c -o $(BUILD)/test_tcp_engine_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_tcp_engine_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/tcp_engine.c src/tcp_ingress.c src/vlan.c src/ipv4_wire.c tests/test_tcp_wire_integration.c -o $(BUILD)/test_tcp_wire_integration_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_tcp_wire_integration_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined src/core.c src/schema.c src/provider.c src/telemetry_guard.c src/http_wire.c src/tcp_engine.c src/tcp_ingress.c src/vlan.c src/ipv4_wire.c src/events.c src/event_archive.c board/config_journal.c target/service.c target/tcp_adapter.c tests/test_tcp_native_http.c -o $(BUILD)/test_tcp_native_http_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_tcp_native_http_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_hal.c tests/test_ns7520_hal.c -o $(BUILD)/test_ns7520_hal_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_hal_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_hal.c board/ns7520_phy.c board/board_probe.c board/ns7520_mem.c board/board_runtime.c tests/test_board_runtime.c -o $(BUILD)/test_board_runtime_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_board_runtime_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_phy.c tests/test_phy_discover.c -o $(BUILD)/test_phy_discover_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_phy_discover_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_timer.c tests/test_ns7520_timer.c -o $(BUILD)/test_ns7520_timer_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_ns7520_timer_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_mac.c tests/test_mac_duplex.c -o $(BUILD)/test_mac_duplex_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_mac_duplex_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_hal.c board/ns7520_timer.c tests/test_hal_timer.c -o $(BUILD)/test_hal_timer_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_hal_timer_sanitized
	$(CC) $(CPPFLAGS) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fno-omit-frame-pointer -fsanitize=address,undefined board/ns7520_phy.c tests/test_phy_link.c -o $(BUILD)/test_phy_link_sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$(BUILD)/test_phy_link_sanitized
