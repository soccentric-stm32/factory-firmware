#
# Makefile - Top-level build, clean and flash for the factory firmware
#
# Author:  Sandesh Ghimire <sandesh@soccentric.com>
# Target:  NUCLEO-F767ZI (STM32F767ZI)
#
# The firmware is built with CMake + Ninja inside the stm32-dev container
# (DCON-001), the same way as scripts/build.sh. Flashing and debugging also
# run in the container by default; set NATIVE=1 to use the host's OpenOCD,
# GDB and picocom instead (scripts/install-tools.sh).
#
#   make                build the firmware (same as make build)
#   make build          configure (if needed) and build
#   make clean          remove the build directory
#   make rebuild        clean, then build
#   make flash          build, then upload factory.img through BOOT (bootctl)
#   make erase-all      erase the whole flash (asks for confirmation)
#   make reset          reset the board
#   make debug          build, flash, then GDB halted at reset
#   make gdbserver      OpenOCD GDB server on :3333
#   make serial         serial console on the ST-LINK VCP (115200 8N1)
#   make lint           clang-tidy static analysis
#   make lint-strict    clang-tidy, failing on any finding
#   make format         clang-format the project sources and headers
#   make test           host tests of the factory command handler (no board)
#   make probe          detect the ST-LINK and the target chip
#   make size           show the firmware size
#   make image          build the stm32-dev Docker image
#   make help           show this help
#
# Variables (override on the command line, e.g. make BUILD_TYPE=Debug):
#   BUILD_TYPE   Release | Debug                       (Release, as build.sh)
#   NATIVE       1 = flash/debug/serial with host tools (0)
#   BOOTCTL      bootctl.py used by make flash
#                (default ../bootloader/tools/bootctl.py)
#   PORT         serial port for make flash (default: bootctl auto-detect)
#

BUILD_TYPE ?= Release
NATIVE     ?= 0

BUILD_DIR  := build
ELF        := $(BUILD_DIR)/factory.elf
IMAGE      := $(BUILD_DIR)/factory.img
DEV        := scripts/docker-dev.sh
OPENOCD    := openocd -f board/st_nucleo_f7.cfg
BOOTCTL    ?= $(firstword $(wildcard ../bootloader/tools/bootctl.py ../../bootloader/bootloader/tools/bootctl.py) ../bootloader/tools/bootctl.py)
BOOT_DIR   ?= $(patsubst %/tools/bootctl.py,%,$(BOOTCTL))
SIGN_KEY   ?= $(BOOT_DIR)/keys/dev-signing-key.pem
SECURITY_COUNTER ?= 3
SIGNED     := $(IMAGE:.img=-signed.img)
PORT       ?=
PYTHON     ?= python3

.DEFAULT_GOAL := help

.PHONY: all build configure clean rebuild lint lint-strict format flash \
        erase-all reset debug gdbserver serial probe size image test help

all: build

# Reconfigure when the build directory is new or the build type changed;
# otherwise let Ninja's own dependency tracking decide what to rebuild.
configure:
	@if ! grep -qx "CMAKE_BUILD_TYPE:STRING=$(BUILD_TYPE)" $(BUILD_DIR)/CMakeCache.txt 2>/dev/null; then \
		echo "Configuring $(BUILD_TYPE) build in $(BUILD_DIR)/"; \
		$(DEV) run cmake -S /work -B /work/$(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE); \
	fi

build: configure
	$(DEV) run cmake --build /work/$(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean
	@$(MAKE) --no-print-directory build

# clang-tidy needs the compile commands from a configured build tree. Reset
# the strict flag so a previous lint-strict run does not leak into this one.
lint: configure
	$(DEV) run cmake -S /work -B /work/$(BUILD_DIR) -G Ninja -DAPP_LINT_STRICT=OFF
	$(DEV) run cmake --build /work/$(BUILD_DIR) --target clang-tidy

lint-strict:
	$(DEV) run cmake -S /work -B /work/$(BUILD_DIR) -G Ninja -DAPP_LINT_STRICT=ON
	$(DEV) run cmake --build /work/$(BUILD_DIR) --target clang-tidy

# Format only hand-written code; src/generated is owned by flatcc.
format:
	clang-format -i $$(find src board tests -name '*.c' -o -name '*.h' | grep -v '/generated/')

# A-6: never program the raw ELF into a slot. BOOT only starts an image
# that has its 256-byte header, metadata and records, which bootctl writes.
flash: build
	@test -f $(IMAGE) || { echo "error: $(IMAGE) not found, run make build"; exit 1; }
	@test -f $(BOOTCTL) || { echo "error: $(BOOTCTL) not found; set BOOTCTL=/path/to/bootctl.py"; exit 1; }
	@test -f $(SIGN_KEY) || { echo "error: $(SIGN_KEY) not found; set SIGN_KEY=/path/to/key.pem"; exit 1; }
	$(PYTHON) $(BOOT_DIR)/tools/mkimage.py --sign $(IMAGE) --key $(SIGN_KEY) \
		--security-counter $(SECURITY_COUNTER) --output $(SIGNED)
	$(PYTHON) $(BOOTCTL) $(if $(PORT),--port $(PORT),) update $(SIGNED) --activate

ERASE_CMD = $(if $(filter 1,$(NATIVE)),$(OPENOCD) -c "init; reset halt; flash erase_sector 0 0 last; exit",$(DEV) erase)

erase-all:
	@echo "WARNING: this erases the whole flash: BOOT, both metadata sectors"
	@echo "         and both application slots."
	@printf "Type 'yes' to continue: " && read answer && [ "$$answer" = "yes" ]
	$(ERASE_CMD)

size:
	@test -f $(ELF) || { echo "error: $(ELF) not found, run make build"; exit 1; }
	$(DEV) run arm-none-eabi-size /work/$(ELF)

image:
	$(DEV) build

ifeq ($(NATIVE),1)

reset:
	$(OPENOCD) -c "init; reset run; exit"

debug: build
	$(OPENOCD) -l $(BUILD_DIR)/openocd.log & ocd=$$!; \
	trap 'kill $$ocd 2>/dev/null' EXIT; sleep 1; \
	gdb-multiarch -q $(ELF) -ex "target extended-remote :3333" -ex "monitor reset halt" -ex load

gdbserver:
	$(OPENOCD)

serial:
	scripts/serial.sh

probe:
	st-info --probe

else

reset:
	$(DEV) reset

debug: build
	$(DEV) debug $(ELF) --load

gdbserver:
	$(DEV) gdbserver

serial:
	$(DEV) serial

probe:
	$(DEV) probe

endif

help:
	@sed -n '/^#   make/p; /^# Variables/,/^#$$/p' $(firstword $(MAKEFILE_LIST)) | sed 's/^# \{0,1\}//'

# Host tests for the factory command handler (no board needed).
test:
	$(MAKE) -C tests/host test
