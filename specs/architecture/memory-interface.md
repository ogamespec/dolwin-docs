# GameCube — Memory Interface (MI / MEM)

> **MEM** (the Memory Interface, also called **MI**) is the block inside the
> **Flipper** ASIC that is the single hub through which every other block reaches
> main memory. It arbitrates the competing memory masters, buffers writes so the
> data bus does not have to keep switching between read and write, refreshes the
> external memory, protects main memory with programmable region registers, and
> drives the high-speed interface to the two external **1T-SRAM** dies (the
> "Splash" memory). There is no other path to main memory: the CPU, the graphics
> pipeline, the audio DSP and the DMA controllers all go through the MEM arbiter.

This is the register-level, emulator-focused specification. The facts are drawn
from the repository's hardware documentation, the memory-controller RTL, the
system-library reverse-engineering notes and the memory-controller patent, and
are summarised rather than reproduced verbatim. It describes the **legacy**
(GameCube, HW2) memory interface as sold.

## 1. Overview

Flipper is the hub of the console, and MEM is the hub of Flipper. Physically it
sits between the Gekko bus, the graphics/triangle pipeline, the audio DSP, the
I/O DMA controllers, the video interface and the external 1T-SRAM. Every byte
that moves to or from main memory must be arbitrated by MEM.

MEM is a *request/acknowledge* crossbar rather than a shared synchronous bus. Each
master has its own, point-to-point interface into MEM with an independent data
width and queue, and MEM grants one master at a time. This lets the design tune
the bandwidth of each master independently (the "bandwidth dials") and lets the
GFX masters run at full memory bandwidth while the latency-sensitive CPU stays
responsive.

The external memory is **MoSys 1T-SRAM** ("Splash"): **24 MB** organised as
**two 12 MB dies**, each with a **32-bit** data path, forming a **64-bit** bus.
The external bus runs at about **twice** the Flipper core clock (~**324 MHz**),
giving roughly **2.6 GB/s** of main-memory bandwidth — noticeably more than the
162 MHz Gekko↔Flipper bus, which is what lets the graphics pipeline read textures
and vertex data at full speed without starving the CPU.

### 1.1 Sub-blocks

| Block | Role |
|---|---|
| `mem_pi` | Interface to the Processor Interface (PI) — the Gekko/CPU master; also the register port for all MEM registers |
| `mem_cp` | Interface to the Command Processor (CP) — 128-bit streaming read master |
| `mem_tc` | Interface to the texture cache (TC) — 128-bit streaming read master |
| `mem_pe` | Interface to the Pixel Engine (PE) — 128-bit write master (EFB→XFB copies) |
| `mem_io` | Interface to the I/O interface (IO) — 64-bit read/write DMA master |
| `mem_dsp` | Interface to the audio DSP — 64-bit read/write DMA master |
| `mem_vi` | Interface to the Video Interface (VI) — 64-bit read master (scan-out) |
| `mem_memreg` | Register file — protection, configuration, bandwidth dials, counters, interrupts |
| `mem_wrbuf` | Write-buffer/global-write-queue; asserts flow control and emits flush/ack |
| `mem_arb` | The arbiter — priority + round-robin state machine over all pending requests |
| `mem_extctl` | External-memory controller — generates the 1T-SRAM address/control and route data to/from the MAC |

The read path is a 3-way fan-out of data widths (see §2.2), and the write path is
funnelled into a single global write buffer that drains to memory in bursts:

```
                     ┌───────────────── MEM (arbiter) ─────────────────┐
  CPU (PI) ──64b──▶ │  mem_pi      ─┐                                  │
  CP ──128b───────▶ │  mem_cp      ─┤                                  │
  TC ──128b───────▶ │  mem_tc      ─┤        ┌── mem_arb (priority/RR) ─┐
  VI ──64b────────▶ │  mem_vi      ─┼──────▶ │  arbitrates pending reqs │──▶ mem_extctl ──▶ 1T-SRAM
  IO ──64b──rw───▶ │  mem_io      ─┤        └──────────────┬───────────┘        (via MAC)
  DSP ──64b──rw──▶ │  mem_dsp     ─┘                       │
  PE ──128b──wr─▶  │  mem_pe ───▶ mem_wrbuf (WQ0) ──────────┘
                   └───────────────────────────────────────────┘
```

### 1.2 Transaction model

Every access to main memory is a **cache-line (32-byte) burst**. A 64-bit master
(CPU/IO/DSP/VI) moves one line in **4 back-to-back 8-byte beats**; a 128-bit
master (CP/TC/PE) moves it in **2 back-to-back 16-byte beats**. Addresses are
32-byte aligned (the low 5 bits are implied), and for a non-aligned CPU read the
**critical double-word** is returned first and the rest wrap. Reads are always
delivered **in-order** — MEM does not reorder on its own, so no software
reordering work is needed.

## 2. External interfaces (per master)

Each master has its own interface into MEM; the widths differ and carry the
per-master request/ack/shared-data handshake. Signal names below use the RTL
convention (`<master>_mem…` for the master→MEM direction, `mem_<master>…` for the
MEM→master direction).

### 2.1 PI / MEM register interface

The MEM registers are reached through the PI, at `0xCC004000` (also mapped read-
write at `0x0C004000` in physical space). All register data is **16-bit**, and a
32-bit CPU access is split by the PI into two 16-bit transfers (high word at the
base, low word at base+1); Flipper is big-endian. `pi_mem_reg` is asserted for a
register access; during a register write the low 8 bits of the address hold the
register address and `pi_mem_data[63:48]` carries the 16-bit value.

### 2.2 CPU (PI) memory interface

The CPU is the only master that goes through the PI path; it is the only master
with **both** read and write capability and with a **dedicated 64-bit** data path
(a second 64-bit "system" path is shared by IO/DSP/VI, and the 128-bit "GFX" path
is shared by CP/TC).

| Signal | Width | Description |
|---|---|---|
| `pi_mem_addr[25:1]` | 25 | Cache-line address; double-word aligned for reads, 32-byte aligned for writes |
| `pi_mem_data[63:0]` | 64 | CPU write data (in) / read data (out on `mem_pi_data`) |
| `pi_mem_rd` | 1 | 1 = read, 0 = write |
| `pi_mem_req` | 1 | One-cycle request start |
| `pi_mem_msk[1:0]` | 2 | Per-32-bit-word write mask (0 = write enabled) |
| `pi_mem_flush` | 1 | Flush the write buffer before continuing |
| `pi_mem_fifoWr` | 1 | This write is to the CP-FIFO region; MEM must tell CP when it is committed |
| `mem_pi_ack` | 1 | One-cycle acknowledge; first 8 bytes return on this cycle |
| `mem_pi_data[63:0]` | 64 | 64-bit read data; a line arrives across 4 back-to-back clocks |
| `mem_pi_reqfull` | 1 | Flow-control: stop issuing requests while asserted |

The CPU interface supports multiple outstanding reads (a new read can be issued
every cycle); writes are issued every 4 clocks (4 cycles to transfer a line).
Uncached byte/halfword stores are written as the full aligned 8-byte block (see
§9, the store-width quirk).

### 2.3 GFX read masters (CP, TC)

CP and TC are **read-only**, **128-bit** masters. Each has a **queued** request
path (depth 16) and a `reqFull` signal that asserts when the queue is almost full
(allowing 2 more entries).

| Signal | Width | Description |
|---|---|---|
| `<cp|tc>_mem_req` | 1 | One-cycle read request |
| `<cp|tc>_mem_addr[25:5]` | 21 | Cache-line read address |
| `mem_<cp|tc>_ack` | 1 | One-cycle ack; first 16 bytes in the next cycle |
| `mem_<cp|tc>_data[127:0]` | 128 | 128-bit data; a line in 2 back-to-back clocks |
| `mem_<cp|tc>_reqFull` | 1 | Queue almost full — 2 more requests may still be accepted |

`mem_cp_fifoWr` informs CP that a CPU write to the command FIFO has been committed
to memory, so the graphics front-end can advance its write pointer.

### 2.4 Pixel Engine (PE) write master

PE is **write-only** and **128-bit**. It writes the copy/scaled EFB image to main
memory (and can also write it in texture format).

| Signal | Width | Description |
|---|---|---|
| `pe_mem_addr[25:5]` | 21 | Cache-line write address |
| `pe_mem_data[127:0]` | 128 | 128-bit write data; 2 back-to-back beats |
| `pe_mem_req` | 1 | One-cycle write request |
| `pe_mem_flushwrbuf` | 1 | Flush the write buffer at the end of a write burst |
| `mem_pe_reqfull` | 1 | Write queue almost full — no more requests |
| `mem_pe_wrbufempty` | 1 | Write buffer empty — safe to start a read |

### 2.5 System masters (IO, DSP, VI)

These three are **64-bit** masters and all sit on the shared "system" data path.
They are low-bandwidth, single-outstanding masters.

- **IO** (`mem_io`) — reads and writes (peripheral DMA: disc, EXI, memory cards,
  network). Single outstanding transfer.
- **DSP** (`mem_dsp`) — reads and writes (audio, ARAM DMA, code loading). Single
  outstanding transfer. Carries a 2-bit write mask.
- **VI** (`mem_vi`) — read-only (framebuffer scan-out). Single outstanding read
  request; a new request is only issued after the previous one is acknowledged,
  and the picture is always fetched in-order.

Each carries an address `[25:5]`, a `req`, an `rd`/read-only indication, a
`mem_<x>Ack`, a `mem_<x>Data[63:0]` read bus and a `mem_<x>FlushWrAck`, plus the
per-master `FlushWrBuf` input at the end of a write burst.

### 2.6 External memory — the "Splash" 1T-SRAM interface

This is the memory-side interface that the reference patent figure omits. MEM
drives the two external **1T-SRAM** dies through the **Memory Access Controller
(MAC)**. The MAC converts the internal wide, moderate-rate datapath to the narrow,
double-data-rate external interface.

The pads at the package fall into groups:

| Group | Bits | Description |
|---|---|---|
| `MEMD0–31`, `MEMD32–63` | 64 | Data bus — 32 bits per die, forming the 64-bit main-memory bus |
| `MEMA1–21` | 21 | Address bus (burst-word addressing); `MEMA0` is implied |
| `MEMRW` | 1 | Direction — 1 = read, 0 = write |
| `MEMADSxB` | 2 | Address strobe / chip select, active low (one per die) |
| `MEMREFxSH` | 1 | Refresh request |
| `MEMCK0–3`, `MEMCLK0/1`, `MEMCLKB0/1` | — | Quad (DDR) clock and inverted-clock outputs for the 1T-SRAM |
| `MEMCKQ0–3` | 4 | Read data strobes returned by the 1T-SRAM |
| `MEMMSK0/1` | 2 | Write data masks |
| `MEMRSTB0/1` | 2 | Reset to each die |
| drive-strength / terminator | — | Programmable pad drive strength and active-terminator enable |

The **MAC** is the physical interface: each MAC block converts a **32-bit**,
200 MHz core-side port into a **16-bit**, 400 MHz **DDR** lane to the die. Four
MAC blocks (one per 16-bit lane) make up the 64-bit memory datapath; the
1T-SRAM's quad-clock scheme (`memclk`/`memclkb`, `memckq0/1`) provides the
double-data-rate timing. The result is a 64-bit bus at roughly twice the Flipper
core clock.

In a software/emulator model the MAC and the pad timing collapse to a single
question: how many bytes can MEM push through the external bus per core cycle.
Because the bus is faster than the core, the arbiter can keep the memory busy
almost every cycle even when a 128-bit master is being served.

## 3. Arbitration

MEM serves **7 masters** plus its own refresh logic. Four can write (CPU, PE, DSP,
IO), three are read-only (CP, TC, VI), and the DSP/IO/CPU can both read and write.
There is also a write-buffer flush agent.

### 3.1 Masters and capability

| Master | Read | Write | Data path |
|---|---|---|---|
| PI (CPU) | yes | yes | 64-bit (own) |
| CP | yes | no | 128-bit (GFX shared) |
| TC | yes | no | 128-bit (GFX shared) |
| PE | no | yes | 128-bit |
| VI | yes | no | 64-bit (system shared) |
| IO | yes | yes | 64-bit (system shared) |
| DSP | yes | yes | 64-bit (system shared) |

### 3.2 Priority order

The overall service order is (highest → lowest):

1. **CPU (PI) read** — the CPU must be serviced with low and predictable latency
   for instruction/data fetch. It has the highest *read* priority, subject only
   to the back-to-back read-width restriction (§3.4).
2. **Write-buffer flush** — when the global write queue fills to a level (≈75–80%)
   or a CPU read address hits an entry in the write buffer, the write buffer is
   drained in one go. Draining the whole buffer amortises the read↔write bus
   turnaround, so it is treated as a single high-priority operation.
3. **Refresh** — normally low priority, but it is **bumped to just below CPU
   read** when the number of pending refresh requests reaches the threshold set
   in the refresh-threshold register.
4. **GFX and system masters** — CP/TC collectively ("GFX") and DSP/IO/VI
   collectively ("system") then arbitrate against each other in a **round-robin**
   scheme driven by a programmable priority register that rotates after every
   grant.

The write buffer (the union of the local write queues and the global write queue)
is always drained **in its entirety** once a flush begins; it is never interleaved
read/write at the sub-line level.

### 3.3 Bandwidth dials

Five masters have a per-master **bandwidth dial** register: CPU read, CPU write,
CP read, TC read and PE write. Each dial is an **8-bit fraction** (format 1.8) of
a memory cycle. Every core cycle the dial value is added to an accumulator that
starts at 0; the master is only admitted to arbitration when the accumulator's
bit 8 (i.e. the fraction reached 1.00) is set.

- A dial of **1.00** (= `0x100`) means the master is always admitted — the default.
- A dial of **0.50** admits the master on roughly every second cycle, throttling
  it when the bus is contended.
- The arbiter **never lets the memory go idle** because of a dial: if the bus has
  nothing else to do, a dialed-down master still gets served. This lets a game
  tune bandwidth toward the graphics pipeline without ever stalling the memory.

The write dials (CPU write, PE write) throttle how quickly writes enter the global
write queue, which in turn spreads the read↔write turnaround over more time.

### 3.4 Structural restrictions

Because of the 3-way data-path fan-out, some masters cannot be back-to-back:

- Two CPU reads cannot occur back to back (the CPU path is half memory bandwidth).
- Two system (DSP/IO/VI) reads cannot occur back to back.
- Only one master is served at a time; after a grant the priority register
  rotates, so a master that just ran is deprioritised for the next cycle.

These restrictions matter for an emulator that wants to reproduce exact memory
timing: they bound the peak per-master bandwidth and explain why the CPU never
saturates the bus.

## 4. Queues, buffering and coherency

Writes are the expensive part of the memory bus because switching between reads
and writes costs idle turn-around cycles. MEM therefore buffers writes and drains
them in bursts, and it implements a coherency protocol so that a master never
reads stale data. The first subsection below describes the queueing architecture
every request passes through; the rest covers how the write buffers drain and how
coherency is maintained.

### 4.1 Queue architecture

Every request is staged in a dedicated queue inside its own interface before it
reaches the arbiter. **Read queues** hold the line *address*; **write queues**
hold the address, the 128-bit data and a byte mask. Each queue lives inside the
matching master interface (`mem_pi`, `mem_cp`, …); only the **global write
buffer** is shared (`mem_wrbuf`). They are sized to the master's access pattern.

| Queue | Owner | Type | Depth | Entry contents |
|---|---|---|---|---|
| RQ1 | CP | read | 16 | 21-bit line address |
| RQ2 | TC | read | 16 | 21-bit line address |
| RQ3 | VI | read | 1 | 21-bit line address |
| RQ4 | DSP | read | 1 | 21-bit line address |
| RQ5 | IO | read | 1 | 21-bit line address |
| RQ6 | PI | read | 6 | 23-bit line address (2 extra bits return the critical double-word first) |
| WQ1 | PE | write | 8 | 21-bit address + 128-bit data |
| WQ2 | DSP | write | 4 | 21-bit address + 128-bit data + 4-bit mask |
| WQ3 | IO | write | 4 | 21-bit address + 128-bit data |
| WQ4 | PI | write | 8 | 21-bit address + 128-bit data + 4-bit mask |
| WQ0 | global | write | 16 | 21-bit address + 128-bit data + 4-bit mask + owner + CP-FIFO flag |

- **Read queues.** CP and TC are 16-deep so they can keep a stream of line reads
  in flight and absorb memory latency; VI, DSP and IO are single-entry so a
  software-visible master only ever has one outstanding read; PI is 6-deep so a
  burst of CPU loads can be queued without stalling the CPU.
- **Write queues (WQ1–WQ4)** are the *local* write buffers inside each write
  master's interface. They hold whole lines so the master can burst the data and
  move on, and they feed the shared global buffer.
- **Global write buffer (WQ0)** collects writes from all four write masters so
  they can be drained to memory as one contiguous burst. Each entry carries an
  **owner** tag (which master wrote it) and a **CP-FIFO** flag; the owner tag is
  what lets the controller flush only the relevant entries when a read address
  matches a buffered write.

**Flow control.** A master is told to pause when its queue is nearly full. CP and
TC raise `mem_<cp|tc>_reqFull` when their 16-deep read queue holds more than 10
entries; PI, IO, DSP and VI signal `mem_<x>_reqfull` when their read queue or
local write buffer is close to full. This throttling keeps a busy master from
over-running the arbiter.

### 4.2 Local write queues and global write queue

Write flow control has three levels:

1. **Local queue full** — MEM asserts `mem_<x>reqfull` to stop the master sending.
2. **Global queue filling** — when WQ0 reaches ~75–80%, MEM switches the bus to
   write and drains WQ0 in a burst.
3. **Coherency flush** — a flush command forces the buffer empty before a related
   read (see §4.4).

By concentrating writes into one global queue that drains as a burst, the number
of read↔write direction changes on the bus is minimised, which is what makes the
buffer worthwhile. Because the buffer is fed by several masters, the write-side
arbiter inside `mem_wrbuf` decides which master's write goes into which free
entry; the write dials (§3.3) set how quickly each master's writes are accepted.

### 4.3 Flush / acknowledge handshake

Four masters can write (CPU, PE, DSP, IO). Each has a two-wire flush protocol:
`<x>_memFlushWrBuf` → `mem_<x>FlushWrAck`. A master asserts flush at the end of a
DMA write **before** interrupting the CPU; MEM completes the drain and asserts
ack, guaranteeing the data is in main memory (not in a buffer) before anyone else
reads it.

### 4.4 Coherency cases

- **Same unit read-after-write.** DSP and IO have no hardware read/write coherency
  — a master that writes then wants to read its own data back must explicitly
  flush and wait for the ack. The CPU (PI) is the exception: every PI read address
  is checked against the write buffer (local + global); on a match the buffer is
  flushed first, then the read proceeds.
- **CPU stall / sync.** When the CPU both writes and reads the same address, the
  read may be sent to memory before the coherency check completes (the address is
  raced to the memory to save a latency cycle). If a match is found one cycle
  later, the read result is aborted, the write buffer is flushed, and the write
  data is merged/returned to the CPU at the end of the flush. The CPU is expected
  not to issue a write to the same address until the read data comes back, and to
  use a `sync` instruction before starting a DMA after building a buffer.
- **Between different units.** A writer must flush before signalling another unit
  to read the data (e.g. PE copies to memory then flushes before VI reads the
  image). This is the two-wire flush protocol above.
- **CPU → CP command FIFO.** The CP-FIFO region is special: PI tags the write with
  `pi_mem_fifoWr`, and MEM delays telling CP until the data is actually committed
  to main memory. This keeps the graphics front-end from reading a command before
  its bytes are visible.

## 5. Memory protection and interrupts

MEM can protect up to **4 regions** of main memory, with **10-bit page granularity**
(1024-byte pages). This is the mechanism the OS uses for `OSProtectRange` and for
trapping invalid memory access.

### 5.1 Region registers (`MEM_MARR0..3`)

Each region is defined by a **start** and **end** register, each holding the
**page number** (physical address `>> 10`). A region covers
`MARRn_START ≤ address < MARRn_END`. The CPU (or software) writes the page-
aligned range; the hardware compares the *physical* address of every memory
transaction against all four regions.

### 5.2 Access mode (`MEM_MARR_CONTROL`)

Each region has a 2-bit access mode encoded as a **read-enable** and a
**write-enable** bit:

| Read-enable | Write-enable | Meaning |
|---|---|---|
| 1 | 1 | Full access (no protection) |
| 1 | 0 | Read-only (writes denied) |
| 0 | 1 | Write-only (reads denied) |
| 0 | 0 | Access denied |

The 8 control bits are region 0 read/write, region 1 read/write, region 2
read/write, region 3 read/write. Reset value is **all enabled** (`0xff`), so by
default nothing is protected.

### 5.3 Interrupts

A violation sets a status bit and latches the offending address:

- `MEM_INT_ENBL` — interrupt enables: bit 0..3 = region 0..3 enable, bit 4 =
  address-error enable. Default all **disabled** (`0x00`).
- `MEM_INT_STAT` — status bits, one per region plus address-error. Reset `0x00`.
- `MEM_INT_CLR` — write any value to clear all interrupt status.
- `MEM_INT_ADDRL` / `MEM_INT_ADDRH` — the low 16 / high 16 bits of the offending
  address.

An **address error** interrupt is generated when the request address is outside
the configured memory range but still within the 64 Mbyte address space (i.e. it
was not already caught by the PI). If the address is *beyond* 64 Mbytes, the PI
raises the address error itself and does not forward the request to MEM.

Note: MEM **does not abort** the offending transaction — it completes it and only
raises the interrupt. The OS handler is what decides how to react.

The MEM interrupt is OR-ed into the PI as the `MEMINT` bit in the PI interrupt
cause/mask registers (`INTSR` bit 0x80 / `INTMSK` bit 0x80).

## 6. Refresh and bus turnaround

The external 1T-SRAM needs periodic refresh. MEM counts the cycles and issues a
refresh request:

- `MEM_REFRESH` — the number of cycles between refresh requests. Reset `0x80`
  (128 cycles). A value of **0 disables refresh generation** (must be paired with
  a non-zero refresh threshold).
- `MEM_REFRESH_THHD` — the threshold on the number of *pending* refresh requests
  above which refresh is promoted to high priority. Reset `0x2`.

Refresh is normally the lowest priority, but once the pending count reaches the
threshold it jumps to just below the CPU read. Two rows are refreshed per
10-ns refresh cycle (one row every 5 ns).

Bus-turnaround idle cycles are inserted between different operation types to give
the pads time to turn around; they are configurable:

- `MEM_RDTORD` — 1 or 2 idle cycles between back-to-back reads.
- `MEM_RDTOWR` — 2 or 3 idle cycles between a read and a write.
- `MEM_WRTORD` — 0 or 1 idle cycle between a write and a read.

An emulator can simply model these as additional latency when the operation type
changes, or ignore them for most purposes.

## 7. Register map

The MEM registers occupy `0xCC004000`–`0xCC00405C` in the peripheral register
space (also mapped at `0x0C004000` physical). Each register is **16-bit**; the
table below shows the byte offset from the base. Offsets are 2× the register
index used in the RTL/convention.

| Addr | Name | R/W | Description |
|---|---|---|---|
| `0x00` | `MEM_MARR0_START` | R/W | Region 0 start page |
| `0x02` | `MEM_MARR0_END` | R/W | Region 0 end page |
| `0x04` | `MEM_MARR1_START` | R/W | Region 1 start page |
| `0x06` | `MEM_MARR1_END` | R/W | Region 1 end page |
| `0x08` | `MEM_MARR2_START` | R/W | Region 2 start page |
| `0x0A` | `MEM_MARR2_END` | R/W | Region 2 end page |
| `0x0C` | `MEM_MARR3_START` | R/W | Region 3 start page |
| `0x0E` | `MEM_MARR3_END` | R/W | Region 3 end page |
| `0x10` | `MEM_MARR_CONTROL` | R/W | Per-region read/write enable |
| `0x12` | `MEM_CP_BW_DIAL` | R/W | CP read bandwidth dial (1.8) |
| `0x14` | `MEM_TC_BW_DIAL` | R/W | TC read bandwidth dial (1.8) |
| `0x16` | `MEM_PE_BW_DIAL` | R/W | PE write bandwidth dial (1.8) |
| `0x18` | `MEM_CPUR_BW_DIAL` | R/W | CPU read bandwidth dial (1.8) |
| `0x1A` | `MEM_CPUW_BW_DIAL` | R/W | CPU write bandwidth dial (1.8) |
| `0x1C` | `MEM_INT_ENBL` | R/W | Interrupt enables |
| `0x1E` | `MEM_INT_STAT` | RO | Interrupt status |
| `0x20` | `MEM_INT_CLR` | W | Clear all interrupt status |
| `0x22` | `MEM_INT_ADDRL` | RO | Offending address, bits 15:0 |
| `0x24` | `MEM_INT_ADDRH` | RO | Offending address, bits 25:16 |
| `0x26` | `MEM_REFRESH` | R/W | Cycles between refresh |
| `0x28` | `MEM_CONFIG` | R/W | Memory geometry config |
| `0x2A` | `MEM_LATENCY` | R/W | Memory latency (3–6 cycles) |
| `0x2C` | `MEM_RDTORD` | R/W | Read→read idle cycles |
| `0x2E` | `MEM_RDTOWR` | R/W | Read→write idle cycles |
| `0x30` | `MEM_WRTORD` | R/W | Write→read idle cycles |
| `0x32` | `MEM_CP_REQCOUNTH` | R/W | CP request counter, high 16 (31:16) |
| `0x34` | `MEM_CP_REQCOUNTL` | R/W | CP request counter, low 16 (15:0) |
| `0x36` | `MEM_TC_REQCOUNTH` | R/W | TC request counter, high |
| `0x38` | `MEM_TC_REQCOUNTL` | R/W | TC request counter, low |
| `0x3A` | `MEM_CPUR_REQCOUNTH` | R/W | CPU read counter, high |
| `0x3C` | `MEM_CPUR_REQCOUNTL` | R/W | CPU read counter, low |
| `0x3E` | `MEM_CPUW_REQCOUNTH` | R/W | CPU write counter, high |
| `0x40` | `MEM_CPUW_REQCOUNTL` | R/W | CPU write counter, low |
| `0x42` | `MEM_DSP_REQCOUNTH` | R/W | DSP counter, high |
| `0x44` | `MEM_DSP_REQCOUNTL` | R/W | DSP counter, low |
| `0x46` | `MEM_IO_REQCOUNTH` | R/W | IO counter, high |
| `0x48` | `MEM_IO_REQCOUNTL` | R/W | IO counter, low |
| `0x4A` | `MEM_VI_REQCOUNTH` | R/W | VI counter, high |
| `0x4C` | `MEM_VI_REQCOUNTL` | R/W | VI counter, low |
| `0x4E` | `MEM_PE_REQCOUNTH` | R/W | PE counter, high |
| `0x50` | `MEM_PE_REQCOUNTL` | R/W | PE counter, low |
| `0x52` | `MEM_RF_REQCOUNTH` | R/W | Refresh counter, high |
| `0x54` | `MEM_RF_REQCOUNTL` | R/W | Refresh counter, low |
| `0x56` | `MEM_FI_REQCOUNTH` | R/W | Forced-idle counter, high |
| `0x58` | `MEM_FI_REQCOUNTL` | R/W | Forced-idle counter, low |
| `0x5A` | `MEM_DRV_STRENGTH` | R/W | Pad drive strength |
| `0x5C` | `MEM_REFRESH_THHD` | R/W | Refresh threshold |

## 8. Register fields

### 8.1 `MEM_MARRn_START` / `MEM_MARRn_END` (0x00/0x02 … 0x0C/0x0E, 16-bit)

The start/end page number of region n. Bits 15:0 hold `address[25:10]`. Reset
value undefined (regions are unprogrammed at power-on).

| Bits | Field | Description |
|---|---|---|
| 15:0 | `PAGE` | Page number (`physical_address >> 10`) |

### 8.2 `MEM_MARR_CONTROL` (0x10, 16-bit)

Reset value `0xff` (all regions fully accessible). Bits 7:0 are meaningful; the
upper bits are unused.

| Bits | Field | Description |
|---|---|---|
| 0 | `MARR0_RDEN` | 1 = reads within region 0 allowed |
| 1 | `MARR0_WREN` | 1 = writes within region 0 allowed |
| 2 | `MARR1_RDEN` | Region 1 read enable |
| 3 | `MARR1_WREN` | Region 1 write enable |
| 4 | `MARR2_RDEN` | Region 2 read enable |
| 5 | `MARR2_WREN` | Region 2 write enable |
| 6 | `MARR3_RDEN` | Region 3 read enable |
| 7 | `MARR3_WREN` | Region 3 write enable |

### 8.3 `MEM_*_BW_DIAL` (0x12, 0x14, 0x16, 0x18, 0x1A — 16-bit)

8-bit fraction (format 1.8), low 8 bits used. The value is added to an
accumulator every cycle; the master is admitted when that accumulator's bit 8 is
set. **1.00** (`0x100`) = always enabled (default); smaller values throttle.

### 8.4 `MEM_INT_ENBL` (0x1C, 16-bit)

Reset `0x00` (all disabled).

| Bits | Field | Description |
|---|---|---|
| 0 | `MARR0_IE` | Region 0 interrupt enable |
| 1 | `MARR1_IE` | Region 1 interrupt enable |
| 2 | `MARR2_IE` | Region 2 interrupt enable |
| 3 | `MARR3_IE` | Region 3 interrupt enable |
| 4 | `ADERR_IE` | Address-error interrupt enable |

### 8.5 `MEM_INT_STAT` (0x1E, 16-bit, read-only)

Reset `0x00`. Mirrors the enable layout. A bit is set when a violation or an
address error has occurred.

### 8.6 `MEM_INT_CLR` (0x20, 16-bit, write-only)

Writing any value clears the interrupt status register.

### 8.7 `MEM_INT_ADDRL` / `MEM_INT_ADDRH` (0x22 / 0x24, 16-bit, read-only)

`ADDRL` = bits 15:0 and `ADDRH` = bits 25:16 of the address that triggered the
last protection or address-error interrupt.

### 8.8 `MEM_REFRESH` (0x26, 16-bit)

Cycles between refresh requests. Reset `0x80` (128). **0 disables refresh**
(use with a non-zero `MEM_REFRESH_THHD`).

### 8.9 `MEM_CONFIG` (0x28, 16-bit)

Selects the memory geometry the external controller should assume. The bits map to
a device/width combination:

| Value | Config |
|---|---|
| `0b00` | 16 Mbit (single device) |
| `0b01` | 2×16 Mbit |
| `0b10` | 24 Mbit |
| `0b11` | 2×24 Mbit |

Retail HW2 uses **`0b10`** for the 24 MB configuration; the OS writes it during
initialisation (and the memory-controller size register reports "2" for a 24 MB
system).

### 8.10 `MEM_LATENCY` (0x2A, 16-bit)

The programmable memory latency (3–6 cycles) used by the external controller to
align the read-data strobes.

### 8.11 `MEM_RDTORD` / `MEM_RDTOWR` / `MEM_WRTORD` (0x2C / 0x2E / 0x30, 16-bit)

Turn-around idle taps. `RDTORD` 0=1 cycle,1=2; `RDTOWR` 0=2,1=3; `WRTORD` 0=0,1=1.
Reset `0`.

### 8.12 `MEM_*_REQCOUNTH` / `MEM_*_REQCOUNTL` (0x32..0x54, 16-bit)

32-bit performance counters, split into high (bits 31:16) and low (bits 15:0)
halves. One per master (CP, TC, CPU read, CPU write, DSP, IO, VI, PE), plus
refresh (`RF`) and forced-idle (`FI`). Write 0 to a counter to clear it. The `FI`
counter is 33 bits and increments every idle cycle; the `RF` counter counts
refresh cycles. Counters saturate at their maximum.

### 8.13 `MEM_DRV_STRENGTH` (0x5A, 16-bit)

Pad drive-strength control for the memory data/clock pads.

### 8.14 `MEM_REFRESH_THHD` (0x5C, 16-bit)

Refresh threshold, bits 2:0. Reset `0x2`. When the number of pending refresh
requests reaches this value, refresh is promoted to high priority. To generate
**no** refresh cycles at all, set this non-zero together with `MEM_REFRESH = 0`.

## 9. Emulator notes

Modelling MEM correctly is the difference between a GameCube that "mostly works"
and one that behaves with the right timing and protection semantics.

1. **Flat memory array.** The 24 MB main RAM is a plain byte array. Ignore the
   two die, the quad-clock and the MAC — model them as a single 64-bit wide,
   faster-than-core memory. The effective consequence is that the memory can be
   kept almost continuously busy by the arbiter.
2. **Model the arbiter, not the pads.** Keep a per-master request bitmask and a
   rotating priority. Serve the CPU (PI) read first, then the write-buffer flush,
   then refresh (promoted when its threshold is exceeded), then round-robin
   among the pending GFX (CP/TC) and system (DSP/IO/VI) masters. The bandwidth
   dials act as a pre-arbitration throttle.
3. **Cache-line transactions only.** Every memory access is a 32-byte burst. A
   64-bit master takes 4 beats, a 128-bit master 2 beats. Deliver reads
   in-order, and for a non-aligned CPU read return the critical double-word first.
4. **Model the queues.** Use the per-master queue depths of §4.1: CP/TC are 16-deep
   (raise `reqFull` when more than 10 entries are pending), PI reads are 6-deep,
   VI/DSP/IO are single-outstanding, and the write queues 8/4/4/8 feed a 16-entry
   global buffer. The depths bound how many requests each master can keep in
   flight and when flow control engages, so emulating them reproduces real bus
   contention.
5. **Write buffering and flush.** Implement the local write queues feeding the
   global write queue, and flush it as a burst when it reaches ~75–80% full or
   when a CPU read address matches a buffered write. Provide the flush/ack pair
   per write master and the implicit read-before-write flush for CPU reads. If
   you skip coherency, stale reads of just-written DMA data will appear.
6. **Store-width quirk.** The main-memory bus only carries 64-bit transactions.
   An uncached **byte** or **halfword** store writes the whole aligned 8-byte
   block, with the byte/halfword replicated into its lane. This is harmless for
   cached (cache-line) writes. Keep it to make uncached `memset`-style code and
   fast-clear routines behave as on real hardware.
7. **Memory protection.** Implement the four 10-bit-page regions and the
   read/write-enable control bits. Check the physical address of every
   transaction against all four regions; on a violation, set the status bit,
   latch the address and raise the MEM interrupt (but still complete the
   transaction). Also raise the address-error interrupt for an in-range-but-
   unconfigured address.
8. **Refresh.** If you want exact throughput, count cycles and insert refresh
   bursts using `MEM_REFRESH`/`MEM_REFRESH_THHD`; if you only care about functional
   correctness, refresh can be modelled as a no-op (the 1T-SRAM is internally
   refreshed).
9. **Interrupt.** The MEM interrupt enters the PI as `MEMINT` (bit 0x80). The
   retail OS installs a single handler that reads `MEM_INT_ADDRL/H`, clears
   `MEM_INT_CLR` and dispatches a protection error.
10. **Performance counters.** These are for tuning; an emulator can expose them
    for diagnostics but does not need them for correctness.

## 10. References

- Internal memory-controller RTL (arbiter, write buffer, external controller,
  per-master interfaces, MAC) — source of the block and datapath structure,
  summarised here.
- Internal register-definition (FDL) files — the register map and field
  semantics used here.
- `HW/IO/mi.txt` — the MI protection registers and the address-error behaviour.
- `HW/IO/Memory.txt` — the store-width quirk, the gather-buffer and the main-
  memory model.
- `RE/OS/osmemory.txt` — the OS memory-protection library (`OSProtectRange`,
  `__OSInitMemoryProtection`, the reset handler).
- `HW/IO/ProcessorInterface.md` — the `MEMINT`/`MEMMSK` PI interrupts and the
  Gekko–Flipper bus.
- `WEB/anandtech.com/1T-SRAM.htm` — the "Splash" 1T-SRAM outside Flipper: 24 MB,
  2× 12 MB, 64-bit bus.
- **US Patent 8,098,255** ("Graphics processing system with enhanced memory
  controller") — the arbitration methodology, the FIG. 9 queue sizing (used in
  §4.1), write buffering and register semantics, summarised here.
- **US Patent 6,609,977** (External interfaces) — Fig. 12E/12F show the Flipper↔
  1T-SRAM ("Splash") pad groups used in §2.6.
