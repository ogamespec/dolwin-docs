# GameCube — Processor Interface (PI)

> **PI** (the Processor Interface) is the block inside the **Flipper** ASIC that sits
> directly on the **Gekko** CPU's 60x bus. It is the host of the whole Flipper
> register space, the interrupt controller that gathers every other block's
> interrupt into a single line to the CPU, the path that drives the Gekko clock and
> reset, and the funnel through which the CPU reaches main memory, the
> embedded/enhanced framebuffer (EFB), the graphics command FIFO and the boot
> ROM. In short, PI is the only path from the CPU into the rest of the console:
> everything else on Flipper is reached through it.

This is the register-level, emulator-focused specification. The facts are drawn
from the repository's hardware documentation, the Flipper RTL and the
system-library reverse-engineering notes, and are summarised (paraphrased) rather
than reproduced verbatim. Where a behaviour is only implicit in the RTL — a
time-out, the exact meaning of a field — it is called out as such.

## 1. Overview

The Gekko bus is a standard PowerPC **60x** interface (32-bit address, 64-bit
data, running at **162 MHz**). PI converts the synchronous 60x transactions into
the request/acknowledge protocols used by the individual Flipper blocks, each of
which has its own point-to-point connection into PI.

### 1.1 Sub-blocks

| Block | Role |
|---|---|
| `pi_dispatch` | The heart of PI — decodes every incoming 60x transaction into a destination and an IO-space module, manages the four-entry dispatch table, the address-acknowledge (AACK) handshake, the sync/eieio barrier and the read/write datapaths |
| `pi_regspace` | The PI's own register file: interrupts (cause/mask), the CPU-FIFO pointers, the error-address/status latches, reset control, drive-strength and chip-id |
| `pi_return` | The CPU-side return path — generates the transfer acknowledge (TA), the data output enables and the byte masks, and routes read data back to the 64-bit bus |
| `pi_mod` | Generic IO-module interface, instantiated once for **CP**, **PI**, **VI** and **DSP**; splits a 32-bit access into two 16-bit transfers and performs the request/ack handshake |
| `pi_mem` | The main-memory path — issues single-beat and burst reads/writes to the memory controller, drives the write buffer, and handles GFX-FIFO writes |
| `pi_pe` | The Pixel-Engine (EFB) path — 64-bit reads/writes to the embedded framebuffer |
| `pi_ei` | The external-interface path — boot ROM and the IO-side peripherals |
| `pi_pref` | The prefetch engine — a small four-slot table that opportunistically fetches the next memory line |
| `pi_err` / `pi_errmod` | Error detection and the error slave that absorbs illegal transactions |

### 1.2 Routing model

Every 60x transaction is decoded once, in `pi_dispatch`, into:

- a **destination** (`Dstn`) — main memory, the EFB, the IO/register space, the CP
  graphics FIFO, or the ROM;
- and, for an IO-space access, an **IO-space module** (`IOsp`) — CP, PE, VI, PI,
  MEM, DSP, IO (external interface), or the error slave.

The dispatch table holds up to **four** outstanding transactions. A transaction is
removed only when the target module has accepted (for a read) or completed (for a
write) it. The table begins to **block further AACK** (see §2.6) once **three**
entries are occupied, because a prefetch transaction needs two slots.

### 1.3 The 60x bus connection

PI presents the Gekko with the classic 60x pins and a single active-low
interrupt, and drives the core clock and reset to the CPU:

| Signal | Width | Direction | Description |
|---|---|---|---|
| `A[0:31]` | 32 | out (to bus) | Address |
| `DL`/`DH[0:31]` | 64 | in/out | Data (two 32-bit halves, big-endian) |
| `TS` | 1 | in | Transfer start |
| `AACK` | 1 | out | Address acknowledge (address bus latched) |
| `TA` | 1 | out | Transfer acknowledge (data beat accepted) |
| `TBST` | 1 | in | Transfer burst (32-byte transfer) |
| `TSIZ[0:2]` | 3 | in | Transfer size (see §2.2) |
| `TT[0:4]` | 5 | in | Transfer type (see §2.3) |
| `INT` | 1 | out | Interrupt request (active low) |
| `HRESET` | 1 | out | Hardware reset; CPU restarts at `0xFFF0_0100` |
| `TRST` | 1 | out | JTAG test reset (asserted with `HRESET`) |
| `SYSCLK` | 1 | out | 486 MHz core clock to the CPU |

## 2. Gekko 60x bus transactions

### 2.1 Transfer sizes (`TSIZ`)

`TSIZ` encodes the transfer size. Burst is signalled by `TBST` rather than by
`TSIZ`; `TSIZ` is only meaningful for single-beat transfers.

| `TBST` | `TSIZ[0:2]` | Size |
|---|---|---|
| 1 | — | **Burst** (32 bytes) |
| 0 | `000` | 8 bytes |
| 0 | `100` | 4 bytes |
| 0 | `010` | 2 bytes |

A **1-byte** transfer (`TSIZ = 001`) is not a valid size on this bus and raises a
PI error (see §5). The byte-enable/mask for writes is derived by PI from the
address bits, so a sub-word access writes only the addressed bytes. Because the
bus carries only 64-bit data, an uncached **byte** or **halfword** store is written
as a whole 8-byte aligned block with the byte/halfword replicated into its lane —
harmless for cached (cache-line) stores, but it is why uncached `memset`-style
copies and fast clear routines behave as they do (see
[memory-interface.md](memory-interface.md)).

### 2.2 Transfer type (`TT`)

`TT[0:4]` names the type of transfer. The 60x-like codes include a number of
address-only and cache-coherency operations that do not apply to a single-CPU
system; only the following are meaningful to Flipper:

| `TT` | Name | Meaning |
|---|---|---|
| `00010` | single-beat write | One write to memory / IO / EFB |
| `00110` | burst write | 32-byte write (cache line, write-gather flush) |
| `01010` | single-beat read | One read from memory / IO / EFB / ROM |
| `01110` | burst read | 32-byte read (cache fill) |
| `01000` | sync | Barrier (see §2.5) |
| `10000` | eieio | Ordering barrier (see §2.5) |
| `01011` | prefetch hint | Prefetch the next memory line (see §2.4) |
| `10010` / `11010` / `11110` | atomic single/burst/forward | Address-only "atomic" controls (no data) |
| `10100` / `11100` | single/burst with EC | Cache-control encodings (no data) |

The remaining address-only codes (`00100`, `01100`, `11000`, `00001`, `01001`,
`01101`) are accepted as ordering/control operations and move no data. All
atomic/address-only and cache-coherency variants are effectively **not
implemented** in the single-CPU Flipper system: there is no snooping or
coherency between the CPU and any other master. PI only acts on data
read/write, the two barriers, and the prefetch hint.

### 2.3 Burst vs single-beat data movement

For the **memory and EFB** paths, a *single-beat* transfer and a *burst* transfer
are both presented to the memory controller as a **32-byte burst** — PI keeps a
12-entry by 64-bit burst write buffer to smooth the CPU's write stream, and reads
are issued as 32-byte line reads. The latency for a sub-line request is therefore
the same as for a full line. The main-memory and EFB paths share this one write
buffer, so a full memory write-queue also stalls PI's EFB writes.

For the **IO/register space**, the transfer width is fixed: only **2-byte** and
**4-byte** single-beat accesses are accepted; a **burst** (or an 8-byte single
beat) to the register space is a PI error.

The **external-interface path** (`pi_ei`) — the IO register space at
`0x0C00_6000` and the boot-ROM region — takes this even further: every request
is broken into **2-byte (halfword) sub-transfers**. A 4-byte IO register access
is two halfword accesses, an 8-byte ROM read is four, and a 32-byte ROM burst
is sixteen back-to-back halfword reads. This is the concrete reason the register
data path is 16-bit. Boot-ROM reads also share a 256-bit return buffer with EFB
reads and are gated on the PE being idle.

### 2.4 Prefetch

PI implements a small, explicit prefetch engine (`pi_pref`). A prefetch hint
transaction (`TT = 0b01011`, always a burst read) causes PI to fetch the
**next line** (request address `+ 0x20`) and place it in a dedicated prefetch
buffer rather than in the normal read-return path. Later, an ordinary read whose
address matches a still-valid prefetch line is **served from the buffer** instead
of generating another memory read — saving a full memory latency.

- Prefetch is only performed for **main memory, EFB and ROM** destinations.
- A read that matches the prefetched address **and** is 32-byte aligned consumes
  the prefetched data (`used`). An unaligned or non-burst read, or a write to the
  same address, **kills** the prefetch line.
- The table holds **four** entries. A prefetch transaction consumes **two** of the
  dispatch-table slots (one for the read request, one for the prefetch part),
  which is why the table blocks AACK at three slots.
- On **sync / eieio**, all outstanding prefetch lines are invalidated.

The prefetch engine does **not** exist to speed up CPU reads in general — Gekko
already has a hardware cache and issues normal 32-byte burst reads. It is there
to serve the narrow case where software issues an explicit prefetch hint and then
reads the same line; emulators can safely model it as a no-op for correctness.

### 2.5 sync / eieio

`sync` and `eieio` are the only ordering operations with a concrete effect. They
**flush the write buffers in both MEM and PE** (so any pending CPU/GFX writes are
committed to memory) and **wait until**:

1. no transaction is still in the dispatch table,
2. no read is outstanding,
3. no prefetch is outstanding,
4. every module is idle, and
5. both MEM and PE have finished their flush.

Only when all of these hold does PI deassert the barrier and let the bus proceed.
In practice the CPU uses `sync` before starting a DMA that reads a buffer it just
wrote, and `eieio` to order MMIO accesses.

### 2.6 Transfer acknowledge / data-return

PI issues `AACK` the cycle after `TS` for every accepted transaction. From the
point of view of software, a read is complete when `TA` is returned with the
data on the 64-bit bus; a write is complete when `TA` is returned. Data is
returned big-endian: the 8-byte read for a burst is the first (lowest) double-word
first, then the next, and so on.

Keys for emulation:

- A **read** returns the **critical double-word first**, then wraps to the rest of
  the line — an emulator models this by serving the requested double-word.
- A **burst** returns **four** 8-byte beats back-to-back (in-order).
- Writes are accepted in the order the CPU presents them; PI flushes the write
  buffer as one burst to memory.

### 2.7 What PI does not implement

A number of 60x-bus capabilities are simply absent in the single-CPU Flipper
design, and it is worth knowing what can be ignored:

- **No cache coherency snooping.** The CPU never snoops the other masters
  (DSP, IO, CP, PE, VI) and there is no hardware to keep their reads coherent
  with the CPU's cached writes. Coherence is achieved only by explicit
  flushing: a DMA writer (DSP/IO/PE) flushes its write buffer before raising its
  interrupt, and the CPU executes `sync` (which flushes the write buffer) before
  starting a read DMA.
- **No write-combining inside PI.** PI has only a small write buffer. The
  CPU-side write-gather pipe (the 128-byte gather buffer flushed in 32-byte
  bursts) is a **Gekko** feature, not part of PI.
- **The fast write-acknowledge path is unused.** The write-transfer acknowledge
  is driven only by the write-buffer-available path; the fast path is dead in the
  shipped design.
- **The read byte-mask is dead.** The read-side `Rmsk` logic is present but
  unused/removable; write data is qualified by a separate `[1:0]` write mask.
- **Boot ROM is read-only.** A store to `0xFFF0_0000` (or any write to the ROM
  destination) is not serviced — it is routed to the error block and raises a
  PI error (code `101`).
- **The GFX FIFO is write-only.** A read of the FIFO destination is an error
  (code `110`); writes to it are treated as (special) memory writes.

The consequence is that PI does not implement a full bus-sniffing
PowerPC-style coherency protocol: it is a simple address/transfer router plus an
interrupt controller, which is all a single-CPU console needs.

## 3. Register access

### 3.1 The register bus is 16-bit

Every Flipper register — in PI and in every other block — is reached through a
**16-bit** data path. `pi_*_addr` is a halfword-granularity address (`[8:1]`) and
`pi_*_data` is `[15:0]`. There is no 32-bit register port.

A **32-bit** CPU access is therefore split by PI into **two 16-bit transfers**,
**big-endian**: the **high word** is transferred at the base (even) address and
the **low word** at `base + 1` (odd) address. So a 32-bit read of a register at
`0x0C003000` returns `[31:16]` from `+0` and `[15:0]` from `+2`. The hardware
implements this by multiplexing the 16-bit half out of a 32-bit internal register
according to address bit 1.

### 3.2 The 2-byte / 4-byte handshake

`pi_mod` (used for CP, PI, VI and DSP) manages the register handshake with a small
state machine:

- a **2-byte** request: `REQ2 → ACK2`;
- a **4-byte** request: `REQ40 (high word, even address) → ACK40 → REQ41 (low word,
  odd address) → ACK41`.

So a 4-byte access is always two back-to-back 16-bit accesses to consecutive
halfwords, high word first. `pi_mem` does the same for the MEM register space.

### 3.3 Register space

PI hosts the register space that the CPU sees at **`0x0C003000`** (the uncached
physical alias is `0xCC003000`). The full Flipper register-space map is given in
§6.2; the PI registers themselves are listed in §8 and §9.

## 4. Interrupts

### 4.1 Interrupt sources

The CPU has a **single** interrupt line (`INT`, active-low) driven by PI. PI
aggregates the interrupt requests from every other Flipper block into one 32-bit
**cause** register (`INTSR`). Each source has a corresponding bit in the
**mask/re-enable** register (`INTMSK`). A source is reported only if the
corresponding mask bit is set; the aggregated, masked result drives `INT` (when
any masked source is pending, `INT` is asserted low).

| Bit | `INTSR` | `INTMSK` | Source |
|---|---|---|---|
| 0 | `PIINT` | `PIMSK` | PI error (bad address/transfer) |
| 1 | `RSWINT` | `RSWMSK` | Reset switch (warm reset request) |
| 2 | `DIINT` | `DIMSK` | Disc interface |
| 3 | `SIINT` | `SIMSK` | Serial interface |
| 4 | `EXINT` | `EXMSK` | Expansion interface |
| 5 | `AIINT` | `AIMSK` | Audio interface |
| 6 | `DSPINT` | `DSPMSK` | Audio DSP |
| 7 | `MEMINT` | `MEMMSK` | Memory interface (MARR/full access errors) |
| 8 | `VIINT` | `VIMSK` | Video interface (retrace) |
| 9 | `PEINT0` | `PEMSK0` | Pixel engine 0 |
| 10 | `PEINT1` | `PEMSK1` | Pixel engine 1 |
| 11 | `CPINT` | `CPMSK` | Command processor |
| 12 | `DBGINT` | `DBGMSK` | Debug (JTAG / development) |
| 13 | `SDINT` | `SDMSK` | Security / decode |
| 15:14 | — | — | Reserved |
| 16 | `RSTVAL` | — | Reset value / status (read-only) |
| 31:17 | — | — | Reserved |

`RSTVAL` (bit 16) is read-only and reports the state of the asynchronous
`rstswb` line after synchronising — it tells software whether the reset switch is
currently pressed or released.

### 4.2 Masking and the `INT` line

`INTMSK` is write-only for the mask bits (it also duplicates the `INTSR` bit
position for each source). Both `INTSR` and `INTMSK` **reset to zero**: no
interrupt is pending and every interrupt is masked, so a freshly-powered Flipper
raises no `INT`.

`INT` is the OR of `(INTMSK & pending source)`. Because there is a single line,
the OS keeps a master handler that reads `INTSR`, and dispatches. `INT` is
cleared when the pending source is cleared (the source block clears its own
status) and, for the PI-specific sources, when software writes back to `INTSR`
(§4.3, §4.4).

### 4.3 Clearing `INTSR`

`INTSR` is **read/write**. Writing a `1` to a bit in `INTSR` **clears** that
interrupt (the standard PowerPC "write 1 to clear" convention). Software writes
back the bit positions it has serviced. Writes to the read-only `RSTVAL` bit are
ignored.

### 4.4 The PI error interrupt

`PIINT` is set by the PI itself whenever a transaction is illegal (see §5). It is
cleared by writing bit 0 of `INTSR`. Clearing `PIINT` also **unblocks** the
error-address/status latches so a subsequent error can be captured (§5.2).

## 5. PI errors

A transaction that Flipper cannot honour is not silently dropped — PI latches
the offending **address** and an **error code** and raises `PIINT`. These are the
only "exceptions" the PI generates; there is no bus-wide abort signal, and the
transaction simply does not complete and does not touch memory.

### 5.1 Error codes (`PIESR`)

`PIESR[2:0]` holds the error code. `PIEAR[31:0]` holds the offending address.
`PIEAR` is captured at the same time as `PIESR` and only updates while the error
latch is unblocked.

| Code | Meaning |
|---|---|
| `001` | **Misaligned transfer** — the address is not aligned to the transfer size (e.g. a 4-byte read from a mod-4 address, a burst write not 32-byte aligned). |
| `010` | **Invalid transfer type** — the transfer type on an address-only transaction is not one PI recognises (an unknown control code). |
| `011` | **Unsupported size for the destination** — e.g. a single-beat 8-byte (double) access to the register space, or a 2-byte access to main memory, or a burst to IO. |
| `100` | **No destination** — the address does not map to any region (memory, EFB, IO, FIFO or ROM). |
| `101` | **Write to ROM** — the boot-ROM region is read-only. |
| `110` | **Read from the CP FIFO** — the graphics command FIFO is write-only. |

### 5.2 Error-address latch (`PIEAR` / earblock)

`set_piint` latches `PIEAR` and `PIESR` *once*. After the first error the latch is
**blocked** (`earblock`) until software clears `PIINT`. So consecutive errors only
record the first one; software must service and clear `PIINT` before a new error
can be captured. This prevents a repeating error from overwriting the address
of the original fault.

### 5.3 Which accesses raise errors

Summarising the decode and error rules:

| Access | Behaviour |
|---|---|
| Main-memory read, 8/4 bytes, aligned | OK |
| Main-memory read, 2 or 1 byte | **Error `011`** (unsupported size) |
| Main-memory read, misaligned | **Error `001`** |
| Main-memory write, 8/4 bytes, aligned | OK (written as an 8-byte block) |
| Main-memory burst read, 8-byte aligned | OK |
| Main-memory burst write, 32-byte aligned | OK |
| EFB read/write | Same as main memory; 4/8 bytes or a 32-byte burst |
| Registers (IO space) 2/4 bytes, aligned | OK (2 bytes = one halfword; 4 bytes = the two halves) |
| Registers (IO space) burst | **Error `011`** |
| Registers (IO space) 8 bytes | **Error `011`** |
| Registers (IO space) misaligned | **Error `001`** |
| ROM (boot) read, 8 bytes, aligned | OK |
| ROM (boot) burst read, 32-byte aligned | OK |
| ROM (boot) write | **Error `101`** |
| CP FIFO burst write, 32-byte aligned | OK (advances the FIFO) |
| CP FIFO read | **Error `110`** |
| Any address outside all regions | **Error `100`** |

## 6. Physical memory map

The CPU's physical map, as decoded by PI:

| Address | Size | Resource | Notes |
|---|---|---|---|
| `0x0000_0000` | 24 MB | Main RAM | Two 12 MB 1T-SRAM banks at `0x0000_0000` and `0x0180_0000`; decoded as a 64 MB window |
| `0x0800_0000` | 32 MB (2 MB used) | Embedded framebuffer (EFB) | On-chip 1T-SRAM |
| `0x0C00_0000` | — | Flipper register space | See §6.2 |
| `0x0C00_8000` | 4 KB | GFX command FIFO | Write-only (see §6.3) |
| `0xFFF0_0000` | 1 MB | Boot ROM | Read-only; Gekko reset vector `0xFFF0_0100` |

The 64 MB main-memory window covers both RAM banks plus the reserved portion.
Addresses between the end of RAM and the top of this window are still decoded as
memory (they are physically idle); an access there does not raise a PI error but
reads back idle bus data.

### 6.1 The `0x0C00_xxxx` register space

The low 12 bits of a register-space address select the block; the next 3 bits
(`[14:12]`) select the IO-space module, and bits `[8:1]` select the register
halfword within that block:

| Base | IOsp | Block |
|---|---|---|
| `0x0C00_0000` | `000` | Command Processor (CP) |
| `0x0C00_1000` | `001` | Pixel Engine (PE) |
| `0x0C00_2000` | `010` | Video Interface (VI) |
| `0x0C00_3000` | `011` | **Processor Interface (PI)** |
| `0x0C00_4000` | `100` | Memory Interface (MEM) |
| `0x0C00_5000` | `101` | DSP + Audio + ARAM DMA |
| `0x0C00_6000` | `110` | IO (DI / SI / EXI / AIS) |
| `0x0C00_8000` | — | GFX command FIFO |

### 6.2 CP / GFX FIFO

The **GFX command FIFO** (the "PI FIFO") is the write path the CPU uses to feed
the graphics command processor. The CPU writes **32-byte bursts** to
`0x0C00_8000`; each burst advances the write pointer by 32 bytes. Four registers
control it:

| Register | Offset | Meaning |
|---|---|---|
| `CPBAS` | `0x0C` | FIFO base — the value the write pointer is reset to on wraparound |
| `CPTOP` | `0x10` | FIFO top — when the write pointer reaches it, it wraps to base |
| `CPWRT` | `0x14` | Write pointer (current address); bit 26 = `WRAP` flag |
| `CPABT` | `0x18` | Abort — writing 1 forces a hardware abort/GFX reset |

The write pointer (`CPWRT`) advances by 32 per written burst, wraps to `CPBAS`
when it equals `CPTOP`, and sets the `WRAP` flag bit. `CPABT` forces the GFX into
a reset state (`gfxrstb`) unconditionally when set.

## 7. Resets

### 7.1 Cold (power-on) reset

On power-up `PorB` (power-on reset) is asserted. PI synchronises this through a
chain of reset synchronisers and drives **`Flipper_RstB`** and **`CPU_RstB`**
(active low). When they deassert, the Gekko starts fetching from
**`0xFFF0_0100`** in the boot ROM. PI also asserts **`TRST`** with `HRESET` so
the CPU's JTAG logic is reset together with the core.

### 7.2 Warm (software) reset via `CONFIG`

The `CONFIG` register at `0x0C00_3024` is the reset control. Bits:

| Bit | Field | Meaning |
|---|---|---|
| 0 | `SYSRSTB` | Write `0` to assert a system reset; write `1` to deassert (it auto-releases after `DURAR` cycles) |
| 1 | `MEMRSTB` | Write `0` to reset the memory interface; write `1` to release |
| 2 | `DIRSTB` | Write `0` to reset the disc interface; write `1` to release |
| 31:3 | `PICFG` | Reserved / configuration |

`SYSRSTB` behaves as a **pulse**: writing `0` asserts reset, starts an internal
down-counter loaded from **`DURAR`**, and when the counter reaches zero the reset
is released automatically. Asserting `SYSRSTB` also asserts `MEMRSTB` and
`DIRSTB`. Writing `1` to `CONFIG[0]` releases the reset early.

`DURAR` (`0x0C00_3028`) sets the reset duration in CPU cycles; its reset value is
`0x1FF` (511 cycles). Setting a large value keeps the reset asserted for longer.

`MEMRSTB` and `DIRSTB` are **read/write** (their state is readable via `CONFIG`);
`SYSRSTB` is **write-only** and reads back as `0`.

### 7.3 GFX reset

`gfxrstb` is derived by PI as `~(PIABT)` AND `Flipper_RstB`. It is normally
asserted high; it is pulled low (GFX held in reset) when either the whole Flipper
is reset or the `PIABT` (CP abort) bit is set.

### 7.4 The reset switch (`RSWINT`)

The console's reset button is wired to `rstswb`. When it is pressed (a falling
edge on the synchronised line), PI sets the **`RSWINT`** interrupt rather than
resetting immediately. The OS's interrupt handler decides what to do — typically
a clean shutdown or a warm reboot — so the reset is **software-controlled**, not
a bare hardware reset. `rstswb`'s current level is also readable as `RSTVAL`
(`INTSR[16]`).

## 8. Register map

The PI registers occupy `0x0C00_3000`–`0x0C00_3036` (uncached alias
`0xCC00_3000`). Each register is **32-bit** internally, addressed as two 16-bit
halves (high at the base, low at base+1). The table gives the byte offset from
`0x0C00_3000`.

| Addr | Name | R/W | Description |
|---|---|---|---|
| `0x00` | `INTSR` | R/W | Interrupt cause (write 1 to clear) |
| `0x04` | `INTMSK` | R/W | Interrupt mask / enable |
| `0x0C` | `CPBAS` | R/W | GFX FIFO base |
| `0x10` | `CPTOP` | R/W | GFX FIFO top |
| `0x14` | `CPWRT` | R/W | GFX FIFO write pointer + wrap |
| `0x18` | `CPABT` | R/W | GFX FIFO abort |
| `0x1C` | `PIESR` | R/W | PI error status / code |
| `0x20` | `PIEAR` | RO | PI error address |
| `0x24` | `CONFIG` | R/W | Reset control / config |
| `0x28` | `DURAR` | R/W | Software-reset duration (cycles) |
| `0x2C` | `CHIPID` | RO | Flipper chip revision |
| `0x30` | `STRGTH` | R/W | Interface pad drive strength |
| `0x34` | `CPUDBB` | R/W | CPU dead-cycle / bus-cycle control |

## 9. Register fields

### 9.1 `INTSR` (0x00, 32-bit)

The interrupt cause register. Bits are defined in §4.1. Reset `0x0000_0000`.
Writing a `1` to a bit clears that interrupt; the `RSTVAL` bit (16) is read-only.

### 9.2 `INTMSK` (0x04, 32-bit)

The interrupt mask. Reset `0x0000_0000` (all masked). The bit positions match
`INTSR` (§4.1) but only the low 14 bits are implemented; the upper bits read as `0`.

### 9.3 `CPBAS` (0x0C, 32-bit)

GFX FIFO base. Bits:

| Bits | Field | Description | Reset |
|---|---|---|---|
| 25:5 | `BASE` | FIFO base address (the value `CPWRT` resets to on wrap) | `0` |
| 4:0 | — | Reserved (zero) | `0` |

### 9.4 `CPTOP` (0x10, 32-bit)

GFX FIFO top. Bits:

| Bits | Field | Description | Reset |
|---|---|---|---|
| 25:5 | `TOP` | FIFO top address (`CPWRT` wraps to `BASE` when it reaches `TOP`) | `0` |
| 4:0 | — | Reserved (zero) | `0` |

### 9.5 `CPWRT` (0x14, 32-bit)

GFX FIFO write pointer. Bits:

| Bits | Field | Description | Reset |
|---|---|---|---|
| 25:5 | `WRPTR` | Current write address; advances by 32 per burst, wraps to `BASE` at `TOP` | `0` |
| 26 | `WRAP` | Set to `1` when the pointer reaches `TOP` and wraps; cleared when `CPWRT` is written | `0` |
| 4:0 | — | Reserved (zero) | `0` |

### 9.6 `CPABT` (0x18, 32-bit)

GFX FIFO abort. Bit 0 = `CPABT` (write `1` to force a GFX reset/abort). Reset `0`.

### 9.7 `PIESR` (0x1C, 32-bit)

PI error status. Bits:

| Bits | Field | Description | Reset |
|---|---|---|---|
| 2:0 | `PESR` | Error code (see §5.1) | `0` |
| 31:3 | — | Reserved | `0` |

### 9.8 `PIEAR` (0x20, 32-bit, read-only)

The address of the PI error. Captured when `PIINT` is set; held until `PIINT` is
cleared.

### 9.9 `CONFIG` (0x24, 32-bit)

Reset / configuration. Bits:

| Bits | Field | Description | After power-on |
|---|---|---|---|
| 0 | `SYSRSTB` | Software reset (write-only; reads `0`) | — (write-only) |
| 1 | `MEMRSTB` | Memory-interface reset | `1` (released) |
| 2 | `DIRSTB` | Disc-interface reset | `0` (held until software releases) |
| 31:3 | `PICFG` | Reserved | `0` |

`SYSRSTB` is write-only and reads as `0`. `MEMRSTB`/`DIRSTB` are R/W and read
back their state. The memory interface is released automatically shortly after
power-on, while the disc interface is left asserted until the boot code writes
`CONFIG` to release it.

### 9.10 `DURAR` (0x28, 32-bit)

Software-reset duration. `PIRDR[9:0]` = number of CPU cycles the `SYSRSTB` pulse
is held. Reset `0x1FF` (511). The field was widened to 10 bits to allow up to
1024 cycles.

### 9.11 `CHIPID` (0x2C, 32-bit, read-only)

The Flipper chip revision / ID, hard-wired and copied into the register on reset.

### 9.12 `STRGTH` (0x30, 32-bit)

Interface drive strength. Nine interfaces, each a 3-bit field. The reset value is
`2` (`0b010`) for every interface — this is the value the boot firmware programs
early, and it must not be raised carelessly (large values on some interfaces can
damage the pads if held for a long time).

| Bits | Field | Interface |
|---|---|---|
| 2:0 | `AI_STR` | Audio |
| 5:3 | `AIS_STR` | Audio streaming |
| 8:6 | `SI_STR` | Serial |
| 11:9 | `EXI2_STR` | Expansion 2 |
| 14:12 | `EXI1_STR` | Expansion 1 |
| 17:15 | `EXI0_STR` | Expansion 0 |
| 20:18 | `DI_STR` | Disc |
| 23:21 | `VI_STR` | Video |
| 26:24 | `SD_STR` | Security / decode |

### 9.13 `CPUDBB` (0x34, 32-bit)

CPU dead-bus-cycle / bus-timing control. Bit 0 = `DBB`. Reset `0`. It selects
whether the CPU-side transfer acknowledge follows the normal 60x protocol
(`DBB = 0`) or omits the dead-bus (turnaround) cycle (`DBB = 1`). It is generally
left `0` and is only used by low-level Flipper bring-up.

## 10. Emulator notes

1. **Register width.** Model every Flipper register as a 16-bit-scoped register
   accessed as two halves for a 32-bit access. Keep the big-endian high-word-first
   ordering for the split, and reject (raise the PI error) any non-2/4-byte access
   to the register space.
2. **Interrupt line.** Maintain a `INT` output = active-low OR of
   `(INTSR & INTMSK)`. Do not model per-block interrupt sources inside PI; PI only
   collects the OR result of each block.
3. **PI errors.** On an illegal transaction, latch the address in `PIEAR`, the code
   in `PIESR`, set `PIINT`, and block further captures until `PIINT` is cleared.
   Do not drive a bus-error/abort to the CPU; the transaction simply stalls and
   never completes.
4. **Memory path.** Model main-memory reads/writes as 32-byte bursts regardless of
   the declared size (the write buffer bursts them). A sub-line read returns the
   critical double-word first.
5. **Prefetch.** Can be modelled as a no-op for correctness; optionally, serve a
   32-byte line from a prefetch buffer when software issues a prefetch hint and
   then reads the same line.
6. **sync / eieio.** Implement as a barrier: flush pending writes, then wait until
   no transaction, read or prefetch is outstanding and all modules are idle. This
   is what the software relies on before starting a DMA that reads freshly written
   data.
7. **CP FIFO.** Track the write pointer `CPWRT` across each 32-byte burst write to
   `0x0C00_8000`, wrapping at `CPTOP` to `CPBAS` and setting `WRAP`. Writes advance
   the pointer by 32; the command processor consumes the stream from memory.
8. **Resets.** `CONFIG[0]=0` starts a `DURAR`-length system reset that also resets
   MEM and DI. Power-on drives `CPU_RstB`/`Flipper_RstB` low and the CPU boots at
   `0xFFF0_0100`. Model the reset-switch as an `RSWINT` interrupt, not a hardware
   reset.

## 11. References

- `HW/IO/ProcessorInterface.md` — the Gekko–Flipper bus and PI register summary.
- `HW/IO/PI_001.png` — the Gekko–Flipper interface diagram.
- `HW/IO/mi.txt` — MI/PI memory-map and protection notes.
- `HW/Flipper_ASIC_Block_Diagram.png` — the PI block within Flipper.
- The Gekko (PowerPC 750-derivative) 60x bus signals, and the Gekko User's Manual
  for the reset vector and transfer encodings.
- **US Patent 6,609,977** (external interfaces) — the CPU–Flipper interface,
  register and reset model.
- The Flipper chip RTL sources are the authority for the PI sub-block structure,
  the register-level behaviour and the error/interrupt logic; all facts above are
  summarised from them.
