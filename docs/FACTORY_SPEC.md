# Factory workflow specification

Shared contract between `factory-firmware` (device) and `factory-station`
(host). Both sides implement exactly what is written here. Author: Sandesh
Ghimire.

## 1. Flow

```
detect ST-LINK + UART
  -> flash BOOT over SWD (signed-image build, key chosen by the operator)
  -> flash factory firmware into slot A through BOOT over UART (HWLINK)
  -> reset, read firmware name and version
  -> write .sys, .cal, .diag over UART through the factory firmware
```

Every write is read back and its CRC-32 compared before the page reports
success.

## 2. Memory map

The factory data lives in the identity sector, bank 2 sector 13
(`0x08104000`, 16 KB). BOOT never erases or writes it (LAY-004 covers
bank 2 sectors 0-1 only for BOOT itself; the identity sector is reserved
by A-01). The factory firmware owns it.

| Block    | Address      | Size  | Contents                                  |
| -------- | ------------ | ----- | ----------------------------------------- |
| PRV1     | `0x08104000` | 4 KB  | Existing identity record, never touched   |
| SYS      | `0x08105000` | 4 KB  | `.sys` system info                        |
| CAL      | `0x08106000` | 4 KB  | `.cal` calibration                        |
| DIAG     | `0x08107000` | 4 KB  | `.diag` fault and diagnostic map          |

The addresses are defaults. The station lets the operator choose another
target; the firmware accepts a write only when the address is 4 KB aligned
and the whole 4 KB block lies inside `0x08105000..0x08107FFF`, so PRV1 can
never be overwritten. Reads may also cover PRV1.

A flash sector erases as a unit, so the firmware writes a block by
read-modify-write of the 16 KB sector through a RAM buffer: copy the sector,
patch the block, erase, program, verify. A power loss during the write can
damage the sector, which is acceptable on a factory bench and is why the
station always re-runs the read-back check.

## 3. File format

All three files share a 32-byte little-endian header followed by the payload.

| Offset | Size | Field        | Notes                                      |
| ------ | ---- | ------------ | ------------------------------------------ |
| 0      | 4    | `magic`      | `SYS1` `CAL1` `DIAG` as ASCII, see below   |
| 4      | 2    | `format`     | `1`                                        |
| 6      | 2    | `revision`   | Record revision, set by the author         |
| 8      | 4    | `length`     | Payload length in bytes                    |
| 12     | 4    | `payload_crc`| CRC-32 (zlib) of the payload               |
| 16     | 12   | `uid`        | Chip UID the file is bound to, or zeros    |
| 28     | 4    | `header_crc` | CRC-32 (zlib) of header bytes 0..27        |

Magic values (little-endian words): `.sys` = `0x31535953` (`SYS1`),
`.cal` = `0x314C4143` (`CAL1`), `.diag` = `0x47414944` (`DIAG`).
A `uid` of zeros means the file is not bound to one chip; a non-zero `uid`
must equal the chip UID or the firmware refuses the write.

The payload is a TLV list, each item `type u8, length u16, value`.
Maximum file size is 4096 bytes including the header.

### 3.1 `.sys` payload types

| Type | Name              | Value                       |
| ---- | ----------------- | --------------------------- |
| 0x01 | `serial`          | ASCII                       |
| 0x02 | `product`         | ASCII                       |
| 0x03 | `hw_version`      | ASCII                       |
| 0x04 | `fw_version`      | ASCII                       |
| 0x05 | `batch`           | ASCII                       |
| 0x06 | `mfg_date`        | ASCII `YYYY-MM-DD`          |
| 0x07 | `mac`             | 6 bytes                     |
| 0x08 | `notes`           | ASCII                       |

### 3.2 `.cal` payload types

| Type | Name        | Value                                              |
| ---- | ----------- | -------------------------------------------------- |
| 0x01 | `channel`   | `id u8`, `gain f32`, `offset f32` (9 bytes)        |
| 0x02 | `temp_ref`  | `celsius f32`                                      |
| 0x03 | `date`      | ASCII `YYYY-MM-DD`                                 |
| 0x04 | `operator`  | ASCII                                              |

### 3.3 `.diag` payload types

| Type | Name        | Value                                                        |
| ---- | ----------- | ------------------------------------------------------------ |
| 0x01 | `fault_map` | `code u16`, `severity u8`, `flags u8`, `text` ASCII          |
| 0x02 | `counter`   | `id u16`, `initial u32`                                      |
| 0x03 | `log_cfg`   | `depth u16`, `flags u16`                                     |

Initializing the map means writing a clean `.diag` block: counters at their
`initial` value and an empty fault log.

## 4. CSV database

`station-data/factory.csv`, UTF-8, header row required.

```
id,serial,product,hw_version,fw_version,batch,mfg_date,mac,notes,status
```

`id` is unique. `status` is `new` or `programmed`; the station sets it to
`programmed` after a verified `.sys` write. Searching by `id` returns one
row, which the station compacts into a `.sys` file.

## 5. UART protocol (factory commands)

Same wire framing as HWLINK: COBS, CRC-32 and the 8-byte header
`proto u8, flags u8, seq u16`. Factory frames use `proto = 0x46` (`F`)
instead of `1` and carry a raw binary payload instead of a FlatBuffer, so
the existing BOOT and HWLINK commands (`Hello`, `Reset`, `EnterBootloader`)
keep working on `proto = 1`.

Payload: `cmd u8`, then arguments. Replies echo `cmd | 0x80` and a
`status u8` (0 = OK).

| Cmd  | Name          | Request                                  | Reply                              |
| ---- | ------------- | ---------------------------------------- | ---------------------------------- |
| 0x01 | `INFO`        | none                                     | `name`, `version`, `build`, `uid`  |
| 0x02 | `READ`        | `address u32`, `length u16`              | bytes                              |
| 0x03 | `WRITE_BEGIN` | `address u32`, `total u16`, `crc u32`    | none                               |
| 0x04 | `WRITE_DATA`  | `offset u16`, bytes (max 512)            | none                               |
| 0x05 | `WRITE_END`   | none                                     | `crc u32` of what was programmed   |
| 0x06 | `DIAG_INIT`   | `address u32`                            | none                               |
| 0x07 | `SELFTEST`    | none                                     | `count u8`, then `count` checks    |

### 5.1 `SELFTEST`

Runs the basic peripheral checks and reports each as 6 bytes:
`id u8`, `result u8` (0 pass, 1 fail, 2 skipped), `value u32` (the number the
check measured). It writes nothing to flash. It is refused with `SEQUENCE`
while a write is in progress, because the RAM test borrows the write buffer.

| Id | Check          | Passes when                                              | `value`            |
| -- | -------------- | -------------------------------------------------------- | ------------------ |
| 1  | `DEVICE_ID`    | `DBGMCU_IDCODE` DEV_ID is `0x451`                        | the IDCODE         |
| 2  | `FLASH_SIZE`   | the flash size register reads 2048 KB                    | size in KB         |
| 3  | `CLOCK`        | the system clock is 216 MHz                              | Hz                 |
| 4  | `RAM`          | a walking-bit and address pattern test of 4 KB of SRAM   | bytes tested       |
| 5  | `CRC`          | CRC-32 of `123456789` is `0xCBF43926`                    | the computed CRC   |
| 6  | `FLASH_READ`   | the identity sector reads back                           | 0                  |
| 7  | `OPTION_BYTES` | nDBANK is 0 (dual-bank) and RDP is not level 2           | `FLASH_OPTCR`      |

Status codes: 0 OK, 1 BAD_COMMAND, 2 BAD_RANGE, 3 BAD_HEADER, 4 BAD_CRC,
5 BAD_UID, 6 FLASH_ERROR, 7 SEQUENCE.

`INFO` strings are length-prefixed (`u8` length, ASCII). `WRITE_END`
verifies the whole block against `crc` from `WRITE_BEGIN`, programs it, reads
it back and replies with the CRC of the flash contents.

## 6. Factory firmware identity

`name` is `soccentric-factory-firmware`, the version follows semantic
versioning, linked at `0x0800C000` and signed with the product key like any
other application. Slot A holds it after the first flash.
