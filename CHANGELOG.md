# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-11-19

### Added

- First application a fresh NUCLEO-F767ZI runs after the factory station
  flashed the bootloader: FreeRTOS in C, linked for slot A
- Answers the factory station over the ST-LINK virtual COM port and reports
  its name and version
- Writes the per-unit system info, calibration data and diagnostic map into
  the factory data window of the identity sector
- Derived from the `app-c-freertos` platform layer

[Unreleased]: https://github.com/soccentric-stm32/factory-firmware/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/soccentric-stm32/factory-firmware/releases/tag/v1.0.0
