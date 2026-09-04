# GameCube — IBM Gekko Processor

> The Gekko is the console's CPU: a PowerPC 750-derived 32-bit processor with
> custom IBM extensions for the GameCube.

## 1. General characteristics

| Parameter | Value |
|---|---|
| Vendor / design | IBM, based on the PowerPC 750CXe |
| Process | 0.18 µm CMOS with copper interconnects |
| Die size | ≈ 40 mm² |
| Transistors | ≈ 21 million |
| Power | ≈ 5 W |
| Core clock | **486 MHz** (3 × the 162 MHz bus) — note the briefly-circulated
  "485 MHz" figure is treated here as a rounding artifact |
| Performance | ≈ 1125 MIPS; scalar FPU ≈ 1.9 GFLOPS, peak ≈ 10.5 GFLOPS in
  paired-single mode |

## 2. Register model

- **32 × 32-bit** general-purpose registers (`r0–r31`; `r1` is the stack pointer).
- **32 × 64-bit** floating-point registers (`f0–f31`).
- **Condition register (CR)**, **machine state register (MSR)**, **FPSCR** and
  approximately **60 special-purpose registers (SPRs)**.
- The usual PowerPC instruction set (integer, floating-point, load/store,
  branch, system) as documented in `HW/Gekko/*`.

## 3. Cache hierarchy

- **L1 instruction** cache: **32 KB**.
- **L1 data** cache: **32 KB**, 8-way set-associative, physically addressed.
- **L2** cache: **256 KB** on-die unified data cache.

### 3.1 Locked cache (LC) mode

The data cache can be split so that **16 KB** behaves as a normal 4-way cache and
the other **16 KB** becomes a software-managed **locked cache** (scratchpad):

- Enabled by `HID2[LCE]`.
- Lines are allocated into the locked region with the `dcbz_l` instruction
  (typically at the virtual address `0xE0000000`, mapped by a DBAT so the MMU
  does not fault).
- `dcbi` / `dcbf` deallocate lines; when the locked set is exhausted the
  pseudo-LRU policy reallocates a tag.
- Switching between the normal and locked modes is slow (~50 µs).
- A 16 KB section of the effective-address space (`0xE0000000`) is reserved for
  this scratch buffer.

### 3.2 Write-gather buffer (WBUF)

The Gekko includes a small write-gather buffer (about **4 × 32-byte** lines) that
collects individual unhidden stores to a single physical address and, once 32
bytes have accumulated, emits them to Flipper as one **burst** transaction.

- Enabled by `HID2[WPE]`; the target physical address is set in **WPAR** (SPR 921).
- When targeted at `0x0C008000` (the PI/GFX FIFO), the burst is used to push
  graphics command data into the command FIFO.
- Compare is on physical address bits 0–26; `WPAR[BNE]` reports buffer-non-empty.

## 4. Paired Single (SIMD)

Gekko's distinctive extension is **Paired Single**: a 64-bit floating-point
register is treated as **two 32-bit singles** so that two single-precision
operations run in parallel. Dedicated paired-single instructions (load/store,
arithmetic, merges, compare) roughly double scalar FPU throughput, which is the
source of the ~10.5 GFLOPS peak figure. It is the GameCube's analogue of MMX/SSE.

## 5. Memory management unit (MMU)

- Separate **data (DMMU)** and **instruction (IMMU)** translation units.
- **Block address translation** via **DBAT/IBAT** registers — Dolphin OS uses this
  exclusively by default.
- **Segment registers** + **page-table** translation for virtual memory (used by
  GC-Linux and for some direct-ARAM tricks).
- **TLB**: 128-entry, two-way set-associative per MMU.
- Unmapped accesses raise an MMU exception; a protection-violation on a protected
  main-memory region additionally triggers a Flipper memory-interface interrupt.

## 6. Clocks

The core clock is produced by an on-die **PLL** from a reference clock; the
multiplier is selected by the `PLL_EXT` / `PLL_CFG[0:3]` strap pins (the full
multiplier table, 2×–16× and bypass modes, is in `HW/Gekko/PLL_CFG.txt`).

## 7. References

- `HW/gcspecs.txt` — CPU summary (die size, transistors, clock, MIPS/GFLOPS).
- `HW/Gekko/LCache.md` — locked-cache mechanism.
- `HW/Gekko/PLL_CFG.txt` — PLL multiplier table.
- `HW/Gekko/GekkoISA.txt`, `HW/Gekko/SPR.txt` — instruction set and SPR list.
- `HW/acronyms.txt` — WBUF, MMU, TLB, SPR, FPR, CR, BAT glossary.
- US Patent 6,578,132 ("Paired-single" load/store), US Patent 6,859,862
  ("software management of on-chip cache", i.e. locked cache).
- IBM Gekko RISC Microprocessor User's Manual.
