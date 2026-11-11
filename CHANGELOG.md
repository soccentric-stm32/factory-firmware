# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed

- Reorganised `src/` into `app`, `bsp`, `platform`, `services`, `protocol`
  and `tasks` layers. Behaviour and wire formats are unchanged.
- The CMake target and build artefacts are now named `factory.*`; the
  duplicate `app.*` outputs were removed.
- Added architecture, contributing, security and test documentation.

### Added

- Host-test stage in the Jenkins pipeline and a matching GitHub Actions
  workflow.
- `make format` target and editor/line-ending configuration.

## [0.1.0] - 2026-11-10

### Added

- FreeRTOS factory firmware for the NUCLEO-F767ZI, linked for the BOOT slot
  at `0x0800C000`.
- HWLINK framing (COBS, CRC-32) with factory commands `INFO`, `READ`,
  `WRITE_BEGIN`, `WRITE_DATA`, `WRITE_END`, `DIAG_INIT` and `SELFTEST` on
  protocol byte `0x46`.
- Read-modify-write access to the 4 KB factory data blocks in the identity
  sector, with header, CRC and chip UID validation.
- Health supervision, watchdog refresh, BOOT handover and pending-slot
  confirmation.
- Debug log on USART6 and once-a-second task status lines.
- Host tests of the command handler against a simulated flash.

[Unreleased]: https://github.com/soccentric-stm32/factory-firmware/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/soccentric-stm32/factory-firmware/releases/tag/v0.1.0
