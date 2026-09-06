# GameCube — Motherboard, Connectors and Components

> Scope: the retail **HW2** production board (codename **Dolphin**). Earlier
> developer boards (Marlin, Arthur, HW1) are not covered here.
>
> This document is the architectural overview of the physical board. It is meant
> as the foundation for the component-level specifications (Gekko, Flipper, the
> disk drive, and the EXI/SI peripherals) that follow in this `architecture/`
> folder.

## 1. Board layout

The console is built around two large integrated circuits on the main CPU board
(stamped `DOL-CPUP-01`), plus a small number of memory/auxiliary chips. All of the
"system" logic with the exception of the TV-out encoder path lives inside a single
ASIC called **Flipper**; everything else on the board hangs off it.

| Block | Part | Role |
|---|---|---|
| CPU | IBM **Gekko** | Modified PowerPC 750-derivative, 486 MHz |
| System ASIC | **Flipper** | Northbridge + graphics processor + audio DSP + I/O controller |
| Main memory | 2 × 12 MB MoSys **1T-SRAM** (codename "Splash") | 24 MB main RAM, 64-bit interface |
| Auxiliary memory | 16 MB DRAM (**ARAM**) | DSP/audio + texture streaming memory |
| Boot ROM + RTC + SRAM | 2 MB encrypted boot chip | IPL, system menu, RTC, settings SRAM |
| Disk drive | **DVD drive unit** (DDU) | Matsushita 3" mini-DVD, intelligent device |
| Clocking | 27 MHz crystal + clock generator | System + video clock sources |

**Total system memory** is therefore 24 MB of fast 1T-SRAM plus 16 MB of slower
DRAM (ARAM) — 40 MB in all. Development boards could extend each of these (up to
48 MB of RAM and 4/16/32 MB ARAM expansions).

## 2. System buses

The Gekko–Flipper bus is the core system interconnect:

- **32-bit** address bus and **64-bit** data bus.
- Bus clock **162 MHz** (one third of the 486 MHz CPU).
- Sustained bandwidth ~**1.3 GB/s** (162 MHz × 8 bytes).
- Transaction sizes: 1, 2, 4, 8 and 32 bytes (a 32-byte **burst** is used by cache
  fills and by the write-gather buffer).

The ARAM is attached to Flipper's own SDRAM controller with a separate **8-bit**
data cable running at **81 MHz**, giving roughly 80 MB/s; it is reached only
through DMA, never directly by the CPU.

## 3. Flipper device pins (physical interface)

The Flipper ASIC is the hub to which every other subsystem is wired. The pin
groups below are taken from the chip's top-level pin list and are the reference
for the connector/signal-level sections elsewhere in this documentation.

| Group | Signals | Notes |
|---|---|---|
| CPU interface | `cpua[31:0]`, `cpud[63:0]`, `cpuintb`, `cputsiz[2:0]`, `cputt[4:0]`, `cputbstb`, `cputab`, `cputsb`, `cpuaackb`, `cpurstb`, `cpuref` | 32-bit address / 64-bit data, interrupt and reset to Gekko |
| Main memory (1T-SRAM) | `mema[21:0]`, `memd[63:0]`, `memclk`, `memclkb`, `memckq[3:0]`, `memads[1:0]b`, `memmsk[1:0]`, `memref`, `memrefsh`, `memrstb`, `memrw` | 64-bit data, 22-bit address |
| ARAM (SDRAM) | `sda[12:0]`, `sdd[7:0]`, `sdba[1:0]`, `sdclk`, `sdclkfbk`, `sdcsb`, `sdcasb`, `sdrasb`, `sdweb`, `sddqm`, `sdintb` | 8-bit data, dedicated SDRAM pins |
| Disk drive (DI) | `did[7:0]`, `didir`, `dihstrbb`, `didstrbb`, `dierrb`, `dibrk`, `dicover`, `dirstb` | Command/data to the DVD drive |
| Audio out (AI) | `aid`, `ailr`, `aiclk` | Serial PCM to the audio DAC |
| Audio stream in (AIS) | `aisd`, `aislr`, `aisclk` | Streamed DVD audio into Flipper |
| Video out (VI) | `vid[7:0]`, `viclk27`, `viclk54`, `viclkin`, `vicr`, `visel` | 8-bit digital video, 27/54 MHz |
| EXI0 (parallel/high-speed) | `exi0clk0`, `exi0clk1`, `exi0csb[2:0]`, `exi0di0/1`, `exi0do0/1`, `exi0extin`, `exi0intb` | 16-bit peripheral channel, 3 chip selects |
| EXI1 / EXI2 (serial) | `exi1clk`, `exi1csb`, `exi1di/do`, `exi1extin`, `exi1intb`; `exi2clk`, `exi2csb`, `exi2di/do`, `exi2intb` | Modem and debugger channels |
| Controller (SI) | `sidi0..3`, `sido0..3` | Four dedicated controller channels |
| Light gun | `guntrg[1:0]` | Two trigger inputs |
| Debug | `tck`, `tdi`, `tdo`, `tms`, `tse`, `tmd`, `etmd`, `dbgintb` | JTAG / ETM trace |
| Reset / PLL | `rstinb`, `rstswb`, `lock`, `byppll`, `clk`, `clko` | Reset switch, PLL lock and bypass |

## 4. System connectors and their signal level

The board exposes the following user-visible connectors. Signal-level detail for
each is described in the linked sections; the disk-drive connector pinout appears
in the *Disk Drive* document.

### 4.1 Disk drive connector (P9)

A 32-pin header carries both the drive control/data signals and three power pins
(+5 V on pads 2, 4, 6, 8) plus grounds. The non-power pins are:

| Pad | Signal | Pad | Signal |
|---|---|---|---|
| 1 | AISLR | 16 | Ground |
| 3 | AISD | 17 | DID7 |
| 5 | AISCLK | 19 | DID6 |
| 7 | DIHSTRB | 21 | DID5 |
| 9 | DIERR | 23 | DID4 |
| 11 | DIBRK | 25 | DID3 |
| 12 | DICOVER | 27 | DID2 |
| 13 | DIDSTRB | 28 | MONI |
| 14 | DIRST | 29 | DID1 |
| 15 | DIDIR | 30 | MONOUT |
| — | — | 31 | DID0 |

### 4.2 Controller ports (SI)

Four identical ports, one per SI channel. Communication is serial,
**half-duplex**, over a **single data line** per channel using
**pulse-width (duty-cycle) modulation**, with data transferred **big-endian**.
Command/response packets are a few bytes each; the controller itself is an
intelligent device. The SI hardware supports up to eight data/response bytes per
channel per poll and hardware polling tied to the video vertical blank.

### 4.3 Peripheral ports (EXI)

- **Parallel / high-speed port** — EXI0, the 16-bit, highest-bandwidth channel.
  Its three chip selects reach: (0) the internal IPL ROM / real-time clock,
  (1) the internal flash ROM, and (2) an external device (modem, broadband
  adapter). EXI0 also contains the descrambler used to decrypt the boot ROM, and
  it drives an internal 32 MHz data line that is electromagnetically protected
  from the slower external line.
- **Serial port 1** — EXI1, used for an external device such as a modem.
- **Serial port 2** — EXI2, used for external debug hardware.

### 4.4 Memory card slots

Two slots, wired to the EXI bus. Memory card capacities available at retail were
512 KB, 2 MB and 8 MB (4 Mb / 16 Mb / 64 Mb advertised).

### 4.5 Analog / digital video out

The VI encoder produces an 8-bit digital video stream (`vid[7:0]`) clocked at
27/54 MHz with a composite sync/reset and a select pin. Early boards also exposed
a digital video output; it was dropped on later revisions because almost no users
used it.

## 5. Power and physical

- **Power adapter:** DC 12 V × 3.5 A.
- **Dimensions:** ≈ 4.3" (h) × 5.9" (w) × 6.3" (d).

## 6. References

- `HW/gcspecs.txt` — repo architecture summary (Russian), the primary basis here.
- `HW/IO/ProcessorInterface.md`, `HW/IO/DiskInterface.md`, `HW/IO/AudioInterface.md`.
- `HW/acronyms.txt` — component glossary.
- Internal chip documentation (Flipper top-level pin list) — source of the signal
  groups, summarised here.
