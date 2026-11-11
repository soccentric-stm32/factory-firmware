# factory-firmware

The first application a fresh NUCLEO-F767ZI runs after the factory station has flashed the bootloader. It is a FreeRTOS application in C, linked for slot A, that answers the factory station over the ST-LINK virtual COM port. It reports its name and version and writes the per-unit system info, calibration data and diagnostic map into the factory data window of the identity sector. It is derived from [app-c-freertos](https://github.com/soccentric-stm32/app-c-freertos) and keeps its health supervision, watchdog and BOOT handover.

[![The factory firmware (simulator run)](https://raw.githubusercontent.com/soccentric-stm32/soccentric-stm32.github.io/main/assets/shots/term-factory-firmware.webp)](https://soccentric-stm32.github.io/#factory-firmware)

## Features

- Runs from slot A at `0x0800C000` under the A/B bootloader
- Factory commands over UART with the HWLINK framing (COBS, CRC-32) and protocol byte `0x46`
- `INFO` returns firmware name, version, build date and chip UID
- `READ` returns any bytes of the factory data window and the identity record
- `WRITE_BEGIN`, `WRITE_DATA` and `WRITE_END` stage, validate and program one `.sys`, `.cal` or `.diag` file
- Checks the file header, header CRC, payload CRC and the chip UID binding before touching flash
- Programs the 4 KB block by read-modify-write of the identity sector and reads it back
- `DIAG_INIT` writes a clean fault and diagnostic map
- Refuses any write outside the three factory data blocks, so the PRV1 identity record is never overwritten
- Existing HWLINK commands (`Hello`, `Reset`, `EnterBootloader`) keep working on protocol byte `1`
- Host tests run the real command handler against a simulated flash

## Build and run

```bash
scripts/docker-dev.sh build   # build the stm32-dev container image once
scripts/build.sh              # build inside the container, produces build/factory.img
make test                     # host tests of the command handler
```

The station signs `build/factory.img` with your key and flashes it through the bootloader. The memory map, file formats and command set are in [docs/FACTORY_SPEC.md](docs/FACTORY_SPEC.md).

## Related projects

- [bootloader](https://github.com/soccentric-stm32/bootloader) — A/B bootloader with UART update, CRC32 checks, trial boot and rollback
- [app-c-freertos](https://github.com/soccentric-stm32/app-c-freertos) — Demo application in C on FreeRTOS
- [factory-station](https://github.com/soccentric-stm32/factory-station) — HWVAL Factory: end-of-line test and provisioning station
- [soccentric-stm32.github.io](https://github.com/soccentric-stm32/soccentric-stm32.github.io) — Project website

## License

Released under the [MIT License](LICENSE) © 2026 Sandesh Ghimire. You are free
to use, modify and ship it in your own products.

Contributions are welcome. Fork the repository, make your change on a branch and
open a pull request. Bug reports, ideas and improvements are all appreciated.
