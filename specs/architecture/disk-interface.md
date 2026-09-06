# GameCube — Disc Interface (DI)

> **DI** (Disc Interface) is the block inside the **Flipper** ASIC that connects
> the console to the GameCube disc drive unit (DDU). The drive is an intelligent,
> command-driven device with its own microcontroller and firmware, so everything
> the console "knows" about the disc is sent as a **command packet** over a simple
> 8-bit, strobed, bidirectional bus. The DI is a register bank plus the
> sequencing logic that turns a command packet into bus-level handshakes, moves
> bulk data to/from main memory in 32-byte blocks (DMA), moves small immediate
> values to/from an on-chip buffer for register-access commands, and raises the
> DI interrupt to the PI. The physical media, the drive architecture and the
> DVD-Audio stream are covered separately in [disk-drive.md](disk-drive.md) and
> in the external-interface hardware documentation.

This is the register-level, emulator-focused specification. The facts are drawn
from the repository's hardware documentation, the Flipper RTL and the
system-library reverse-engineering notes, and are summarised (paraphrased) rather
than reproduced verbatim. Where a behaviour is only implicit in the RTL — a
time-out, the exact latitude of a field, or an ambiguity in the original design
note — it is called out as such. In every case the RTL is taken as authoritative
over the published register descriptions.

## 1. Overview

The DI is a **slave** on the Flipper register bus and a **master** on the
main-memory interface. It presents one interrupt (`di_piInt`, aggregated by the PI
as `DIINT`) and drives the pins to the drive. Software never touches the bus
directly; it only writes register values and lets the DI sequence the
handshake.

### 1.1 Position in the IO-top subsystem

The four self-contained peripheral interfaces — DI, SI, EXI and AI — are all
instantiations of a single **I/O (`io`) module** inside Flipper. That `io` block
contains, apart from its peripheral sub-blocks:

- an **IO register / CPU interface** (`io_Pi`) — decodes the four module bases
  (`0x0C006000` DI, `0x0C006400` SI, `0x0C006800` EXI, `0x0C006C00` AI), carries
  the 16-bit big-endian register data and multiplexes each sub-block's read data
  back to the Processor Interface;
- a **shared main-memory port** (`io_Mem`) — one 64-bit, 32-byte-line port to the
  Flipper memory controller, shared by a **round-robin IO-DMA arbiter** between
  the DI and the three EXI channels;
- the four **peripheral sub-blocks** — DI, SI, EXI (3 channels) and AI — each with
  its own interrupt line (`di_piInt`, `si_piInt`, `exi_piInt`, `ai_piInt`) that PI
  aggregates into `DIINT`/`SIINT`/`EXINT`/`AIINT`.

```
io  (I/O module)
├── io_Pi    — register/CPU interface (16-bit PI path, module decode)
├── io_Mem   — shared main-memory port + round-robin IO-DMA arbiter
├── io_di    — DI  (disk-drive command transport)          « this block »
├── io_Si    — SI  (serial / controller interface)
├── io_Exi   — EXI (expansion, 3 channels)
├── io_Ai_top— AI  (audio interface)
└── io_TstMux— test/scan mux and pad output-enable control
```


### 1.2 Sub-blocks

The `io` block instantiates the DI as `io_di`, which is built from five
sub-modules that together implement the whole interface:

| Module | Role |
|---|---|
| `io_dipctl` | The PI-facing control module — decodes the register address, implements the read mux, holds the programmer-visible status/cover/control registers (`DISR`, `DICVR`, `DICR`), derives the immediate/DMA and read/write strobes, and computes `di_piInt` |
| `io_didctl` | The external-device control module — the command/data state machine that drives the DIDD/DIDIR/DIHSTRB pins, implements the break and error/reset protocols, counts the number of bytes transferred, and feeds the command/immediate/DMA buffers |
| `io_dimctl` | The memory-side control module — the DMA controller. Holds `DIMAR`/`DILENGTH`, issues 32-byte requests to the memory controller, tracks the block count and the last-block flag, and manages the double-buffered DMA staging |
| `io_dififo` | The DMA data staging FIFO — 8 × 64-bit entries, written either from the drive (DVD→memory) or from memory (memory→DVD), read out on the opposite side |
| `io_dibuf` | The command + immediate register file — the three 32-bit command buffers (`DICMDBUF0..2`) and the 32-bit immediate buffer (`DIIMMBUF`), readable/writable by both the CPU and the DI's device state machine |

Data flow for a **read from disc** (the common case):

```
drive (DIDD) ──io_didctl──▶ io_dififo ── (32-byte full line) ──io_dimctl──▶ main memory
```

For a **write to disc** the same FIFO is filled from memory by `io_dimctl` and
drained to the drive by `io_didctl`.

### 1.3 Connection to the drive

The DI connects to the drive through the **DDU connector (P9)** on the
motherboard. The drive is powered (5 V), ground and the DI mains signals plus the
separate DVD-Audio pins; `MONI`/`MONOUT` on the connector are drive monitor
lines not used by the DI itself. Full connector detail is in the external-interface
hardware documentation; the signal-level description is in §2.

## 2. External pins / signals

All DI mains signals are **LVCMOS**. `DIBRK` is open-drain (output) and needs an
external pull-up. Data flows over the `DIDD[7:0]` bus whose direction is set by
`DIDIR`; the two strobes qualify data in opposite directions.

| Signal | Width | Dir | Description |
|---|---|---|---|
| `DIDD[7:0]` | 8 | I/O | Data bus. When `DIDIR`=0 the DI drives it (data is **written to** the drive, qualified by `DIHSTRB`); when `DIDIR`=1 the drive drives it (data **read from** the drive, qualified by `DIDSTRB`). During reset it is latched as the DI configuration (see §5) |
| `DIDIR` | 1 | O | Direction control. `0` = DI→drive (outputs), `1` = drive→DI (inputs) |
| `DIHSTRB` | 1 | O | Host strobe. On a **write** it qualifies `DIDD[7:0]` (data valid on its rising edge); on a **read** it is a *ready* line (asserted = DI ready to accept another data byte) |
| `DIDSTRB` | 1 | I | Device strobe. On a **read** it qualifies `DIDD[7:0]` (data valid on its rising edge); on a **write** it is a *ready* line (asserted = drive ready for another byte) |
| `DIERR` | 1 | I | Drive error. Edge-triggered; a falling edge from the drive halts the current command and may raise `DEINT`. The drive deasserts it only after the host sends the next command (typically a request-sense) |
| `DIBRK` | 1 | I/O | Break (open-drain, external pull-up). The DI drives it low while idle; to break, the DI releases it (external pull-up raises it) and the **drive** becomes master, then pulses it low and releases — the DI recognises the **rising edge** as the break acknowledge. The DI delays a break until any command packet transfer completes (§3) |
| `DICOVER` | 1 | I | Disc cover switch. `1` = cover open, `0` = cover closed |
| `DIRST` | 1 | O | Drive reset. Not generated by the DI; driven from the PI general/reset register. Asserting it resets the drive |

### 2.1 DVD-Audio (AIS) signals

Streamed disc audio is carried on separate pins into Flipper and crosses into the
Audio Interface (AI); they are not part of the DI command/data path, but are
listed here because they are physically part of the drive interface.

| Signal | Dir | Description |
|---|---|---|
| `AISD` | I | Serial left/right audio bitstream from the drive, synchronised to the rising edge of `AISCLK` |
| `AISLR` | O | Frame / left-right signal at the sample rate (32 kHz / 48 kHz). It also gates flow: if it does not toggle after a stereo sample, the drive treats the stream as paused and sends zeros; the drive starts only after a high-low-high sequence |
| `AISCLK` | O | Free-running bit clock for `AISD` |

The Audio Interface resamples the DVD-Audio stream to the mixer's sample rate; see
the AI documentation and [disk-drive.md](disk-drive.md).

## 3. Drive command model

Software drives the disc controller entirely through the register file. A
command is a **12-byte packet** held in `DICMDBUF0` (bytes 0–3), `DICMDBUF1`
(bytes 4–7) and `DICMDBUF2` (bytes 8–11). The packet is sent over `DIDD[7:0]`,
**high byte first** — the most-significant byte of `DICMDBUF0` is byte 0 and is
the first one transmitted. In practice byte 0 is the opcode.

Issuing a command consists of:

1. Writing the 12-byte packet into `DICMDBUF0..2`.
2. For a DMA transfer, writing `DIMAR` (destination/source address) and
   `DILENGTH` (byte count).
3. Starting the transfer by writing `DICR` with `TSTART`=1 and the direction
   (`RW`) and mode (`DMA`) bits.

### 3.1 Immediate vs DMA mode

`DICR[DMA]` selects how the *data* portion of a command is carried:

- **DMA mode** (`DMA`=1): data is streamed to/from **main memory** in 32-byte
  blocks through `io_dimctl`. `DIMAR`/`DILENGTH` name the memory operand. This is
  the normal bulk data path (game data reads, disc-ID reads, inquiries).
- **Immediate mode** (`DMA`=0): data is transferred to/from the on-chip
  **`DIIMMBUF`** (4 bytes) instead of memory. `DIMAR`/`DILENGTH` are ignored.
  The only packet command that uses immediate mode is the **register access**
  command — read or write a drive-internal register of 4 bytes or fewer.

Both modes send the full 12-byte command packet first. Immediate mode expects
exactly a **4-byte** data exchange after the command; DMA mode exchanges
`DILENGTH` bytes in 32-byte chunks.

### 3.2 Read / write direction

`DICR[RW]` picks the direction:

- **Read** (`RW`=0, "read from drive"): data flows drive→(DI)→destination. For
  DMA the destination is memory (`DIMAR`); for immediate it is `DIIMMBUF`. This
  is the normal case.
- **Write** (`RW`=1, "write to drive"): data flows source→DI→drive. For DMA the
  source is memory (`DIMAR`); for immediate it is `DIIMMBUF`. Used by the
  register-access write command.

### 3.3 Command-only transfer (zero length)

If a DMA transfer is started with `DILENGTH` = 0, the DI performs a
**command-only** transfer: it sends the 12-byte packet and then completes without
any data phase. This is how the drive's side-effect commands (seek, stop motor,
request error/status, audio-stream control, audio-buffer config) are issued and
how a "response-less" control command is dispatched.

### 3.4 `DIIMMBUF` immediate register access

In immediate mode, `DIIMMBUF` carries the register data. `REGVAL0` (bits [31:24])
is the byte at drive-register offset +0, `REGVAL1` (+1), `REGVAL2` (+2),
`REGVAL3` (+3). On a **read** command the drive returns the register contents
into `DIIMMBUF` (byte 0 → `REGVAL0`); on a **write** command the DI sends
`DIIMMBUF`'s bytes to the drive.

### 3.5 Break protocol

A break aborts the current operation. Software requests one by writing
`DISR[BRK]` = 1. The DI delays the assertion of `DIBRK` until the **whole command
packet** has been transferred (so a break can only take effect during a data
phase or while idle), then releases `DIBRK`. The drive becomes master, pulses the
line low, and releases; the DI sees the **rising edge** as the break
acknowledge.

After an acknowledged break:

- `DISR[BRK]` is cleared (the break request is no longer pending) and `DISR[BRKINT]`
  is set.
- `DICR[TSTART]` is cleared.
- Any in-flight data transfer is aborted; `DILENGTH` holds the **remaining** byte
  count that had not been transferred (§4.3).
- A new command may be sent.

A break issued during the command packet is held pending until the byte counter
indicates the command is complete, then the break sequence runs.

### 3.6 Error (`DIERR`) handling

The drive asserts `DIERR` on a falling edge to signal an error. The DI:

- **Halts the current command** immediately (the command/data state machine is
  reset by the error).
- Clears `DICR[TSTART]`.
- Sets `DISR[DEINT]` (subject to the error being aligned to a transfer boundary —
  the drive is required to place the error at the end of the command packet, or at
  the end of a 4-byte immediate or 32-byte DMA block — see §6.2).
- **Suppresses `TCINT`** for the current transaction (an errored transfer does
  not report "transfer complete").

`DIERR` is edge-triggered, so `DEINT` reflects the **event**, not the level. The
drive deasserts `DIERR` only after receiving the next command (typically a
request-sense that the software sends in response to the error interrupt).

### 3.7 The drive command set

The drive is a command-driven device: the host writes a **12-byte** packet into
`DICMDBUF0..2` (big-endian, so byte 0 of the packet is bits `[31:24]` of
`DICMDBUF0`) and sets `DICR[TSTART]`. An opcode is the first command byte
(CMDBYTE0). There are three kinds of command, distinguished by how they move data:

- **DMA** — bulk data moves between main memory and the drive. `DIMAR`/`DILENGTH`
  are set and `DICR = DMA | TSTART`.
- **Immediate (command-only)** — only the command packet is sent; no data moves
  through memory. `DICR = TSTART` only. The io-top specification notes that the
  only packet command which uses the immediate **data buffer** (`DIIMMBUF`) is a
  drive **register-access** command; the other control commands transfer no data
  at all.
- The command set observed in the system library is:

| Opcode | Command | Data | Packet / notes |
|---|---|---|---|
| `0x12` | Inquiry | DMA | Read the drive manufacturer info (`DVDDriveInfo` — revision, device code, release date). `CMDBUF2` = length, `MAR`/`LEN` = destination, `CR = DMA \| TSTART` |
| `0xA8` | Read | DMA | Read disc data. `subcmd` (`CMDBUF3`), `CMDBUF1 = offset >> 2`, `CMDBUF2` = byte length, `MAR`/`LEN` = destination, `CR = DMA \| TSTART` |
| `0xA8` sub `0x40` | Read disc ID | DMA | Read the 32-byte disc header (`DVDDiskID`) from the start of the disc; otherwise the same as `0xA8` |
| `0xAB` | Seek | none | Move the pickup to the sector containing `offset`. `CMDBUF1 = offset >> 2`, `CR = TSTART` |
| `0xE0` | Request error | none | Retrieve the last drive error / sense status. `CR = TSTART` |
| `0xE1` | DVD audio stream | none | Start/config a DVD-Audio stream. `subcmd` selects the stream, `CMDBUF1 = offset >> 2`, `CMDBUF2` = length, `CR = TSTART` |
| `0xE2` | Request audio status | none | Request DVD-Audio stream status. `subcmd` selects what is reported, `CR = TSTART` |
| `0xE3` | Stop motor | none | Stop the spindle. `CR = TSTART` |
| `0xE4` | Audio buffer config | none | Configure the streaming-audio buffer: `0xE4000000 \| (enable ? 0x1000 : 0) \| size`; low nibble of `size` = buffer size (≤ 16), high nibble = trigger (0–2), the `0x1000` bit sets the enable flag. `CR = TSTART` |

The `0xA8` **read** sub-command is `CMDBUF3` (bits `[7:0]` of `DICMDBUF0`); the
usual value is `0x00` for a normal data read and `0x40` for the disc-ID read.
`DMA` commands are initiated with `CR = DMA | TSTART`; control commands with
`CR = TSTART` only.

Address/alignment constraints enforced by the driver (and expected by the drive)
are worth modelling: the `offset` is byte-granular but must be a multiple of 4 and
is transmitted as `offset >> 2` in `CMDBUF1`; the data `length` must be a
multiple of 32 (one DMA block) and is given as a byte count; the destination
address in `DIMAR` must be 32-byte aligned. An `0xA8` offset beyond the media, or
an unsupported sub-command, is reported back through the drive-error interrupt
rather than through the register interface.

## 4. DMA transfers

`io_dimctl` is the DMA engine. It is a bus master on the main-memory interface and
transfers data in **32-byte (cache-line) transactions**, independent of the
requested length. It keeps a **double buffer** so the drive side and the memory
side can proceed independently; on a DVD→memory transfer the FIFO is filled by
the device state machine and drained a full line at a time to memory, on a
memory→DVD transfer the buffers are filled from memory and drained to the FIFO.

### 4.1 `DIMAR` and `DILENGTH`

| Field | Meaning |
|---|---|
| `DIMAR[25:5]` | 21-bit memory address, 32-byte aligned. It is auto-incremented by one block (+32 bytes) each time a block completes, so the register always holds the **next** address to use |
| `DILENGTH[25:5]` | 21-bit **byte** length, 32-byte aligned. It is decremented by one block (32 bytes) per block transferred; bits [4:0] read back as zero |

`DIMAR`/`DILENGTH` are 21-bit fields occupying bits [25:5]; bits [31:26] and [4:0]
are reserved and read as zero. Because of the field position, the value software
writes to `DILENGTH` in bytes is stored as `bytes >> 5` (the number of blocks).
A DMA transfer starts only when `DILENGTH` is non-zero; a zero length selects the
command-only path (§3.3).

### 4.2 Operation

For a **DVD→memory** read:

1. `io_didctl` sends the 12-byte command, then streams data into the FIFO one
   byte at a time, gated by `DIDSTRB`.
2. When a full 32-byte line is present, `io_dimctl` issues a memory write
   (`DiMemReq`/`DiMemRd`=0), increments `DIMAR` and decrements `DILENGTH`.
3. The last block is flagged (`lastdma`) and, for the write-to-memory direction,
   a **flush** handshake (`DiMemFlush`/`MemFlushAck`) confirms the write is
   committed before completion is declared.

For a **memory→DVD** write the sequence is reversed (memory read, then byte
stream to the drive). The address/width of `DiMemAddr[25:5]` constrains DMA to
the main-memory region (up to bit 25, i.e. the 24–32 MB memory window the PI
decodes).

### 4.3 `DILENGTH` after break / error / reset

The length register updates as blocks complete. If a DMA transfer is interrupted
by a **break**, an **error**, or a **soft reset**, the blocks that did not
complete are not counted, so `DILENGTH` holds the amount **left** to transfer
when the transfer was aborted. This lets software resume or account for the
partial transfer. (This is the RTL behaviour; the published description states
the same thing.)

## 5. Resets & config

### 5.1 DI reset (`DIRST` / PI `CONFIG`)

The drive reset (`DIRST`) is **not** driven by the DI itself — it comes from the
PI general/reset register. `CONFIG[2]` (`DIRSTB`) at `0x0C003024` is the disc-interface
reset control: writing `0` asserts reset, writing `1` releases it. `DIRSTB` is
read/write (its state is readable via `CONFIG`) and is **left asserted at power-on
until the boot code writes `CONFIG` to release it**, in contrast to the memory
interface which is released automatically shortly after power-up. The DI asserts
`DIRST` to the drive while `DIRSTB` is in its reset state. Asserting the
system-reset bit (`CONFIG[0]`/`SYSRSTB`) also pulses `DIRSTB`. See the
[processor-interface spec](processor-interface.md) §7.2 for the CONFIG model.

### 5.2 DI configuration latch (`DICONFIG`)

At reset the DI **latches the state of the `DIDD[7:0]` bus** into the read-only
`DICONFIG` register (8-bit `CONFIG` field in bits [7:0]). The latches sample the
bus while `resetb` is asserted and hold the value. The configuration is set by
the board's strapping on the data bus; currently **only bit 0 is meaningful** —
`DIDD[0]` selects whether the boot-ROM **scramble** is disabled. Other bits are
reserved.

## 6. Interrupts

The DI raises a single interrupt to the PI interrupt controller as `di_piInt`
(`DIINT`), which the PI aggregates as **INTSR bit 2** (`DIINT`/`DIMSK`). The
DI's own interrupt sources are the four status/mask pairs inside `DISR` and
`DICVR`:

| Status bit | Mask bit | Source |
|---|---|---|
| `DISR[BRKINT]` (6) | `DISR[BRKINTMSK]` (5) | Break completed (break acknowledge received from the drive) |
| `DISR[TCINT]` (4) | `DISR[TCINTMSK]` (3) | Transfer complete (DMA or immediate transfer finished) |
| `DISR[DEINT]` (2) | `DISR[DEINTMSK]` (1) | Drive error (`DIERR` asserted) |
| `DICVR[CVRINT]` (2) | `DICVR[CVRINTMSK]` (1) | Disc cover closed |

### 6.1 Aggregation and masking

`di_piInt` is the OR of the four `(status & mask)` pairs:

```
di_piInt = (BRKINT & BRKINTMSK) | (TCINT & TCINTMSK) | (DEINT & DEINTMSK) | (CVRINT & CVRINTMSK)
```

The mask bits only gate the interrupt to the CPU; they do **not** affect whether
the corresponding status bit is set in the register. Each status bit is set purely
by its hardware event; only its ability to reach `di_piInt` is masked. Because
the DRAM-side aggregation happens in the PI, an emulator models `di_piInt` as the
masked OR above and lets the PI combine it with the other sources.

### 6.2 Ordering (error vs transfer-complete)

The DI deliberately orders the completions so that an error is visible before a
transfer complete. `TCINT` is only asserted once the device strobe is low (the
"ready" state); this guarantees that if `DIERR` coincides with the end of a
transfer, the error (`DEINT`) is recorded and `TCINT` is **not** asserted. An
emulator should reproduce this: on a transfer that also ends in an error, raise
`DEINT` and suppress `TCINT` for that transaction.

### 6.3 Clearing semantics

Each status bit is **write-1-to-clear** — writing a `1` to the bit (in the
low halfword of `DISR`/`DICVR`) clears it; writing `0` has no effect. In the
low-level interrupt handler the software reads the register, and writes back the
active interrupt bits (to clear them) together with the current mask bits (to
preserve them). `DEINT` is additionally dependent on the drive deasserting
`DIERR` (which happens only after the next command is issued), so clearing the
status bit and externally resetting the drive's error line are both involved.

## 7. Register access (16-bit model)

The DI registers are reached through the Flipper **16-bit** IO register interface
(`PiData[15:0]`, `PiAddr[19:1]`, halfword-granular). A register is 32-bit
internally and addressed as two 16-bit halves: the **high word** at byte offset
`N` (even halfword), the **low word** at `N+2`. A 32-bit CPU access is two
back-to-back 16-bit transfers, high word first. Only 2-byte and 4-byte accesses
are valid.

Decode within the block uses `PiAddr[5:2]` to select one of the ten DI
registers (byte offset = index × 4). The high/low selection uses `PiAddr[1]`:

- `PiAddr[1]` = 0 selects the **high** halfword (offset `N`).
- `PiAddr[1]` = 1 selects the **low** halfword (offset `N+2`).

The writable halves differ by register type. The **control/status** registers
`DISR`, `DICVR` and `DICR` are written at the **low** halfword only; a write to
their high halfword is ignored (this is a direct consequence of the decode: the
write strobes are gated on `PiAddr[1]`). The **data/address/length** registers
(`DIMAR`, `DILENGTH`, `DICMDBUF0..2`, `DIIMMBUF`) use **both** halves — the high
halfword carries bits [31:16], the low halfword bits [15:0]. The read side mirrors
this: `DISR`/`DICVR`/`DICR`/`DICONFIG` are readable at the low halfword, while the
buffers and `DIMAR`/`DILENGTH` return both halves.

## 8. Register map

The DI block occupies `0x0C006000`–`0x0C006024` (uncached physical alias
`0xCC006000`). It is IO-module `io_addr_di` (`PiAddr[11:10]` = `0`). Each
register is 32-bit internally; the table gives the byte offset from
`0x0C006000`. Reset values are shown where a defined power-on value exists
(`–` means data-dependent / not initialised).

| Addr | Name | R/W | Description |
|---|---|---|---|
| `0x00` | `DISR` | R/W | Status: break, transfer-complete, drive-error; the break request bit; masks |
| `0x04` | `DICVR` | R/W | Cover: current cover state, cover-close interrupt + mask |
| `0x08` | `DICMDBUF0` | R/W | Command packet bytes 0–3 |
| `0x0C` | `DICMDBUF1` | R/W | Command packet bytes 4–7 |
| `0x10` | `DICMDBUF2` | R/W | Command packet bytes 8–11 |
| `0x14` | `DIMAR` | R/W | DMA memory address, 32-byte aligned (`[25:5]`) |
| `0x18` | `DILENGTH` | R/W | DMA byte length, 32-byte aligned (`[25:5]`) |
| `0x1C` | `DICR` | R/W | Control: read/write, DMA/immediate, transfer start |
| `0x20` | `DIIMMBUF` | R/W | Immediate-mode register data (4 bytes) |
| `0x24` | `DICONFIG` | RO | Drive configuration latched from `DIDD` at reset |

The register index derived from `PiAddr[5:2]` matches these offsets
(index × 4), and matches the register table in the drive/media document
`[disk-drive.md](disk-drive.md)`; the RTL confirms every offset. The
drive/media document's high-level table is otherwise consistent with the RTL, so
no offset correction is needed — the RTL merely pins down the **bit positions**
(the 21-bit `[25:5]` fields, for example) that the higher-level table leaves
implicit.

## 9. Register fields

### 9.1 `DISR` (0x00)

DI status register. Reset `0x0000`. Bits are write-1-to-clear for the status
bits; the mask bits are plain R/W. All bits live in the low halfword.

| Bits | Field | Type | Description | Reset |
|---|---|---|---|---|
| 6 | `BRKINT` | R/WC | Break-complete interrupt status. Set when the drive acknowledges a break (rising edge on `DIBRK`). Write `1` clears | 0 |
| 5 | `BRKINTMSK` | R/W | Break-complete interrupt mask (1 = enabled) | 0 |
| 4 | `TCINT` | R/WC | Transfer-complete interrupt status. Set when a DMA or immediate transfer completes (delayed until the device strobe is low so an error is seen first). Write `1` clears | 0 |
| 3 | `TCINTMSK` | R/W | Transfer-complete interrupt mask | 0 |
| 2 | `DEINT` | R/WC | Drive-error interrupt status. Set on the falling edge of `DIERR`. Write `1` clears the internal status; the drive also deasserts `DIERR` on the next command | 0 |
| 1 | `DEINTMSK` | R/W | Drive-error interrupt mask | 0 |
| 0 | `BRK` | R/WC | Break request. Write `1` to request a break; it is cleared by the break acknowledge. Read gives the request/pending state | 0 |
| 31:7 | — | R | Reserved | 0 |

### 9.2 `DICVR` (0x04)

DI cover register. Low halfword holds the fields; `CVR` is read-only and depends
on `DICOVER`.

| Bits | Field | Type | Description | Reset |
|---|---|---|---|---|
| 2 | `CVRINT` | R/WC | Cover-close interrupt status. Set when the cover **closes** (transition of `DICOVER` from open to closed). Write `1` clears | 0 |
| 1 | `CVRINTMSK` | R/W | Cover interrupt mask | 0 |
| 0 | `CVR` | R | Current cover state: 0 = closed, 1 = open (reflects `DICOVER`) | `–` |
| 31:3 | — | R | Reserved | 0 |

> **Correction vs. the published description.** The external-interface
> documentation describes `CVRINT` as firing whenever the `DICOVER` status
> *changes* (open *or* closed). The RTL, however, was changed to generate the
> cover interrupt **only when the cover is closed** — the set condition is
> `(~cover_now) & cover_prev` (cover was open and has become closed), not an
> arbitrary change of the signal. The RTL wins and is what an emulator should
> model.

### 9.3 `DICMDBUF0` (0x08)

Command packet bytes 0–3. All reset to `0`.

| Bits | Field | Type | Description |
|---|---|---|---|
| 31:24 | `CMDBYTE0` | R/W | Packet byte 0 — the first byte transmitted (the opcode) |
| 23:16 | `CMDBYTE1` | R/W | Packet byte 1 |
| 15:8 | `CMDBYTE2` | R/W | Packet byte 2 |
| 7:0 | `CMDBYTE3` | R/W | Packet byte 3 |

### 9.4 `DICMDBUF1` (0x0C)

Command packet bytes 4–7. Same layout: `CMDBYTE4` [31:24] … `CMDBYTE7` [7:0]. Reset `0`.

### 9.5 `DICMDBUF2` (0x10)

Command packet bytes 8–11. Same layout: `CMDBYTE8` [31:24] … `CMDBYTE11` [7:0]. Reset `0`.

### 9.6 `DIMAR` (0x14)

DMA memory address. A 21-bit field at `[25:5]`. Reset `0`.

| Bits | Field | Type | Description | Reset |
|---|---|---|---|---|
| 25:5 | `DIMAR` | R/W | Main-memory address for the current DMA (destination when `RW`=read, source when `RW`=write). Incremented by +32 per block transferred | 0 |
| 31:26 | — | R | Reserved | 0 |
| 4:0 | — | R | Reserved, read zero (all DMA is 32-byte aligned) | 0 |

The 21-bit field is written/read as two halves: the high halfword carries
`[25:16]` (in bits [9:0] of the halfword), the low halfword carries `[15:5]`
(in bits [15:5] of the halfword).

### 9.7 `DILENGTH` (0x18)

DMA byte length. A 21-bit field at `[25:5]`. Reset `0`.

| Bits | Field | Type | Description | Reset |
|---|---|---|---|---|
| 25:5 | `DILENGTH` | R/W | DMA byte length (32-byte aligned). Decremented by one block (32 bytes) per block transferred; on a break it holds the **remaining** byte count. Zero selects a command-only transfer | 0 |
| 31:26 | — | R | Reserved | 0 |
| 4:0 | — | R | Reserved, read zero (lengths are multiples of 32) | 0 |

### 9.8 `DICR` (0x1C)

DI control register. Reset `0`. Written at the low halfword.

| Bits | Field | Type | Description | Reset |
|---|---|---|---|---|
| 2 | `RW` | R/W | Direction: 0 = read (drive→DI), 1 = write (DI→drive) | 0 |
| 1 | `DMA` | R/W | Mode: 0 = immediate (`DIIMMBUF`), 1 = DMA (`DIMAR`/`DILENGTH`, main memory) | 0 |
| 0 | `TSTART` | R/WC | Transfer start. Write `1` to execute the command (and read the status). Cleared by transfer complete, by a break acknowledge, or by `DIERR` | 0 |
| 31:3 | — | R | Reserved | 0 |

### 9.9 `DIIMMBUF` (0x20)

Immediate-data buffer. Reset `0`.

| Bits | Field | Type | Description |
|---|---|---|---|
| 31:24 | `REGVAL0` | R/W | Drive register offset +0 data |
| 23:16 | `REGVAL1` | R/W | Drive register offset +1 data |
| 15:8 | `REGVAL2` | R/W | Drive register offset +2 data |
| 7:0 | `REGVAL3` | R/W | Drive register offset +3 data |

### 9.10 `DICONFIG` (0x24, read-only)

Drive configuration. Reset value is latched from `DIDD[7:0]` at reset.

| Bits | Field | Type | Description | Reset |
|---|---|---|---|---|
| 7:0 | `CONFIG` | R | Latched configuration. Bit 0 = ROM-scramble disable; remaining bits reserved | `DIDD` strapped |
| 31:8 | — | R | Reserved | — |

## 10. Emulator notes

1. **Register file and 16-bit port.** Model each DI register as a 32-bit value,
   but route all access through the 16-bit interface: a 2-byte access touches one
   halfword, a 4-byte access does two halfword accesses, high word first. Since
   this block is in the IO space, reject (via the PI error) any non-2/4-byte
   access and any burst. Apply the high/low write-locality: gate writes to
   `DISR`/`DICVR`/`DICR` on the **low** halfword (`PiAddr[1]` = 1), while
   `DIMAR`/`DILENGTH`/`DICMDBUF0..2`/`DIIMMBUF` accept both halves.
2. **Command packet.** Treat the three command buffers as a 12-byte big-endian
   packet (byte 0 = `DICMDBUF0[31:24]`, transmitted first). Read the packet from
   the register values; send byte 0 to the drive first.
3. **Modes.** On `TSTART` rising, decode `DMA`/`RW` and `DILENGTH`: if `DMA`=0,
   do the immediate 4-byte transfer via `DIIMMBUF`; if `DMA`=1 and `DILENGTH`>0,
   do a DMA transfer; if `DMA`=1 and `DILENGTH`=0, do a command-only transfer.
   For DMA read, copy drive data into memory starting at `DIMAR`; for DMA write,
   copy memory at `DIMAR` to the drive. Increment `DIMAR` and decrement
   `DILENGTH` by 32 bytes per block.
4. **DMA block flow.** Emulate the double-buffered 32-byte staging; model the
   transfer as a sequence of 32-byte blocks to/from main memory. Issue a flush
   (commit) at the end of a DVD→memory transfer before declaring completion.
5. **Break.** `DISR[BRK]`=1 requests a break; if a command packet is mid-transfer,
   defer it until the packet is fully sent. On the later break acknowledge, clear
   `DISR[BRK]`, set `DISR[BRKINT]`, and clear `DICR[TSTART]`. Abort the DMA and
   leave `DILENGTH` at the not-yet-transferred count.
6. **Error.** `DIERR` falling edge sets `DISR[DEINT]`, clears `DICR[TSTART]`, and
   suppresses `TCINT` for that transaction. Clear `DEINT` on `DISR[2]` write-1.
7. **Cover interrupt.** Track `DICOVER`. Set `DICVR[CVR]` to the current level
   always. Set `DICVR[CVRINT]` **only on the cover-closed transition** (open →
   closed), per the RTL, not on any change.
8. **Interrupt line.** Drive `di_piInt` = `(BRKINT&BRKINTMSK) | (TCINT&TCINTMSK)
   | (DEINT&DEINTMSK) | (CVRINT&CVRINTMSK)`, and hand it to the PI as INTSR bit 2
   (`DIINT`). Do not infer the PRI inside the DI; the PI aggregates.
9. **Reset / config.** Model `DICONFIG` as an 8-bit latch of the strapped
   `DIDD[7:0]` reading bit 0 = ROM-scramble-disable. Drive `DIRST` from the PI
   `CONFIG[2]` (`DIRSTB`), and reset the DI's command/data state machine on its
   asserted-to-released transition.
10. **Direction of control writes.** Remember that the DI command is initiated by
    writing `DICR` with `TSTART`=1 in the **low** halfword. Software commonly
    writes `DICR` (and `DISR`/`DICVR`) as a 32-bit value; the high halfword is
    ignored, so only the low halfword's bits matter for these registers.

## 11. References

- `HW/IO/DiskInterface.md` — DI signal definitions, DDU connector, and register
  field descriptions (based on the external-interfaces patent). Baseline for signal
  naming/meaning; bit positions cross-checked against the RTL.
- `specs/architecture/disk-drive.md` — the 3" mini-DVD media and drive
  architecture; cross-linked here for the drive model, DVD-Audio stream and
  copy-protection overview.
- `specs/architecture/processor-interface.md` — §3 register access (16-bit
  model), §4 interrupts (`DIINT` is INTSR bit 2), §7.2 `CONFIG[2]` `DIRSTB`
  disc-interface reset, and the PI error model for the IO register space.
- `RE/DVD/DVDLow.c` — system low-level drive command routines (command-packet
  layout, immediate/DMA start, break, error/interrupt handling, cover handling;
  the `0x3024` reset-register usage).
- `RE/DVD/dvdfs.txt` — disc filesystem / command-block notes.
- **US Patent 6,609,977** (External Interfaces) — the DI signal and register
  behaviour, break protocol, and immediate/DMA/register-access modes.

> Register-level facts are taken from the Flipper RTL sources, which are the
> authority for the DI sub-block structure (`io_dipctl`, `io_didctl`,
> `io_dimctl`, `io_dififo`, `io_dibuf`), the exact bit positions and the
> interrupt/break/error logic. All such facts above are paraphrased from them.
