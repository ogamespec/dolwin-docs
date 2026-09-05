# GameCube — Flipper Chipset

> **Flipper** is the single ASIC that acts as the console's northbridge, graphics
> processor, audio DSP and I/O controller. It was designed by **ArtX** (led by
> Wei Yen, largely ex-SGI engineers); ArtX was acquired by **ATI** late in the
> design, but the Flipper design was already complete and not overtly influenced
> by ATI. Everything but the TV-out encoder sits inside this one chip.

## 1. Role and connection

Physically, Flipper sits between the Gekko CPU, the main 1T-SRAM, the ARAM SDRAM,
the disc drive, the audio DAC, the video encoder and every peripheral connector.
It is reached by the CPU only through the Gekko–Flipper bus (32-bit address /
64-bit data, 162 MHz) and it hosts all of the memory-mapped hardware registers.

Flipper internal blocks (see `HW/Flipper_ASIC_Block_Diagram.png`):

| Block | Function |
|---|---|
| **PI** (Processor Interface) | Host of the register space on the Gekko bus; interrupt control; Gekko reset/clock; PI FIFO |
| **MI / MEM** (Memory Interface) | The hub and memory arbiter — services all slaves that touch main memory |
| **CP** (Command Processor) | Reads the graphics command stream (FIFO) and display lists; fetches vertex attributes; vertex cache |
| **XF** (transform/geometry) | Vertex transform, lighting, clipping; fixed-point math |
| **SU** (setup) | Triangle setup |
| **RAS0/RAS1/RAS2** | Primitive, texture-coordinate and color rasterizers |
| **Bump** | Bump-mapping matrix / directive unit |
| **TC / eTM** (texture cache / texture memory) | Texture memory (TMEM) and cache, texture address generation, LOD |
| **TF** | Texture filter (bilinear/trilinear, etc.) |
| **TEV** (Texture Environment) | Per-pixel color/alpha combine, fog, indirect texturing |
| **PE** (Pixel Engine) | Color/Z compare, blending, the **eFB** embedded framebuffer, framebuffer copy/scaling to XFB |
| **IO** | `IO_PI` (register I/F), SI, AI, EXI, DI, IO_MEM (peripheral DMA) |
| **VI** (Video Interface) | Framebuffer readout, H/V scaler, TV encoding |
| **DSP** (audio DSP) | 16-bit signal processor for audio |

The complete register-level, emulator-focused description of the Processor
Interface — the Gekko 60x bus, the 16-bit register access, the interrupt
controller, PI errors, the physical memory map and the Gekko reset — is in
[processor-interface.md](processor-interface.md).

## 2. Memory interface (MI) and memory protection

The memory interface is the arbitration hub. Its bus users (masters) are:

- **PI** (the Gekko)
- **CP** (command processor / GFX)
- **TC** (texture cache)
- **PE** (pixel engine, for EFB reads/writes and copies)
- **IO** (peripheral DMA)
- **DSP**
- **VI** (video frame buffer reads)

plus a write-buffer path and an external controller (`mem_extctl`) that owns the
ARAM SDRAM.

The complete register-level, emulator-focused description of the memory interface —
the arbiter, the per-master queues and data paths, write buffering and coherency,
memory protection, and the external 1T-SRAM ("Splash") interface — is in
[memory-interface.md](memory-interface.md).

### 2.1 Memory map (physical, Flipper)

| Address | Size | Resource |
|---|---|---|
| `0x00000000` | 24 MB | Main RAM (1T-SRAM) |
| `0x08000000` | 2 MB | Embedded framebuffer (EFB) |
| `0x0C000000` | — | Command Processor (CP) |
| `0x0C001000` | — | Pixel Engine (PE) |
| `0x0C002000` | — | Video Interface (VI) |
| `0x0C003000` | — | Peripheral Interface (PI) |
| `0x0C004000` | — | Memory Interface (MI) |
| `0x0C005000` | — | DSP + Audio + ARAM DMA |
| `0x0C006000` | — | Disc Interface (DI) |
| `0x0C006400` | — | Serial Interface (SI) |
| `0x0C006800` | — | External Interface (EXI) |
| `0x0C006C00` | — | Audio Streaming (AIS) |
| `0x0C008000` | — | GFX command FIFO (PI/CP FIFO) |
| `0xFFF00000` | 1 MB | Boot ROM |

A separate *effective*-address map (as seen by the CPU through the MMU) mirrors
this and adds the write-back/write-through (cached/uncached) aliases of main RAM
at `0x80000000` / `0xC0000000` and the locked-cache scratchpad at `0xE0000000`.

### 2.2 Memory protection

MI provides protection for up to **4 regions** of main memory, each a 1024-byte
page with one of four access modes (deny / read-only / write-only / full). A
violation raises one of four dedicated interrupts (`MEM_0`–`MEM_3`).

### 2.3 Undocumented store quirk

The main-memory bus only carries 64-bit transactions. An **uncached byte** or
halfword store therefore ends up writing a full 8-byte aligned block (the byte is
replicated). This is harmless for cached accesses (which use cache lines) but
means `memset()` on uncached memory fills incorrectly. Software exploiting the
replication can clear 8 bytes with one byte store and is noticeably faster than
`memset`.

## 3. Graphics processor (GFX)

The GFX is a **fixed-function** pipeline (no programmable shading) with three main
stages: command processor, geometry/lighting, and rasterizer.

- **Command processor:** an on-chip FIFO for the command stream (commands read in
  32-byte chunks), a call FIFO for display lists (no nesting), a vertex cache and
  vertex-format fetch. Vertex attributes can come from the stream or from arrays
  in main memory (including fixed-point formats).
- **Geometry:** fixed-point math, **8** hardware RGBA light sources with
  diffuse/specular components, angle and distance attenuation, toon shading,
  bump mapping and a 64-entry matrix RAM (plus dual-texture transform).
- **Rasterizer:**
  - **24-bit RGB color**, **24-bit Z** depth buffer.
  - Embedded framebuffer (**eFB/EFB**): **2 MB** of fast 1T-SRAM inside Flipper,
    giving a native resolution of about **640×528**. All drawing happens into the
    EFB; it is then copied (and optionally scaled/filtered) to an external
    framebuffer (**XFB**) in main memory for TV output.
  - **Texture memory (TMEM)** of **2 MB** with a texture cache; textures up to
    **1024×1024**. Formats include indexed TLUT, RGB565, RGB5A3, RGBA, IA4/8 and
    S3TC compression; alpha channel, 3D textures and indirect texturing are
    supported.
  - **Multitexturing** up to 16 texture coordinate sets (8 for the original
    design); mip-mapping, bilinear/trilinear filtering and an anisotropic
    filter (up to 4×).
  - Fog, blending and gamma correction.
  - Stated performance ≈ **6–12 M polygons/s** (≈100–200 k triangles/frame) under
    realistic game conditions.

### 3.1 GFX→Gekko FIFOs

Two FIFOs feed commands to GFX:

- **PI FIFO** — belongs to the Gekko side; the CPU writes burst stores to
  `0x0C008000`, and the write pointer advances by 32 bytes with wrap-around
  control (`CPBAS`, `CPTOP`, `CPWRT`, `CPABT`).
- **CP FIFO** — belongs to the GFX; the command processor reads the stream from
  main memory (base/top/watermarks/pointers via `0x0C00C000`-area CP registers).

## 4. Audio DSP and Audio Interface (AI)

### 4.1 DSP

A 16-bit **Macronix** audio DSP integrated into Flipper, running at **81 MHz**
(1/6 of the CPU clock). Memory:

- **8 KB** instruction RAM (IRAM) + **8 KB** instruction ROM (IROM) — together
  IMEM.
- **8 KB** data RAM (DRAM) + **4 KB** data ROM (DROM, coefficient tables) —
  together DMEM.

It includes a custom ADPCM decoder, an ARAM "accelerator"/cached interface, a DMA
interface to main memory (to boot microcode) and **mailbox registers** for CPU
communication. It mixes up to 64+ 3D-positioned channels with effects such as
reverb, echo and per-channel ADSR, running in parallel with the CPU and GFX.

### 4.2 Audio Interface

- **AI DMA** streams PCM from main memory, in **32-byte blocks**, into a 32-byte
  FIFO that the FIFO drains at **32 kHz or 48 kHz** stereo; a sample-rate
  conversion stage keeps the stream at 48 kHz. Typical DSP sample rate is set via
  the audio DMA registers (`0x0C005030` area).
- **Auxiliary/streaming input (AIS)** receives DVD audio bitstream
  (`aisd`, clocked by `aisclk`, framed by `aislr`) and can be converted to 48 kHz.
- Audio output pins `aid`/`ailr`/`aiclk` drive the stereo DAC; volume for the
  auxiliary channel is in `AIVR`.

## 5. Video Interface (VI)

VI reads the XFB from main memory, performs horizontal scaling and optional
filtering/antialiasing of the framebuffer, and drives the TV encoder. It
generates the vertical-blank interrupt (programmable to any beam position),
supports light-gun position latching (via `guntrg`), interlaced, double-strike and
**progressive (480p)** modes, and PAL/NTSC/M-PAL/PAL60 output. The video buffer is
stored in Y1UY2V-packed format to save bandwidth.

The register-level, emulator-focused description of the VI — complete register
map, H/V timing tables, the horizontal scaler, and the scan-out model — is in
[video-interface.md](video-interface.md).

## 6. Boot ROM / reset

Flipper generates the Gekko reset vector address and the reset signal; on reset
the CPU starts at `0xFFF00100`. The IPL then reads the 2 MB encrypted boot ROM
(exposed on EXI0 and de-scrambled by Flipper), which contains the initialisation
code, the system menu, and fonts for all languages.

## 7. References

- `HW/Flipper_ASIC_Block_Diagram.png` — internal block diagram.
- `HW/GraphicsSystem/GFX.md` — GFX pipeline detail (command processor, XF, setup,
  rasterizers, TEV, PE register space).
- Internal chip documentation (Flipper top-level pin list and memory-arbiter RTL) —
  source of the block and memory-master structure, summarised here.
- `HW/IO/Memory.txt`, `HW/IO/mi.txt` — memory map and MI protection.
- `HW/IO/ProcessorInterface.md` — Gekko–Flipper bus and PI registers.
- `HW/AudioSystem/AudioInterface.md`, `HW/AudioSystem/dsp_info.md`,
  `HW/AudioSystem/DSPCore.md` — AI and DSP.
- `HW/acronyms.txt` — EFB, XFB, TMEM, GP, GX, DSP, MI, VI, WBUF, FIFO glossary.
- US Patents 6,609,977 (External Interfaces), 6,466,218 (Graphics Interface),
  6,999,100 / 6,937,245 (PE EFB / copy), 7,199,710 (GX FIFO), 6,571,328
  (Paired-single), 6,619,804 (Z-clamping).
