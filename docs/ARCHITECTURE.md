# Architecture

This document describes how the factory firmware is put together: the boot
contract it relies on, the source layers, the FreeRTOS tasks and the data
flow from the factory station to the identity sector.

The wire formats, flash addresses and command semantics are specified in
[FACTORY_SPEC.md](FACTORY_SPEC.md). This file is about the code.

## Boot context

The device runs a two-stage firmware:

```
BOOT (0x08000000)                 factory-firmware (slot at 0x0800C000)
  - verifies the image              - runs four FreeRTOS tasks
  - maps the active bank            - answers the factory station over UART
  - starts the IWDG (4 s)     --->  - confirms a pending slot once healthy
  - hands over in RTC_BKP2R
```

The application never assumes it was started from slot A. It always links
at `0x0800C000`; BOOT swaps the active flash bank into place with
`SYSCFG_MEMRMP.SWP_FB`. The shared constants live in
[`src/platform/boot_contract.h`](../src/platform/boot_contract.h) and are
checked by the CI "Boot contract" stage.

## Source layers

The source tree is split by responsibility. Every layer is on the include
path, so modules keep using short and unique header names.

```
src/
  app/         entry point, fault record, debug log, task reporting, RTOS hooks
  bsp/         clock tree, HAL time base, cache/MPU, USART3 DMA driver
  platform/    boot/BOOT contract, handover, reset cause, IWDG, flash confirm
  services/    CRC-32, identity reader, factory flash store, flatcc arena
  protocol/    HWLINK framing, factory command handler
  tasks/       Health, Comms, Button and LED FreeRTOS tasks
  generated/   flatcc headers generated from protocol/hwlink.fbs
```

Dependency direction is downward: `tasks` may call `protocol`, `services`,
`platform`, `bsp` and `app`; `protocol` is pure logic and has no hardware
dependency at all, which is what makes the host tests possible.

`protocol/factory_proto.c` is deliberately hardware-agnostic. All flash,
UID and self-test access goes through the `factory_ops_t` function table.
On the target the table is provided by `services/factory_store.c`; in the
host tests it is provided by `tests/host/factory_sim.c`.

## Runtime

`main()` starts the board, prints the debug banner on USART6, creates every
task and queue and then calls `vTaskStartScheduler()`. Nothing allocates
after start-up. The FreeRTOS heap (`heap_4`) and the flatcc arena are both
fixed at compile time.

| Task     | Priority | Stack | Period | Responsibility                                      |
| -------- | -------- | ----- | ------ | --------------------------------------------------- |
| `Health` | 5        | 384 W | 20 ms  | Supervises tasks, refreshes the IWDG, confirms slot  |
| `Comms`  | 4        | 1536 W| 10 ms  | HWLINK frames, factory commands, replies            |
| `Button` | 3        | 384 W | 10 ms  | Debounces B1, reports short/long presses            |
| `LED`    | 2        | 384 W | event  | LED patterns and the LD3 latched error display      |

The Health task is the only task that kicks the watchdog. Every other task
sets a check-in notification at least twice a second. If a check-in is
missing for 2 s, Health stops refreshing the watchdog and the 4 s IWDG
resets the board. A pending slot gets a health window of 5 s and is
confirmed by writing the CONFIRMED record; a failed check lets the watchdog
roll back to the other slot.

## Data flow

```
factory-station
      |  COBS + CRC-32 frame, proto 0x46, raw factory payload
      v
USART3 DMA ring  ->  Comms task  ->  factory_handle()
                                          |
                                          +-- INFO / READ / SELFTEST
                                          +-- WRITE_BEGIN / WRITE_DATA
                                          +-- WRITE_END -> factory_store
                                                               |
                                                               v
                                              bank 2 sector 13 (16 KB)
                                              PRV1 | SYS | CAL | DIAG
```

A write is staged in RAM. `WRITE_END` validates the file header, both
CRC-32 values and the chip UID binding before the flash store copies the
sector, patches one 4 KB block, erases, programs and verifies it. PRV1 is
read-only for this firmware.

The debug log goes to a separate port (USART6) so it never competes with
the protocol traffic on the ST-LINK virtual COM port.

## Build and test layers

| Layer            | Command                          | What it proves                         |
| ---------------- | -------------------------------- | -------------------------------------- |
| Host unit tests  | `make test`                      | Command handler against simulated flash |
| Cross build      | `make build`                     | Firmware links, fits and produces `.img`|
| Static analysis  | `make lint` / `make lint-strict` | clang-tidy rules from `.clang-tidy`     |
| Image validation | Jenkins "Image" stage            | Header, CRC and vector table of `.img`  |
| Boot contract    | Jenkins "Boot contract" stage    | Shared layout constants have not drifted|
| Package          | `ci/package.py`                  | Artefacts plus `SHA256SUMS` and manifest |

The host tests need no board and no STM32 SDK. The cross build and static
analysis run inside the `stm32-dev` container, which provides the
STM32CubeF7 SDK and the ARM toolchain.
