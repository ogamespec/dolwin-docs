# GameCube Architecture Overview

This is the top-level architectural overview of the Nintendo GameCube retail
hardware (**HW2**, codename **Dolphin**). It is the grounding document for the
future component-level specifications; each section below links to a dedicated
specification that is maintained in this folder.

The system is built around two large chips on the main CPU board:

- **Gekko** — the 486 MHz PowerPC 750-derived CPU.
- **Flipper** — the single ASIC that is the northbridge, graphics processor,
  audio DSP and I/O controller for the whole console.

Everything else (main memory, ARAM, the disc drive, the peripherals) hangs off
Flipper.

## Sections

| # | Topic | Document | Highlights |
|---|---|---|---|
| 1 | Motherboard, connectors & components | [motherboard.md](motherboard.md) | board layout, system buses (162 MHz / 64-bit), Flipper pin groups, connector pinouts, power |
| 2 | Clock generator | [clock-generator.md](clock-generator.md) | 27 MHz reference oscillator, clock-generator chip & pinout, system clock tree, PLL straps, emulator timing |
| 3 | IBM Gekko processor | [gekko.md](gekko.md) | 486 MHz PowerPC 750, caches, locked cache, write-gather buffer, Paired-Single SIMD, MMU, PLL |
| 4 | Flipper chipset | [flipper.md](flipper.md) | northbridge + GPU + audio DSP + I/O; memory arbitration & protection; graphics pipeline; DSP/AI; VI; boot/reset |
| 5 | Disk Drive | [disk-drive.md](disk-drive.md) | 3" mini-DVD, CAV, copy protection, intelligent MN-10200 drive, DI interface & signals, streaming audio |
| 6 | Peripheral devices (EXI, SI) | [peripherals.md](peripherals.md) | EXI channels/devices/transfer modes, SI channels/polling, controller protocol & hardware |

## Key system parameters (quick reference)

| Parameter | Value |
|---|---|
| Clock reference | 27 MHz crystal → 162 MHz bus, 486 MHz CPU, 81 MHz DSP/ARAM, 27/54 MHz video |
| CPU | IBM Gekko, 486 MHz, PowerPC 750-derivative, Paired-Single |
| System ASIC | Flipper (ArtX/ATI), 162 MHz |
| Main bus | 32-bit address / 64-bit data, 162 MHz, ~1.3 GB/s |
| Main memory | 24 MB 1T-SRAM (2 × 12 MB, "Splash") |
| Auxiliary memory | 16 MB DRAM (ARAM), 8-bit, 81 MHz, ~80 MB/s |
| Embedded framebuffer | 2 MB EFB (≈640×528) |
| Texture memory | 2 MB TMEM, max texture 1024×1024 |
| Audio DSP | 16-bit Macronix, 81 MHz, 8 KB IRAM + 8 KB IROM + 8 KB DRAM + 4 KB DROM |
| Disc | 3" mini-DVD, 1,459,978,240 bytes, CAV, ~2–3 MB/s |
| Power | DC 12 V × 3.5 A |

## Source disciplines

- The hardware facts here are drawn from the repository's internal chip
  documentation, board and RTL sources, and summarised (paraphrased) rather than
  reproduced verbatim.
- The **`HW`, `RE` and `WEB`** folders provide the reverse-engineering,
  documentation and patent references that corroborate and explain the hardware.
- All facts are stated in English; confidential source text is not quoted here.
