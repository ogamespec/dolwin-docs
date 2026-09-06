# GameCube — Disk Drive

> The GameCube reads games from a proprietary **3" mini-DVD** (Nintendo's
> "GameCube Game Disk"). The drive itself is an intelligent, command-driven
> device built around a small CPU with firmware, developed with Matsushita. It is
> controlled by the Flipper **Disk Interface (DI)**.

## 1. Media characteristics

| Parameter | Value |
|---|---|
| Format | 3" mini-DVD, Matsushita technology |
| Capacity | **1,459,978,240 bytes** (~1.4 GB / 1.5 GB) |
| Rotation | **Constant Angular Velocity (CAV)** |
| Density | constant; sectors spiral from edge to centre |
| Read rate | ≈ 2–3 MB/s (comparable to a ~13× CD-ROM) |
| Audio | streamed **DVD-Audio**, ADPCM, **48 kHz** |

A proprietary copy-protection scheme (unlike the CSS used on standard DVDs)
combines **non-standard barcodes** with **encryption of the sector data**, which
the DVD controller decrypts on the fly. The result is that a GameCube disk cannot
be read on ordinary PC optical hardware.

## 2. Drive architecture

The drive is an **intelligent device**: it contains a microcontroller from the
**MN-10200** family running proprietary firmware from an internal ROM. Because the
drive has its own processor, the console talks to it **at the command level**
rather than at the raw media level. This also means the drive can perform its own
copy-protection de/encryption internally.

Two interaction modes are exposed by the DI:

1. **Immediate commands** — quick control (stop/start the motor, start a DVD-Audio
   stream, register access, etc.).
2. **DI DMA** — bulk data transfer between the disk and main memory (the normal
   way game data and streaming audio are read). All DMA transfers are
   32-byte-aligned.

## 3. DISK Interface (DI)

The DI register block is at physical address `0x0C006000` (32-bit registers).

| Register | Offset | Purpose |
|---|---|---|
| `DISR` | 0x00 | Status: Break & transfer-complete interrupts/masks, device error, break request |
| `DICVR` | 0x04 | Cover/loose-lid status and interrupt |
| `DICMDBUF0..2` | 0x08–0x10 | 12-byte command packet |
| `DIMAR` | 0x14 | DMA memory address (32-byte aligned) |
| `DILENGTH` | 0x18 | DMA transfer length (bytes, 32-byte aligned) |
| `DICR` | 0x1C | Control: read/write, DMA/immediate mode, transfer start |
| `DIIMMBUF` | 0x20 | Immediate-mode register data |
| `DICFG` | 0x24 | Drive configuration latched from the data bus at reset |

Commands issued to the drive are sent as a 12-byte packet; data may then be
transferred either in DMA mode to/from main memory or immediately to/from the
`DIIMMBUF` (only the "register access" command uses immediate mode). The drive
signals completion/errors through the status register and the dedicated `DIERR`/
`DIBRK` signal lines.

## 4. DI signals (signal level)

| Signal | Direction | Description |
|---|---|---|
| `DIDD[7:0]` | I/O | 8-bit data bus, direction controlled by `DIDIR`; also latches the reset configuration |
| `DIDIR` | O | Data-bus direction (0 = Flipper→drive, 1 = drive→Flipper) |
| `DIHSTRB` | O | Host strobe (qualifies data on the rising edge when writing; acts as a "ready" when reading) |
| `DIDSTRB` | I | Device strobe (qualifies data when reading; acts as "ready" when writing) |
| `DIERR` | I | Error from the drive; halts the current command and may raise an interrupt |
| `DIBRK` | I/O | Break signal (open-drain with external pull-up) used to interrupt a transfer |
| `DICOVER` | I | Drive cover switch; high = open |
| `DIRST` | O | Drive reset (controlled by the PI general reset register) |

## 5. DVD-Audio streaming (AIS)

Streamed audio is carried on separate pins into Flipper:

| Signal | Direction | Description |
|---|---|---|
| `AISD` | I | Serial left/right audio bitstream from the drive |
| `AISLR` | O | Frame/left-right signal at 32/48 kHz; also gates data flow (a stopped stream is silenced) |
| `AISCLK` | O | Free-running bit clock for `AISD` |

The stream is sample-rate-converted to 48 kHz by the Audio Interface before being
mixed.

## 6. References

- `HW/IO/DiskInterface.md` — DI signals and register descriptions (based on US
  Patent 6,609,977).
- `HW/gcspecs.txt` — media capacity, CAV, read rate, ADPCM streaming.
- `HW/acronyms.txt` — DI/DVD entry (MN-10200 controller, Matsushita, barcode +
  encryption copy protection).
- `WEB/DVDDisc/DVD Disc Specifications.htm` — media technical notes.
