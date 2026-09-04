# GameCube — Video Interface (VI)

> **VI** (Video Interface) is the block inside the **Flipper** ASIC that reads the
> external framebuffer (XFB) out of main memory and turns it into a serial video
> stream for the television. It generates all the NTSC / PAL / M-PAL timing an
> external encoder needs, drives the video DAC, and can optionally scale the
> image horizontally. In the retail (HW2, "Dolphin") design the VI is a
> fully-digital generator — it does not mix analogue video; an external
> **Rohm** encoder (or, for progressive output, the encoder path itself) turns
> the digital stream into the TV signal.

This is the register-level, emulator-focused specification. The facts are drawn
from the repository's hardware documentation and reverse-engineering sources and
summarised rather than reproduced verbatim. It describes the **legacy**
(GameCube) VI as sold; the later "enhanced" HD (Conexant) additions are out of
scope and only mentioned where they touch the base design.

## 1. Overview

The VI is a scan-out engine. On every horizontal line it issues cache-line
reads to the memory controller, unpacks the 4:2:2 YUV data, optionally runs it
through a horizontal resampling/anti-aliasing filter, and clocks the result out
on a digital bus together with the generated sync/blanking flags. A vertical
counter and a horizontal (pixel) counter produce the line/field/frame timing.

### 1.1 Sub-blocks

| Block | Role |
|---|---|
| `vi_pi` | Register interface to the Processor Interface (PI); holds all programmer-visible registers |
| `vi_horcnt` | Horizontal (pixel) counter — generates HSYNC, HBLANK, colour-burst position, active-window |
| `vi_vercnt` | Vertical (line) counter — generates VSYNC, VBLANK, field/frame number, equalisation/serration |
| `vi_mem_read` | Address/fetch control — issues cache-line reads, packs pixel stream |
| `vi_mem_write` | Memory side of the read path — drives the bus request/acknowledge handshake |
| `vi_hscaler` | Horizontal resampler (stepper + phase generation) |
| `vi_hf` | 6-tap 8-phase FIR filter (the anti-aliasing low-pass) |
| `vi_rohm` | Output formatter/encoder — serialises pixels, injects sync/blanking flags into Cb/Cr |

The three-component pixel path is:

```
main RAM (YUV422) ──vi_mem_read──▶ vi_hscaler ──▶ vi_hf (FIR) ──▶ vi_rohm ──▶ vdata / vphase
```

## 2. External interfaces

### 2.1 PI / VI register interface

Registers are reached through the Processor Interface. The bus is asynchronous
to the video clock and is sampled/registered on both ends (so a request and its
acknowledge are never on the same cycle). All register data is 16-bit; a 32-bit
CPU access is split by the PI into two 16-bit transfers (high word at the base
address, low word at base+1), and the whole Flipper is big-endian.

| Signal | Width | Direction | Description |
|---|---|---|---|
| `pi_viAddr[8:1]` | 8 | in | Register address (halfword granularity) |
| `pi_viData[15:0]` | 16 | in | Write data |
| `vi_piData[15:0]` | 16 | out | Read data |
| `pi_viRd` | 1 | in | 1 = read, 0 = write |
| `pi_viReq` | 1 | in | One-cycle transaction start |
| `vi_piAck` | 1 | out | One-cycle completion |
| `vi_piVrtIntr` | 1 | out | Vertical interrupt request to the CPU |
| `resetb` | 1 | in | Hardware reset |

### 2.2 VI / encoder interface (VI/EN)

This is the digital output to the video encoder. It is a 10-pin interface
running at **27 MHz** (the pixel-clock domain), and doubles to **54 MHz** only
for the progressive (non-interlaced, "480p") output.

| Pin | Width | Description |
|---|---|---|
| `vclk` | 1 | Video clock (27 MHz, or 54 MHz in progressive mode) |
| `vphase` | 1 | Phase — low selects the YCb set, high the YCr set |
| `vdata` | 8 | YCrCb pixels during active display; timing flags during blanking |
| (`viclk54`/`visel` etc.) | — | Clock select / status pins exposed on the package |

The stream carries **Y, Cb, Cr** at the 4:2:2 rate. During the *blanking*
interval the two chroma bytes (Cb, Cr) are reused to transmit the timing flags,
so the encoder must reconstruct the flags to the full 13.5 MHz line rate; this
means HSYNC and the colour burst can be shifted by one pixel relative to a
pure-analogue implementation.

Two cases matter for the encoder and for emulators that model pixel timing:

- **HBE even** — the first chroma sample of the active line is **Cb**.
- **HBE odd** — the first chroma sample is **Cr**.

### 2.3 VI / memory interface (VI/MEM)

The VI is a bus *master* on the main-memory interface. All reads are
**cache-line sized (32 bytes)** and return over the 64-bit data bus in four
back-to-back clocks (2 bytes of data per clock per lane).

| Signal | Description |
|---|---|
| `vi_memAddr[28:5]` | Cache-line aligned address of the read |
| `vi_memReq` | One-cycle request to enqueue a read |
| `mem_viAck` | One-cycle acknowledge; first 8 bytes (7..0) return on this cycle |
| `mem_viData[63:0]` | 64-bit data; a cache line arrives across 4 clocks |
| `mem_viReqFull` | (Enhanced mode only) stop issuing further requests while asserted |

In legacy (GameCube) mode the VI keeps **one outstanding read request**: a new
request is only issued after the previous one is acknowledged. The picture is
always fetched in-order.

## 3. Addressing and pixel geometry

The VI derives every fetched word from a base pointer plus fields in the
picture-configuration and base registers. It uses **word** units where one word
is **16 pixels** (i.e. one 32-byte cache line for 4:2:2 YUV with 2 bytes/pixel).

The canonical relationships (all lengths in pixels unless noted):

```
word_per_line = ceil(picture_width / 16)          // words per scan line

FBB = picture_origin + x_offset/16 + y_offset * word_per_line   // base (in words)
STD = 2 * word_per_line                            // picture stride in words
XOF = x_offset % 16                                // pixel offset inside first word
WPL = ceil(XOF + display_width / 16)               // fetched words per line
```

`STD` (stride), `WPL` (words/line) and `XOF` are what let the VI window and
**pan** the displayed region at pixel resolution without copying data in main
memory.

### 3.1 Frame buffer format

The external framebuffer (XFB) is stored as packed **YUV 4:2:2** (the "Y1UY2V"
layout), four bytes per two pixels:

```
pixel pair  :  Y0  U0  Y1  V0
byte offset :  +0  +1  +2  +3
```

Each cache line (32 bytes) therefore holds **16 pixels**. `Y` is normally
clamped to the legal video range; during blanking `Y` is forced to zero and the
Cb/Cr positions carry the timing/status flags (see §4.3). Cb and Cr are
averaged to 4:4:4 before filtering — this is what lets the luma and chroma
paths share the same control logic and prevents a chroma read from running
ahead of the luma read.

## 4. Video timing

The heart of the VI is a **timing generator**: a vertical line counter and a
horizontal pixel counter running continuously at the **27 MHz** pixel clock
(13.5 MHz luma/chroma sample rate; 27 MHz in non-interlaced/progressive when
driving the encoder). Their outputs are decoded by a programmable decoder that
implements one of four modes — **NTSC**, **PAL**, **M-PAL**, and a **debug**
mode (a reduced-size format used for bring-up/simulation).

### 4.1 Horizontal timing

The horizontal parameters are programmed in two 32-bit registers. `HLW` is the
half-line width; `HSY`, `HBE`, `HBS` and the burst positions `HCS`/`HCE` are
offsets from the horizontal (sync) reference, all expressed in the counter clock
ticks of the 13.5 MHz luma/chroma sample domain (a full line is two half-lines).

| Symbol | Meaning | Register field |
|---|---|---|
| `HLW` | Half-line width (in 13.5 MHz clock ticks of a half line) | `VI_HORIZ_TIMER0[9:0]` |
| `HCS` | Horizontal blank start → colour-burst start | `VI_HORIZ_TIMER0[30:24]` |
| `HCE` | Horizontal blank start → colour-burst end | `VI_HORIZ_TIMER0[22:16]` |
| `HSY` | HSYNC width | `VI_HORIZ_TIMER1[6:0]` |
| `HBE` | Horizontal blank start → blanking end | `VI_HORIZ_TIMER1[16:7]` |
| `HBS` | Half-line reference → blank start | `VI_HORIZ_TIMER1[26:17]` |

### 4.2 Vertical timing

The vertical parameters are in **half lines**, except `ACV` which is in
**full lines** and is double-buffered (it changes at the start of the
vertical back porch, giving software a full vertical-blank interval to update
the mode).

| Symbol | Meaning | Location |
|---|---|---|
| `EQU` | Equalisation (and serration) pulse length | `VI_VERT_TIMER[3:0]` |
| `ACV` | Active video, in full lines | `VI_VERT_TIMER[13:4]` |
| `PRB` | Pre-blanking, odd or even field (half lines) | `VI_VERT_TIMER_ODD/EVEN[9:0]` |
| `PSB` | Post-blanking, odd or even field (half lines) | `VI_VERT_TIMER_ODD/EVEN[25:16]` |
| `BS1..4` | Burst-blanking start for fields 1..4 | `VI_BLANK_ODD/EVEN` |
| `BE1..4` | Burst-blanking end for fields 1..4 | `VI_BLANK_ODD/EVEN` |

The vertical counter runs on a **frame** basis (for interlace) or a **field**
basis (for non-interlace). It is a *frame* line count, so on a 525-line NTSC
frame, vertical count 264 is the first full line of the second field and 525 is
the last line of the frame; the four fields are numbered 1–4. In non-interlaced
mode the same 1–525 numbering is retained even though only the odd fields are
shown.

### 4.3 The blanking / status flags

During vertical blanking, the timing flags ride on the Cb and Cr byte
positions. They are produced by the horizontal and vertical counters and are
individually invertible via the output-polarity register:

| Bit | Flag | Meaning (active low unless inverted) |
|---|---|---|
| 7 | `C` | Composite sync |
| 6 | `F` | Field flag — 0 for odd fields (1,3,…), 1 for even fields (2,4,…) |
| 5 | `V` | Vertical sync |
| 4 | `H` | Horizontal sync |
| 3 | `B` | Burst flag — active once per line where the colour burst sits |
| 2 | `K` | Burst-blank flag — blank the colour burst on this line |
| 1 | `N` | NTSC (0) vs PAL / M-PAL (1) mode flag |
| 0 | `I` | Interlace (0) vs non-interlace (1) flag |

### 4.4 Nominal timing values — NTSC (525 lines)

| Symbol | Interlace | Non-interlace | Progressive (480p) |
|---|---|---|---|
| `EQU` | 6 | 6 | 12 |
| `ACV` | 241 | 241 | 480 |
| `PRB` (odd/even) | 24 / 25 | 24 / 24 | 44 / 44 |
| `PSB` (odd/even) | 1 / 0 | 2 / 2 | 10 / 10 |
| `BS1..4` | 12/13/12/13 | 12/12/12/12 | 24/24/24/24 |
| `BE1..4` | 520/519/520/519 | 520/520/520/520 | 1038 / 1038 / 1038 / 1038 |
| `HLW` | 429 | 429 | — |
| `HSY` | 64 | 64 | — |
| `HCS` | 71 | 71 | — |
| `HCE` | 105 | 105 | — |
| `HBE` | 162 | 162 | — |
| `HBS` | 373 | 373 | — |

In non-interlaced NTSC only the odd fields are shown (≈ 60 Hz but 263 lines per
field rather than 262.5). For progressive, the value of `HBE`/`HBS` is computed
from the desired line width; the nominal 640-wide values are 162 and 373.

### 4.5 Nominal timing values — PAL (625 lines)

| Symbol | Interlace | Non-interlace | Progressive |
|---|---|---|---|
| `EQU` | 5 | 5 | 10 |
| `ACV` | 287 | 287 | 576 |
| `PRB` (odd/even) | 35 / 36 | 35 / 35 | 58 / 58 |
| `PSB` (odd/even) | 1 / 0 | 2 / 2 | 10 / 10 |
| `BS1..4` | 13/12/11/10 | 13/11/13/11 | 20/20/20/20 |
| `BE1..4` | 619/618/617/620 | 619/621/619/621 | 1240 / 1240 / 1240 / 1240 |
| `HLW` | 432 | 432 | — |
| `HSY` | 64 | 64 | — |
| `HCS` | 75 | 75 | — |
| `HCE` | 106 | 106 | — |
| `HBE` | 172 | 172 | — |
| `HBS` | 380 | 380 | — |

### 4.6 Nominal timing values — M-PAL

| Symbol | Interlace | Non-interlace |
|---|---|---|
| `EQU` | 6 | 6 |
| `ACV` | 241 | 241 |
| `PRB` (odd/even) | 24 / 25 | 24 / 24 |
| `PSB` (odd/even) | 1 / 0 | 2 / 2 |
| `BS1..4` | 16/15/14/13 | 16/14/16/14 |
| `BE1..4` | 518/517/516/519 | 518/520/518/520 |
| `HLW` | 429 | 429 |
| `HSY` | 64 | 64 |
| `HCS` | 78 | 78 |
| `HCE` | 112 | 112 |
| `HBE` | 162 | 162 |
| `HBS` | 373 | 373 |

### 4.7 Progressive mode

The 525-line progressive (480p) mode uses a **54 MHz** video clock (selected by
the clock-select register) and produces the same line rate as NTSC (525 lines)
or, if programmed for 625, the same line rate as PAL. Non-interlaced fields run
at just under 60 fps (263 lines) / just under 50 fps (313 lines) because the
half-line framing of interlace is not used. There is **no colour burst** in
progressive mode, so `HCS`/`HCE` and `BS`/`BE` are left unprogrammed.

## 5. Register map

The VI occupies 0xCC002000–0xCC002078 in the peripheral register space (the same
region is mapped read/write at `0x0C002000` in the uncached physical map). Each
word is 16-bit; 32-bit registers are two adjacent words, high word first. Reset
values are shown for the registers that have a defined power-on value (`–` means
undefined/not initialised).

| Addr | Width | Name | R/W | Description |
|---|---|---|---|---|
| `0x00` | 16 | `VI_VERT_TIMER` | R/W | Vertical timing — `EQU`, `ACV` |
| `0x02` | 16 | `VI_DISPLAY_CFG` | R/W | Enable / reset / mode / gun trigger enable |
| `0x04` | 32 | `VI_HORIZ_TIMER0` | R/W | Horizontal timing — `HLW`, `HCE`, `HCS` |
| `0x08` | 32 | `VI_HORIZ_TIMER1` | R/W | Horizontal timing — `HSY`, `HBE`, `HBS` |
| `0x0C` | 32 | `VI_VERT_TIMER_ODD` | R/W | Odd-field vertical blanking — `PRB`, `PSB` |
| `0x10` | 32 | `VI_VERT_TIMER_EVEN` | R/W | Even-field vertical blanking — `PRB`, `PSB` |
| `0x14` | 32 | `VI_BLANK_ODD` | R/W | Odd-field colour-burst blank interval — `BS1/3`, `BE1/3` |
| `0x18` | 32 | `VI_BLANK_EVEN` | R/W | Even-field colour-burst blank interval — `BS2/4`, `BE2/4` |
| `0x1C` | 32 | `VI_TOP_BASE_L` | R/W | Top-field (left) base pointer + pan |
| `0x20` | 32 | `VI_TOP_BASE_R` | R/W | Top-field right base pointer (3D mode) |
| `0x24` | 32 | `VI_BOTTOM_BASE_L` | R/W | Bottom-field (left) base pointer |
| `0x28` | 32 | `VI_BOTTOM_BASE_R` | R/W | Bottom-field right base pointer (3D mode) |
| `0x2C` | 32 | `VI_RASTER_POS` | RO | Current beam position (`HCT`, `VCT`) |
| `0x30` | 32 | `VI_INT_0` | R/W | Display interrupt 0 |
| `0x34` | 32 | `VI_INT_1` | R/W | Display interrupt 1 |
| `0x38` | 32 | `VI_INT_2` | R/W | Display interrupt 2 |
| `0x3C` | 32 | `VI_INT_3` | R/W | Display interrupt 3 |
| `0x40` | 32 | `VI_LATCH_0` | R/W | Light-gun latch 0 |
| `0x44` | 32 | `VI_LATCH_1` | R/W | Light-gun latch 1 |
| `0x48` | 16 | `VI_PICTURE_CFG` | R/W | Picture geometry — `STD`, `WPL` |
| `0x4A` | 16 | `VI_HSCALE` | R/W | Horizontal-scaler step + enable |
| `0x4C` | 32 | `VI_FILTER0` | R/W | Filter taps `T0..T2` |
| `0x50` | 32 | `VI_FILTER1` | R/W | Filter taps `T3..T5` |
| `0x54` | 32 | `VI_FILTER2` | R/W | Filter taps `T6..T8` |
| `0x58` | 32 | `VI_FILTER3` | R/W | Filter taps `T9..T12` |
| `0x5C` | 32 | `VI_FILTER4` | R/W | Filter taps `T13..T16` |
| `0x60` | 32 | `VI_FILTER5` | R/W | Filter taps `T17..T20` |
| `0x64` | 32 | `VI_FILTER6` | R/W | Filter taps `T21..T24` |
| `0x6A` | 16 | `VI_OUTPOL` | R/W | Output-polarity inversion of the control flags |
| `0x6C` | 16 | `VI_CLOCK_SEL` | R/W | 27 MHz (0) vs 54 MHz (1) video clock |
| `0x6E` | 16 | `VI_DTV_STATUS` | RO | DTV/Conexant select pin status |
| `0x70` | 16 | `VI_WIDTH` | R/W | Horizontal-scaler source width (`SRCWIDTH`) |
| `0x72` | 16 | `VI_HBE656` | R/W | Border/CCIR-656 blank-end + border enable |
| `0x74` | 16 | `VI_HBS656` | R/W | Border/CCIR-656 blank-start |

The register addresses above are byte offsets from `0xCC002000`; a 32-bit CPU
read/write at the base address covers the two 16-bit words at `+0` and `+2`.

## 6. Register fields

### 6.1 `VI_VERT_TIMER` (0x00, 16-bit)

Vertical timing register. `ACV` is double-buffered and takes effect at the
start of the vertical back porch.

| Bits | Field | Description |
|---|---|---|
| 3:0 | `EQU` | Equalisation-pulse length in half lines |
| 13:4 | `ACV` | Active video height in full lines |

### 6.2 `VI_DISPLAY_CFG` (0x02, 16-bit)

Configures and enables the VI. It is normally reset first so the counters and
request logic start from a known state. Reset value `0x0000`.

| Bits | Field | Description | Reset |
|---|---|---|---|
| 0 | `ENB` | Enable video timing generation and data request | 0 |
| 1 | `RST` | Clear all data requests and force the idle state | 0 |
| 2 | `NIN` | 0 = interlace, 1 = non-interlace (only top field displayed) | 0 |
| 3 | `DLR` | 3D display mode select | 0 |
| 5:4 | `LE0` | Latch-enable mode for light-gun 0 (0=off, 1=1 field, 2=2 fields, 3=always) | 0 |
| 7:6 | `LE1` | Latch-enable mode for light-gun 1 | 0 |
| 9:8 | `FMT` | Video format: 0=NTSC, 1=PAL, 2=M-PAL, 3=Debug (CCIR-656) | 0 |

### 6.3 `VI_HORIZ_TIMER0` (0x04, 32-bit)

Horizontal timing. Reset value undefined.

| Bits | Field | Description |
|---|---|---|
| 9:0 | `HLW` | Half-line width |
| 22:16 | `HCE` | Horizontal blank start → colour-burst end |
| 30:24 | `HCS` | Horizontal blank start → colour-burst start |

### 6.4 `VI_HORIZ_TIMER1` (0x08, 32-bit)

Horizontal timing. Reset value undefined.

| Bits | Field | Description |
|---|---|---|
| 6:0 | `HSY` | Horizontal-sync width |
| 16:7 | `HBE` | Horizontal blank start → blanking end |
| 26:17 | `HBS` | Half-line reference → blank start |

### 6.5 `VI_VERT_TIMER_ODD` / `EVEN` (0x0C / 0x10, 32-bit)

Odd/even-field vertical blanking. `PRB`/`PSB` are double-buffered. Reset value
undefined.

| Bits | Field | Description |
|---|---|---|
| 9:0 | `PRB` | Pre-blanking, in half lines (odd field in the low words) |
| 25:16 | `PSB` | Post-blanking, in half lines |

### 6.6 `VI_BLANK_ODD` / `EVEN` (0x14 / 0x18, 32-bit)

Colour-burst blanking interval per field. Reset value undefined.

| Bits | Field | Description |
|---|---|---|
| 4:0 | `BS1` | Field 1 → burst-blanking start (half lines) |
| 15:5 | `BE1` | Field 1 → burst-blanking end (half lines) |
| 20:16 | `BS3` | Field 3 → burst-blanking start (half lines) |
| 31:21 | `BE3` | Field 3 → burst-blanking end (half lines) |

`VI_BLANK_EVEN` uses the same layout for fields 2 (`BS2`/`BE2`) and 4
(`BS4`/`BE4`).

### 6.7 `VI_TOP_BASE_L` (0x1C, 32-bit)

Display origin of the top (odd) field in 2D mode, or the left picture in 3D
mode. Reset value undefined.

| Bits | Field | Description |
|---|---|---|
| 23:0 | `FBB` | Frame-buffer address (in words) |
| 27:24 | `XOF` | Horizontal pixel offset of the left-most pixel inside the first word |
| 28 | `HIGH` | 0 = treat `FBB` as a 24-bit address (lower 24 MB); 1 = use the full 28-bit address (512 MB) |

The 3D/right variants (`VI_TOP_BASE_R`, `VI_BOTTOM_BASE_L`,
`VI_BOTTOM_BASE_R`) carry only the 24-bit `FBB` field; the right pointers are
unused in 2D mode.

### 6.8 `VI_RASTER_POS` (0x2C, 32-bit, read-only)

Current beam position. `HCT` runs from 1..pixels-per-line, reset to 1 at the
start of every line; `VCT` runs on a frame basis from 1..lines-per-frame, and is
1 at the start of pre-equalisation.

| Bits | Field | Description |
|---|---|---|
| 10:0 | `HCT` | Horizontal count (pixels) |
| 26:16 | `VCT` | Vertical count (lines) |

### 6.9 `VI_INT_0`..`VI_INT_3` (0x30 / 0x34 / 0x38 / 0x3C, 32-bit)

Four programmable display interrupts. Each has its own enable and status bit;
the interrupt is cleared by writing a zero to the `INT` status bit (in SDK
practice the handler writes back the current `INT` position to clear it).

| Bits | Field | Description | Reset |
|---|---|---|---|
| 10:0 | `HCT` | Horizontal count at which to fire | 0 |
| 26:16 | `VCT` | Vertical count at which to fire | 0 |
| 28 | `ENB` | Interrupt enabled | 0 |
| 31 | `INT` | Interrupt active status (1 = pending) | 0 |

Whenever any VI interrupt fires it also raises the VI master interrupt in the PI,
which is what the OS uses as the single retrace callback.

### 6.10 `VI_LATCH_0` / `VI_LATCH_1` (0x40 / 0x44, 32-bit)

Light-gun latches. They capture the display position at the rising edge of the
gun-trigger input; the trigger flag is set and cleared by writing zero.

| Bits | Field | Description | Reset |
|---|---|---|---|
| 10:0 | `HCT` | Horizontal count at trigger | 0 |
| 26:16 | `VCT` | Vertical count at trigger | 0 |
| 31 | `TRG` | Trigger flag | 0 |

### 6.11 `VI_PICTURE_CFG` (0x48, 16-bit)

Picture geometry. The hardware reset value is undefined; the system library
programs `0x2828` on initialisation.

| Bits | Field | Description |
|---|---|---|
| 7:0 | `STD` | Stride per line, in words |
| 14:8 | `WPL` | Number of reads (words) per line |

### 6.12 `VI_HSCALE` (0x4A, 16-bit)

Horizontal scaler. Reset value `0x0100` (step `0x100`/256, scaler off).

| Bits | Field | Description | Reset |
|---|---|---|---|
| 8:0 | `STP` | Stepper step size, U1.8 | 0x100 (256) |
| 12 | `HS_EN` | Horizontal-scaler enable | 0 |

### 6.13 `VI_FILTER0`..`VI_FILTER6` (0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64; 32-bit)

The 6-tap × 8-phase resampling filter is symmetric; only half is programmed.
The first three tables (`VI_FILTER0..2`) hold the three "centre" taps each —
`T0..T8` in the range `[0.0, 2.0)` as unsigned `U1.9` values:

| Bits | Field |
|---|---|
| 9:0 | `T0` (U1.9) |
| 19:10 | `T1` |
| 29:20 | `T2` |

`VI_FILTER0` = `T0,T1,T2`; `VI_FILTER1` = `T3,T4,T5`; `VI_FILTER2` = `T6,T7,T8`.
`VI_FILTER3..6` hold the "outer" taps `T9..T24` in the signed range
`[-0.125, 0.125)` as `S-2.9`, four 8-bit taps per table:

| Bits | Field |
|---|---|
| 7:0 | `Tn` |
| 15:8 | `Tn+1` |
| 23:16 | `Tn+2` |
| 31:24 | `Tn+3` |

`T24` (in `VI_FILTER6`) is hard-wired to zero.

### 6.14 `VI_OUTPOL` (0x6A, 16-bit)

Inverts each outgoing control flag individually. Reset `0x0000`.

| Bits | Field | Description |
|---|---|---|
| 0 | `I_POL` | Invert interlace flag |
| 1 | `N_POL` | Invert NTSC flag |
| 2 | `K_POL` | Invert burst-blank flag |
| 3 | `B_POL` | Invert burst flag |
| 4 | `H_POL` | Invert HSYNC flag |
| 5 | `V_POL` | Invert VSYNC flag |
| 6 | `F_POL` | Invert field flag |
| 7 | `C_POL` | Invert composite-sync flag |

### 6.15 `VI_CLOCK_SEL` (0x6C, 16-bit)

Selects the video clock. Reset `0x0000`.

| Bits | Field | Description |
|---|---|---|
| 0 | `VICLKSEL` | 0 = 27 MHz, 1 = 54 MHz (progressive only) |

### 6.16 `VI_DTV_STATUS` (0x6E, 16-bit, read-only)

Returns the state of the DTV/Conexant selection pins (`VISEL[1:0]`).

### 6.17 `VI_WIDTH` (0x70, 16-bit)

The number of *source* pixels to be scaled — used only when the horizontal
scaler is enabled (e.g. 320 when scaling 320→640).

| Bits | Field | Description |
|---|---|---|
| 9:0 | `SRCWIDTH` | Source pixel count | 

### 6.18 `VI_HBE656` (0x72, 16-bit)

Border/CCIR-656 blank-end. In debug/CCIR-656 mode the encoder may only accept
720 active pixels, so the border registers implement a black border: the border
`HBE`/`HBS` can be programmed for the encoder's 720-active-pixel window while the
main `HBE`/`HBS` still reflect the real active width, letting the frame buffer
be any width without adding a border in memory.

| Bits | Field | Description | Reset |
|---|---|---|---|
| 9:0 | `HBE656` | Border horizontal-blank-end | 0 |
| 15 | `BRDR_EN` | Border enable | 0 |

### 6.19 `VI_HBS656` (0x74, 16-bit)

| Bits | Field | Description |
|---|---|---|
| 9:0 | `HBS656` | Border horizontal-blank-start |

## 7. Horizontal scaler

The scaler lets a small frame buffer be stretched horizontally to the display
width. It is a **6-tap resampling** filter with **eight** phases per tap plus a
zero tap (49 taps in total). The mechanism:

- The Y, Cb and Cr streams are buffered in three shift registers; the first and
  last pixels are replicated at the boundaries so the filter always has a full
  neighbourhood.
- The new sampling position comes from a **stepper** incremented by the step
  size for every output pixel. The low 8 bits of the stepper are rounded to
  3 bits to select the filter phase; the single bit immediately left of the
  binary point, when it toggles, shifts a new input pixel into the registers.
- The datapath is time-shared between the three components (the video clock is
  twice the pixel rate).

```
step_size = floor(256 * destination_size / source_size) / 256
```

The step size and coefficients are programmed through the register interface; the
cut-off frequency, pass-band ripple and stop-band attenuation are therefore
software-controlled. Because the filter is symmetric, only half the coefficients
are stored: the centre 16 are in `[0,2.0)` and the outer 32 in `[-0.125,0.125)`.

**Default taps.** The system library programs a default 24-tap filter when the
VI is initialised (the scaler is left disabled). The un-packed tap values in
`U1.9`/`S-2.9` notation are:

```
T0  T1  T2  T3  T4  T5  T6  T7
0.969 0.922 0.840 0.727 0.578 0.428 0.278 0.137
T8  T9  T10 T11 T12 T13 T14 T15
0.023 0.442 0.397 0.375 0.405 0.434 0.461 0.461
T16 T17 T18 T19 T20 T21 T22 T23
0.492 0.016 0.029 0.037 0.037 0.029 0.023 0.016
```

(The three `VI_FILTER0..2` tables store `T0..T8`; the four `VI_FILTER3..6`
tables store `T9..T23`; `T24` is hard-wired to zero.) The default source width
programmed by the OS is 640.

## 8. Gun-trigger (light-gun) and 3D display

**Gun trigger.** Two inputs latch the current beam position. The latch-enable
field (`LE0`/`LE1`) is double-buffered, so it becomes active on the next field.
Three sampling modes exist:

- **one-field** — armed for one field, auto-disarms when a trigger is seen or at
  time-out;
- **two-field** — armed for two fields, same auto-disarm;
- **continuous** — armed until explicitly disabled.

Only the *first* trigger within a field is latched; later triggers in that field
are ignored.

**3D display.** Two frame buffers (left and right) can be merged into one stream
that alternates between the two pictures every two pixels. The left/right base
and bottom-base registers hold the second picture pointer; the right picture is
unused in 2D mode.

## 9. Emulator notes

To model the VI in software:

1. **Clock.** The VI pixel clock is 27 MHz; the horizontal counter advances every
   27 MHz tick (13.5 MHz luma/chroma). In progressive mode the clock is 54 MHz.
2. **Counters.** Maintain a horizontal counter (pixels per line, 1-based) and a
   vertical counter (lines per frame, 1-based, reset at pre-equalisation).
   Derive HSYNC, HBLANK, colour-burst and the active window from `HCS/HCE/HSY/
   HBE/HBS`; derive VSYNC, VBLANK, field/frame number and equalisation/serration
   from `EQU/ACV/PRB/PSB/BS/BE`.
3. **Field/frame.** The field flag toggles each field; combine interlaced fields
   with the standard half-line offset. In non-interlace only the odd field is
   shown, and the count is on the same 1..525 basis.
4. **Fetch.** Issue 32-byte cache-line reads from the computed `FBB` with the
   stride `STD`; `XOF`/`WPL` give the per-line fetch window. Read one word =
   16 pixels of YUV 4:2:2.
5. **Pixel format.** Each cache line is 16 pixels: `Y0 U0 Y1 V0 Y2 U1 Y3 V1 …`
   (4 bytes per pixel pair). `U`/`V` are the Cb/Cr chroma.
6. **Scaler.** When `HS_EN` is set, resample horizontally with the 6-tap ×
   8-phase filter using the programmed `STP` (step) and `SRCWIDTH` (source
   count); the default step is 0x100 and the default taps are given above.
7. **Interrupt / retrace.** `VI_INT_0..3` fire on matched `HCT`/`VCT`; any of
   them also raises the VI master interrupt, which the OS uses as the "retrace"
   callback. It is cleared by writing a zero to the `INT` status bit.

## 10. References

- `HW/IO/vi.htm` — register-level summary of the VI register block.
- `HW/Flipper_ASIC_Block_Diagram.png` — VI location within Flipper.
- `RE/VI/vi.asm`, `RE/VI/vistrucs.h` — notes on the system VI library (default
  timings, taps and register/address usage).
- **US Patent 6,609,977** (Video / external interfaces) — the register field
  descriptions trace back to this patent.
