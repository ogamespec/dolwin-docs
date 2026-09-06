# GameCube — Serial Interface (SI)

> **SI** (the Serial Interface) is the block inside the **Flipper** ASIC that drives
> the four GameCube **controller ports** (and the keyboard, which shares the same
> connector type). It is the serial clock and framing engine that turns a short
> command/response packet into a single half-duplex, pulse-width-modulated line per
> channel, periodically polls the controllers on a schedule tied to the video
> vertical blank, and captures the controller's reply into a double-buffered set
> of registers. A small **Communication RAM** and a second, software-initiated
> transfer mode (`SICOM`) let the same hardware talk to any intelligent device on a
> channel (for example a Game Boy Advance) using variable-length transfers.

This is the register-level, emulator-focused specification. The facts are drawn
from the repository's hardware documentation, the Flipper RTL (`io_Si*`) and the
reverse-engineering notes, and are summarised (paraphrased) rather than reproduced
verbatim. Where a behaviour is only implicit in the RTL — a time-out, the exact
meaning of a length field — it is called out as such. This is *not* the controller
itself: the SI only supplies the electrical framing and the registers; the device
on the other end of the wire responds according to its own protocol (§3).

The SI is one of the four blocks in the Flipper **IO-module** space (with DI, EXI
and AI). It is decoded as IO module `io_addr_si` (`PiAddr[11:10] == 1`) and sits at
**physical base `0x0C006400`** (uncached alias **`0xCC006400`**).

## 1. Overview

The SI is a small, heavily state-machine-driven block. It has **four identical
channels**; each channel owns one controller port and the full command/response
buffering for that port. A single polling counter is shared by all four channels
and decides *when* a channel is polled; each channel has its own state machine that
implements the transfer itself.

It is one of the four peripheral sub-blocks of the Flipper **I/O (`io`) module**
(the others being DI, EXI and AI). The `io` block also contains the 16-bit register
interface (`io_Pi`, which decodes the `0x0C006000`–`0x0C006C00` module bases) and a
shared main-memory port (`io_Mem`) with a round-robin IO-DMA arbiter; each
peripheral sub-block has its own interrupt (`si_piInt` here) that PI aggregates. The
SI does **not** use the shared memory port — it is a purely PIO-driven block (the OS
reads and writes the registers directly), so it is the only one of the four with no
DMA path.

```
io  (I/O module)
├── io_Pi    — register/CPU interface (16-bit PI path, module decode)
├── io_Mem   — shared main-memory port + round-robin IO-DMA arbiter
├── io_di    — DI  (disk-drive command transport)
├── io_Si    — SI  (serial / controller interface)          « this block »
├── io_Exi   — EXI (expansion, 3 channels)
├── io_Ai_top— AI  (audio interface)
└── io_TstMux— test/scan mux and pad output-enable control
```


### 1.1 Sub-blocks

| Block | Role |
|---|---|
| `io_Si` | Top level — register address decode, the read/write datapath mux, the channel-to-SRAM routing and the interrupt aggregation |
| `io_SiCSR` | The control/status registers — `SICOMCSR`, `SISR`, `SIEXILK`; the interrupt/mask bits; the WR and TSTART decoding; the per-channel error latch |
| `io_SiPoll` | The shared polling counter — `SIPOLL` X/Y, per-channel enable, vblank-copy, the HSync/VSync-derived poll schedule |
| `io_SiXfer` | The serial **transfer engine** — shift out the command, shift in the response, duty-cycle (pulse-width) modulation, big-endian, error detection (overrun/underrun/collision/no-response) |
| `io_SiBuf` | The per-channel **double buffers** — the CPU-visible `SICnOUTBUF`/`SICnINBUFH/L` and the shadow buffers used during an actual transfer, plus the lock/unlock read sequence |
| `io_SiRAM` | The **Communication RAM** — 128 bytes of single-port SRAM, byte addressing to the SI, 16-bit word addressing to the CPU |
| `io_SiCh` | Per-channel wrapper — instantiates one `io_SiBuf`, one `io_SiXfer` and one `io_SiSM` |
| `io_SiSM` | Per-channel **master state machine** — sequences COM vs polling, counts bytes, talks to the RAM and buffers |
| `io_SiClk` | The SI clock generator — divides the Flipper IO clock down to the ~2 MHz serial tick |
| `io_SiSync` | Clock-domain synchronisers for the 27 MHz VSync/HSync and the serial data line |

### 1.2 The four channels

Channels 0–3 each carry one controller port. In the retail console the controller
ports 0–3 correspond to channels 0–3, and the same four channels are what an
emulator models. There is no inherent difference between the channels beyond their
register address and their physical port; the block is fully symmetric.

## 2. External pins / signals

The SI connects to the four controller connectors and to the video interface (for
the poll timing). The light-gun trigger pins are physically on the same connector
group but are **not consumed by the SI** — they pass straight through to the VI's
beam-position latch (see below).

| Signal | Width | Direction | Description |
|---|---|---|---|
| `sidi0`..`sidi3` | 1 each | in | Serial data **in** from channels 0–3 (controller → SI) |
| `sido0`..`sido3` | 1 each | out | Serial data **out** to channels 0–3 (SI → controller), driven **inverted** (negative logic for the external buffer) |
| `guntrg[1:0]` | 2 | in | Light-gun trigger inputs — **pass through** to the VI's HV counter/latch for connectors 1 and 2 (see below) |
| `vi_ioVsync` | 1 | in | Vertical blank from the VI — used to schedule the poll counter |
| `vi_ioHsync` | 1 | in | Horizontal blank from the VI — the poll interval clock |
| `si_piInt` | 1 | out | SI interrupt request to the PI (aggregated as `SIINT`, PI `INTSR` bit 3) |
| `Lock32MHz` | 1 | out | Lock-out of the EXI 32 MHz clock setting (from `SIEXILK`) |

The four controller ports (motherboard `§4.2`) are identical: communication is
serial and half-duplex over a **single** data line per channel, using
pulse-width/duty-cycle modulation, transferred big-endian. The command/response
packets are only a few bytes; the device is intelligent, so the SI does not decode
the packets, it only clocks them.

### 2.1 Light-gun triggers — a note on ownership

`guntrg[0]` and `guntrg[1]` are brought out beside the controller data lines, but
in the Flipper RTL they are **passed straight through the `io` module** to the VI
(`io_TstMux`, `io.v` — "gun trg pass through to VI"). The VI latches the current
beam (HV) position when a trigger edge arrives, into its `VI_LATCH_0/1` registers.
The SI has **no** trigger input and no trigger register; emulators should route the
light-gun trigger to the VI, not to the SI. This is called out because a casual
reading of the connector group (motherboard groups `guntrg` under the "Controller
(SI)" heading) can mislead.

## 3. The controller protocol

The SI speaks a byte-oriented serial protocol to the device on the wire. The
hardware does the framing; the semantics of the bytes are the device's business.

### 3.1 Command/response packet

- The host (via `SICnOUTBUF`) supplies a **3-byte** command packet for a poll:
  `CMD`, `OUTPUT0`, `OUTPUT1`. These are shifted out **in that order** — `CMD`
  first (byte 0), then `OUTPUT0`, then `OUTPUT1`.
- The device replies with a **response packet**. For a poll the SI always shifts
  in **8 bytes** (`INPUT0`..`INPUT7`). The device returns fewer meaningful bytes
  for a short reply (e.g. the 3-byte controller status), leaving the rest as `0`.
- The very first response byte, `INPUT0`, is captured as **6 bits** — the top two
  bits of the byte coming back are assumed to be `0` and are not stored (see
  `SICnINBUFH[INPUT0]`). This is a hardware quirk of the receiver, not a bit
  count the software chooses.

The command bytes are **opaque** to the SI. It does not decode `CMD`; it merely
clocks out whatever three bytes are in the output buffer. This is why the same
block can drive a standard GameCube controller, a keyboard, a Game Boy Advance on
an SI port, or anything else that follows the same framing.

### 3.2 Framing (duty-cycle / pulse-width modulation)

Communication is **half-duplex** over a **single** line per channel and uses
**duty-cycle (pulse-width) modulation** — a data bit's value is carried by how
long the line is asserted within a bit cell, not by a level. The framing is
big-endian: the **most significant bit** of each byte is sent first.

The transmitted frame around each byte is:

| Phase | Meaning |
|---|---|
| start bit | The line is pulled to the "start" polarity (a falling edge) to mark the beginning of a byte |
| 8 data bits | MSB first; each bit is a high pulse whose width encodes a 1 or a 0 |
| stop bit | The line returns to the idle polarity |
| end | A short end/termination pulse after the last data byte |
| separation | Passive gap before the next transfer |

The receiver (in `io_SiXfer`) samples the line within each bit cell, integrates
the number of "high" samples (`sidiSum`), and **slices** at an accumulated count:
if 4 or more of the sample window read high the bit is a `1`, else a `0`. The
comment in the RTL notes this "favors 1 over 0"; the exact high-pulse duty for a
`1` and a `0` are chosen so the integrator lands on opposite sides of the slice
threshold. The output driver is **inverted** (negative logic), so what the SI
drives and what the device sees are complementary — an emulator modelling the
electrical level should invert the SI output before summing the pulse window.

A byte is clocked out/in at the SI bit rate, derived by `io_SiClk` (a counter on
the Flipper IO clock). The retail divider produces a serial tick of about **2 MHz**
(resolution of the sample counter); the exact bit clock depends on the configured
divider and thus on the IO clock. An emulator can model a fixed bit time; the SI
does not need the precise cycle count to be functionally correct.

### 3.3 The "0x00 trigger"

The byte value `0x00` is a protocol-level convention, not a hardware rule. The
standard GameCube controller uses command `0x00` as the **poll/status** request:
the host writes `CMD = 0x00` into `SICnOUTBUF` and the controller responds with its
current button/stick state. Other command values request the other sub-channels
(status, origin/calibration, etc.). Because the SI treats `CMD` as opaque data
(§3.1), this "trigger" is realised purely in software — the SI does not interpret
`0x00` specially. The only hardware place `0x00 → 0` is meaningful is the **length
fields** of `SICOMCSR`, where a value of `0x00` means **128 bytes** (§6.3), and the
receiver's special handling of the first response byte's top two bits.

## 4. Channels & buffers

Each channel has its own double-buffered output and input registers, plus the
shadow buffers that actually talk to the wire. The whole design exists so a poll
in progress on the wire cannot be corrupted by a concurrent CPU register access.

### 4.1 Output buffer (`SICnOUTBUF`)

The CPU-visible `SICnOUTBUF` is `CMD` / `OUTPUT0` / `OUTPUT1`. It is **double
buffered**: the CPU writes to the visible register, and at the appropriate time the
hardware copies it to a hidden shadow buffer (`SiOutBuf0`) that the shift engine
reads. The copy is triggered by setting `SISR[WR]` (bit 31), which sets `WRSTn`
until every channel has copied. The copy is **suppressed while an output transfer
is running** on that channel (`SetOutCopyLock`), except during a `SICOM` transfer,
so a mid-transfer register write does not corrupt the bytes being shifted out.
When `SIPOLL[VBCPYn]` is set the copy is deferred to the next vertical blank
(§5, for 3D shutter glasses).

### 4.2 Input buffer (`SICnINBUFH` / `SICnINBUFL`)

The response is captured by the shift engine into a hidden shadow buffer
(`SiInBuf0`), then copied to the visible `SICnINBUFH`/`SICnINBUFL` once the
transfer is complete (`InCopyGate`). The input registers are **double buffered** and
use a **locking read sequence**:

1. The **shadow → visible** copy only happens when the input buffer is **not
   locked** (`InCopyGate = InCopy && ~InCopyLock`).
2. **Reading `SICnINBUFH`** sets the lock (`SetInCopyLock = RdSiInBufHH`). From then
   on no new data is copied into `SICnINBUFH`/`SICnINBUFL`, protecting the CPU's
   read from being overwritten by a newer poll.
3. **Reading `SICnINBUFL`** clears the lock (`ClrInCopyLock = RdSiInBufLL`), so
   subsequent polls can update the buffer again.

In the RTL the lock fires on the read of `SICnINBUFH` and the unlock on the read of
`SICnINBUFL`; with the 16-bit access model this means a **32-bit** read of
`SICnINBUFH` locks at its high halfword and a **32-bit** read of `SICnINBUFL`
unlocks at its low halfword (§9). The response bytes are big-endian: `INPUT0` is
the most-significant byte of `SICnINBUFH`, `INPUT7` the least-significant of
`SICnINBUFL`.

The `SICnINBUFH` high bits carry the per-poll error summary (see §7):
- `ERRSTAT` (bit 31) — the **last** poll on this channel had an error.
- `ERRLATCH` (bit 30) — the OR of the latched error bits in `SISR` for this channel
  (set if *any* past poll or COM transfer on the channel had an error).

## 5. Polling

The SI polls controllers automatically so the CPU does not have to poll them by
software. `io_SiPoll` is a shared counter driven by the VI's HSync/VSync.

### 5.1 Schedule

`SIPOLL[X]` is the poll **interval** — the number of horizontal video lines between
polls. `SIPOLL[Y]` is the number of polls per frame. Polling begins at (and is
anchored to) the vertical blank:

- At `VSync` the poll counters are captured (`SiPollXV`/`SiPollYV`) and, for each
  enabled channel, `EnSync[n]` is set. Polling starts at the next `#VBlank` after
  a channel's enable bit is set — not immediately when the bit is written.
- A poll is issued at `VSync` and then every `X` HSync lines, up to `Y` polls per
  frame. When `Y` is reached, polling stops for that frame.
- If `Y == 0`, **no polling occurs** (`PollMaster` requires `Y != 0`).
- Disabling `SIPOLL[ENn]` stops polling **immediately** after the current
  transaction; a channel with `ENn = 0` is never polled.

`X` takes effect after `VSync` (the captured copy is used), so a change to `X`/`Y`
during a frame applies from the next blank. The default `X = 0x07` is the minimum
workable interval (limited by how long a single poll takes to complete).

### 5.2 Per-channel enable / vblank copy

`SIPOLL[EN0..EN3]` enable polling on channels 0–3. `ENn` gates **only** the
automatic poll schedule; it has **no effect** on `SICOM` transfers — a software
COM transfer to a channel runs even if that channel is not enabled for polling.

`SIPOLL[VBCPY0..VBCPY3]` (vblank copy) defer the `SICnOUTBUF` → shadow-buffer copy
to the next vertical blank. Normally the copy happens as soon as `SISR[WR]` is set;
with `VBCPYn` set, the copy is held until `VSync`. This is used to keep the
command timing to **3D LCD shutter glasses** (driven from the VI's output)
synchronised to the field boundary, so all channels' commands hit the shutter
at the same point in the frame.

### 5.3 Polling resuming after `SICOM`

After a `SICOM` transfer completes, the channel's state machine goes to a
`synchronise` state and waits for the next vertical blank before returning to idle.
If that channel's `SIPOLL[ENn]` is set, automatic polling resumes at the next
`#VBlank` (this is what the register documentation states, and what the RTL state
machine `SI_COM_SYNC` implements). A new COM transfer started immediately (a new
`TSTART`) bypasses this wait and runs at once.

## 6. Communication transfers (`SICOM`)

Besides polling, each channel can be used for a software-driven, variable-length
transfer to/from the **Communication RAM**. This is started by writing
`SICOMCSR[TSTART] = 1` and is the mode used for devices (like a Game Boy Advance)
that speak a longer protocol.

### 6.1 `SICOMCSR` control/status

| Field | Bits | Meaning |
|---|---|---|
| `TSTART` | 0 | Write 1 to start a COM transfer; read 1 = pending, 0 = complete |
| `CHANNEL` | 2:1 | Which channel (0–3) the transfer uses |
| `INLNGTH` | 14:8 | Response (controller→SI) length in bytes; 0 ⇒ 128 bytes |
| `OUTLNGTH` | 22:16 | Command (SI→controller) length in bytes; 0 ⇒ 128 bytes |
| `COMERR` | 29 | Read-only; 1 if the **last** COM transfer completed in error |
| `RDSTINT` | 28 | Read-status interrupt flag (see §8) |
| `RDSTINTMSK` | 27 | Read-status interrupt mask/enable |
| `TCINT` | 31 | Transfer-complete interrupt status/clear (write 1 to clear) |
| `TCINTMSK` | 30 | Transfer-complete interrupt mask/enable |

The `CHANNEL`, `INLNGTH` and `TSTART` fields live in the **low** halfword; the
`OUTLNGTH` and the interrupt/mask bits live in the **high** halfword (§11).

### 6.2 Communication RAM (`SIRAM`)

`SIRAM` is a **128-byte** block of single-port SRAM at **`0x0C006480`** (byte
offset `0x80` from the SI base). It is organised as **64 × 16-bit words** (big
endian — the high byte at an even address, the low byte at the following odd
address). The SI side addresses it byte-wise (8-bit); the CPU side addresses it
word-wise (16-bit).

It is **not** a normal register and has its own access rules:

- It can be accessed by the CPU **or** by the SI (during a COM transfer), **never
  both simultaneously**.
- During a COM transfer (`TSTART` active) the **SI has priority**: CPU writes to
  the RAM have **no effect** and CPU reads return **undefined/incorrect** data.
- Otherwise the CPU reads/writes it as 16-bit words; it is 128 bytes so the
  address range is `0x0C006480`–`0x0C0064FF`.
- The SI drives the RAM from the transfer engine's input/output byte stream, using
  a byte-address pointer that is reset to 0 at the start of each transfer.

### 6.3 Length semantics — the `0 ⇒ 128` encoding

`INLNGTH`/`OUTLNGTH` are 7-bit fields. The documented meaning is that a value of
**`0` transfers 128 bytes** and `1..127` transfer that many bytes (the minimum
transfer is 1 byte). In the RTL this works because the transfer counter is **7
bits** and is compared against the length field: it counts `0..127` and wraps to
`0` modulo 128, so a field of `0` matches only after **128** byte positions —
giving exactly a 128-byte transfer. Consequently the effective length is the field
value taken modulo 128, and you cannot request a zero-byte transfer.

These fields should not be modified while a COM transfer is in progress.

### 6.4 Transfer sequence

Per channel, a COM transfer is: shift out `OUTLNGTH` bytes from the RAM (with the
data source switched from the output buffer to the RAM), then shift in `INLNGTH`
bytes into the RAM, then set `TCINT` (and `COMERR` if any error latched), then wait
for `VSync` before the next poll. The transfer is purely byte-clock driven; the
channel is busy for the whole time and a poll on the same channel waits until it is
through.

## 7. Errors

The SI detects transmission faults at the framing level and reports them two
ways: per-channel error bits in `SISR`, and a summary in `SICnINBUFH`.

### 7.1 `SISR` per-channel bits

Each channel occupies 6 bits of `SISR` (the channel groups are laid out with
channel 0 at the high end and channel 3 at the low end, see §11). Within a
channel the bit order (from the LSB side) is:

| Bit in group | Name | Meaning |
|---|---|---|
| 0 | `UNRUNn` | Underrun — the receiver got **fewer** data bits/bytes than expected |
| 1 | `OVRUNn` | Overrun — the receiver got **more** data than expected (a bit arrived in the "overrun" window) |
| 2 | `COLLn` | Collision — the line was driven to a value that disagreed with what the SI was sending (data or stop collision) |
| 3 | `NOREPn` | No response — no device answered within the no-response time-out (also set when no controller is present) |
| 4 | `WRSTn` | Write status — the shadow output buffer has not yet been copied from `SICnOUTBUF` |
| 5 | `RDSTn` | Read status — new poll data is in the input buffer and has not yet been read |

`UNRUN`/`OVRUN`/`COLL`/`NOREP` are **latched** and each is cleared by writing a `1`
to its own bit (write-1-to-clear). `NOREP` staying set is a reliable way to detect
an absent controller. `RDSTn` is set when data is copied into the input buffer and
cleared when the CPU reads `SICnINBUFH`. `WRSTn` is set when `SISR[WR]` is written
and cleared when the shadow output buffer copy completes.

### 7.2 `SISR[WR]` (bit 31)

A single, global **write** bit. Writing `1` copies all channels' output buffers to
their shadows; reading it reports whether the copy is still pending
(1 = not copied, 0 = copied). It returns to 0 only when **every** channel has
finished copying (`WRST0..WRST3` all clear).

### 7.3 `SICnINBUFH` summary

`ERRSTAT` (bit 31) = the last poll on the channel had an error (refreshed on every
poll). `ERRLATCH` (bit 30) = the OR of the channel's `NOREP`/`COLL`/`OVRUN`/`UNRUN`
in `SISR`, so it stays set until those bits are individually cleared. It is an
"something went wrong since you last looked" flag; read `SISR` for the specifics.

### 7.4 `SICOMCSR[COMERR]`

`COMERR` (bit 29) is set when the last **COM transfer** completed with an error on
the selected channel; it is read-only and is re-evaluated on each COM completion.

## 8. Interrupts

The SI produces a **single** interrupt `si_piInt` to the PI's interrupt controller,
aggregated as **`SIINT`** — PI `INTSR` bit 3 (`SIINT`/`SIMSK`). The SI only has two
interrupt *events*, and only two mask bits control them:

| Event | Flag | Mask | Meaning |
|---|---|---|---|
| Poll data available | `SICOMCSR[RDSTINT]` | `RDSTINTMSK` | Set when **any** `SISR[RDSTn]` is set (a poll completed and new input data is in the buffer); cleared when all `RDSTn` clear (i.e. the CPU has read every channel's input buffer) |
| COM transfer complete | `SICOMCSR[TCINT]` | `TCINTMSK` | Set when a COM transfer finishes; **write 1 to clear** |

The actual line is asserted when `(TCINT & TCINTMSK) || (RDSTINT & RDSTINTMSK)`.
The per-channel `SISR` error bits and `COMERR` do **not** raise the interrupt on
their own; they are status the CPU reads in the handler. Both interrupt flags and
both masks reset to 0 (masked), so a freshly powered SI raises no interrupt.

The PI masks/aggregates this as `SIINT`/`SIMSK` at `INTSR` bit 3; emulators route
`si_piInt` there (see [processor-interface.md](processor-interface.md) §4.1).

## 9. Register access

Like every Flipper IO register, the SI is reached through the **16-bit** IO
register interface. `PiData[15:0]` is the data, `PiAddr[7:1]` is the halfword-
granular address (the low bit, `PiAddr[1]`, selects one half of a 32-bit register),
and the access is one 2-byte or one 4-byte transfer. There is no 32-bit register
port.

- A **32-bit** access is split into two 16-bit transfers, **big-endian**: the
  **high** word is transferred at the base (even) address and the **low** word at
  base+1 (odd). The read datapath muxes `[31:16]` for `PiAddr[1]=0` and `[15:0]`
  for `PiAddr[1]=1`.
- Registers are **32-bit** internally, addressed at byte offset `index × 4`, and
  asserted to be 32-bit by the documentation. The byte offsets in §10 are the
  register base addresses.

### 9.1 `SIRAM` and the read-ready handshake

`SIRAM` is not a normal register, and the SI returns its read data one clock after
the request. The SI block registers its read-output mux (`SiPiData <= SiPiDataMux`)
and drives `SiRamRdy` one clock after `PiSiSel` is asserted, so the whole SI
(registers and RAM) is "ready a clock later than other IO". Reads to the RAM region
in particular require this ready handshake because the SRAM read data is
registered; an emulator should return the RAM/register read value with a one-cycle
delay (or otherwise respect the ready signal) rather than returning it in the same
cycle as the request. Reads of the RAM while a COM transfer is in progress return
undefined data (see §6.2).

## 10. Register map

Registers are at byte offsets from the SI base `0x0C006400` (uncached alias
`0xCC006400`). Each register is 32-bit, reached as two 16-bit halves (§9). Offsets
are `index × 4`, where `index` is the register index used in the FDL/RTL decode
(`PiAddr[7:2]`).

| Offset | Name | R/W | Description |
|---|---|---|---|
| `0x00` | `SIC0OUTBUF` | RW | Channel 0 command/data out |
| `0x04` | `SIC0INBUFH` | RO | Channel 0 response, high word (`INPUT0`–`INPUT3`, error summary) |
| `0x08` | `SIC0INBUFL` | RO | Channel 0 response, low word (`INPUT4`–`INPUT7`) |
| `0x0C` | `SIC1OUTBUF` | RW | Channel 1 command/data out |
| `0x10` | `SIC1INBUFH` | RO | Channel 1 response, high word |
| `0x14` | `SIC1INBUFL` | RO | Channel 1 response, low word |
| `0x18` | `SIC2OUTBUF` | RW | Channel 2 command/data out |
| `0x1C` | `SIC2INBUFH` | RO | Channel 2 response, high word |
| `0x20` | `SIC2INBUFL` | RO | Channel 2 response, low word |
| `0x24` | `SIC3OUTBUF` | RW | Channel 3 command/data out |
| `0x28` | `SIC3INBUFH` | RO | Channel 3 response, high word |
| `0x2C` | `SIC3INBUFL` | RO | Channel 3 response, low word |
| `0x30` | `SIPOLL` | RW | Poll schedule (`X`, `Y`, per-channel enable, vblank-copy) |
| `0x34` | `SICOMCSR` | RW | Communication control/status |
| `0x38` | `SISR` | RW | Per-channel status/error and the global write bit |
| `0x3C` | `SIEXILK` | RW | EXI 32 MHz clock lock |
| `0x80` | `SIRAM` | RW | Communication RAM, 128 bytes (64 × 16-bit words) |

Note that `SIRAM`'s offset is `0x80` and its decode is by `PiAddr[7]` (the RAM
region is distinguished from the status/control registers by that bit), not by an
even `index × 4` that continues the list above.

## 11. Register fields

Real bit positions. Bits are described LSB-first (the first field listed is bit 0)
and confirmed against the RTL and the patent-derived `HW/IO/si.htm` table.
`-` means reserved/unused and reads as 0 unless stated.

### 11.1 `SICnOUTBUF` (0x00 / 0x0C / 0x18 / 0x24 — 32-bit, RW)

| Bits | Field | Description | Reset |
|---|---|---|---|
| 23:16 | `CMD` | Command byte, sent first in the command/response packet | 0 |
| 15:8 | `OUTPUT0` | Second data byte sent (conventionally 0 for a poll-style command) | 0 |
| 7:0 | `OUTPUT1` | Third data byte sent | 0 |
| 31:24 | — | Reserved | 0 |

### 11.2 `SICnINBUFH` (0x04 / 0x10 / 0x1C / 0x28 — 32-bit, RO)

| Bits | Field | Description | Reset |
|---|---|---|---|
| 31 | `ERRSTAT` | Last poll on this channel ended in error (0 = no error, 1 = error) | 0 |
| 30 | `ERRLATCH` | OR of the channel's latched `SISR` error bits (0 = none, 1 = an error is latched — read `SISR`) | 0 |
| 29:24 | `INPUT0` | First response byte — **6 bits**; the top two bits of the byte are assumed 0 and not stored | 0 |
| 23:16 | `INPUT1` | Second response byte | 0 |
| 15:8 | `INPUT2` | Third response byte | 0 |
| 7:0 | `INPUT3` | Fourth response byte | 0 |

Reading this register (its high halfword) sets the input-buffer **lock** (§4.2).

### 11.3 `SICnINBUFL` (0x08 / 0x14 / 0x20 / 0x2C — 32-bit, RO)

| Bits | Field | Description | Reset |
|---|---|---|---|
| 31:24 | `INPUT4` | Fifth response byte | 0 |
| 23:16 | `INPUT5` | Sixth response byte | 0 |
| 15:8 | `INPUT6` | Seventh response byte | 0 |
| 7:0 | `INPUT7` | Eighth response byte | 0 |

Reading this register (its low halfword) clears the input-buffer **lock** (§4.2).

### 11.4 `SIPOLL` (0x30 — 32-bit, RW)

| Bits | Field | Description | Reset |
|---|---|---|---|
| 25:16 | `X` | Poll interval (HSync lines between polls). Takes effect after `VSync`. `0x07` is the minimum | `0x07` |
| 15:8 | `Y` | Polls per frame. Takes effect after `VSync`. `0` ⇒ no polling | `0x00` |
| 7 | `EN0` | Enable polling of channel 0 | 0 |
| 6 | `EN1` | Enable polling of channel 1 | 0 |
| 5 | `EN2` | Enable polling of channel 2 | 0 |
| 4 | `EN3` | Enable polling of channel 3 | 0 |
| 3 | `VBCPY0` | Defer channel 0 output copy until `VSync` (shutter-glasses sync) | 0 |
| 2 | `VBCPY1` | Defer channel 1 output copy until `VSync` | 0 |
| 1 | `VBCPY2` | Defer channel 2 output copy until `VSync` | 0 |
| 0 | `VBCPY3` | Defer channel 3 output copy until `VSync` | 0 |
| 31:26 | — | Reserved | 0 |

### 11.5 `SICOMCSR` (0x34 — 32-bit, RW)

| Bits | Field | Description | Reset |
|---|---|---|---|
| 0 | `TSTART` | Write 1 to start a COM transfer; read = 1 while pending, 0 when complete | 0 |
| 2:1 | `CHANNEL` | Channel for the transfer (0–3) | 0 |
| 14:8 | `INLNGTH` | Response length in bytes; `0` ⇒ 128 bytes | 0 |
| 22:16 | `OUTLNGTH` | Command length in bytes; `0` ⇒ 128 bytes | 0 |
| 27 | `RDSTINTMSK` | Read-status interrupt mask (0 = masked, 1 = enabled) | 0 |
| 28 | `RDSTINT` | Read-status interrupt status/clear — set when any `SISR[RDSTn]` is set; cleared when all clear | 0 |
| 29 | `COMERR` | Read-only; 1 if the last COM transfer errored | 0 |
| 30 | `TCINTMSK` | Transfer-complete interrupt mask (0 = masked, 1 = enabled) | 0 |
| 31 | `TCINT` | Transfer-complete interrupt status — write 1 to clear | 0 |
| 26:23,15 | — | Reserved | 0 |

The **low** halfword (bits 0–15) holds `TSTART`, `CHANNEL` and `INLNGTH`; the
**high** halfword (bits 16–31) holds `OUTLNGTH` and the interrupt/mask bits.

### 11.6 `SISR` (0x38 — 32-bit, RW)

Per-channel groups; channel 0 is at the **high** side, channel 3 at the **low**
side. Within a channel, the group order from the LSB is `UNRUN, OVRUN, COLL,
NOREP, WRST, RDST`.

| Bits | Field | Description | Reset |
|---|---|---|---|
| 31 | `WR` | Global write — write 1 to copy all output buffers; reads 1 until all copied | 0 |
| 29 | `RDST0` | Read status channel 0 (new input data unread) | 0 |
| 28 | `WRST0` | Write status channel 0 (output shadow not copied) | 0 |
| 27 | `NOREP0` | No-response error channel 0 (write 1 to clear) | 0 |
| 26 | `COLL0` | Collision error channel 0 (write 1 to clear) | 0 |
| 25 | `OVRUN0` | Overrun error channel 0 (write 1 to clear) | 0 |
| 24 | `UNRUN0` | Underrun error channel 0 (write 1 to clear) | 0 |
| 21 | `RDST1` | Read status channel 1 | 0 |
| 20 | `WRST1` | Write status channel 1 | 0 |
| 19 | `NOREP1` | No-response channel 1 (write 1 to clear) | 0 |
| 18 | `COLL1` | Collision channel 1 (write 1 to clear) | 0 |
| 17 | `OVRUN1` | Overrun channel 1 (write 1 to clear) | 0 |
| 16 | `UNRUN1` | Underrun channel 1 (write 1 to clear) | 0 |
| 13 | `RDST2` | Read status channel 2 | 0 |
| 12 | `WRST2` | Write status channel 2 | 0 |
| 11 | `NOREP2` | No-response channel 2 (write 1 to clear) | 0 |
| 10 | `COLL2` | Collision channel 2 (write 1 to clear) | 0 |
| 9 | `OVRUN2` | Overrun channel 2 (write 1 to clear) | 0 |
| 8 | `UNRUN2` | Underrun channel 2 (write 1 to clear) | 0 |
| 5 | `RDST3` | Read status channel 3 | 0 |
| 4 | `WRST3` | Write status channel 3 | 0 |
| 3 | `NOREP3` | No-response channel 3 (write 1 to clear) | 0 |
| 2 | `COLL3` | Collision channel 3 (write 1 to clear) | 0 |
| 1 | `OVRUN3` | Overrun channel 3 (write 1 to clear) | 0 |
| 0 | `UNRUN3` | Underrun channel 3 (write 1 to clear) | 0 |
| 30, 23:22, 15:14, 7:6 | — | Reserved | 0 |

### 11.7 `SIEXILK` (0x3C — 32-bit, RW)

| Bits | Field | Description | Reset |
|---|---|---|---|
| 31 | `LOCK` | 0 = EXI clock unlocked (32 MHz allowed); 1 = EXI 32 MHz clock **locked out** | 1 |
| 30:0 | — | Reserved | 0 |

`LOCK` drives the `Lock32MHz` output that prevents the EXI clock from being set to
32 MHz. It resets to **locked** (`=1`), i.e. bit 31 set, so on a cold start the
32 MHz EXI clock is disallowed until software clears it. (`HW/IO/si.htm` shows the
reset in a form that can be read as `0x1`; the RTL and FDL put `LOCK` at bit 31 and
reset it to the *locked* state, so the register value at reset is `0x80000000`.)
The SI shares the EXI/IO clock tree, which is why locking the EXI 32 MHz setting
matters to the SI as well.

### 11.8 `SIRAM` (offset `0x80`, 128 bytes)

Not a normal register. Organised as 64 × 16-bit words, big-endian machine words.
CPU access is 16-bit only, byte offset `0x80 + 2*i` for `i = 0..63`. During a COM
transfer the SI owns it. Reset value undefined (SRAM contents). See §6.2 for access
rules and the read-ready handshake (§9.1).

## 12. Emulator notes

1. **Model four symmetric, independent channels.** Do not share state between channels.
   Each channel has its own output buffer, input buffer, error bits and its own
   transfer state machine. Only the poll counter (`SIPOLL`) is shared.
2. **Two transfer modes per channel.** A **poll** is a fixed 3-byte output
   (`CMD`,`OUTPUT0`,`OUTPUT1`) followed by an 8-byte input. A **COM** transfer uses
   `OUTLNGTH`/`INLNGTH` (modulo 128, see §6.3) and moves data through `SIRAM`. The
   SM never mixes the two: a poll ignores the length registers, and a COM transfer
   does not touch the port input/output buffers.
3. **Length is modulo 128.** Model the transfer byte counter as a 7-bit counter with
   a length field; a length of 0 yields a **128-byte** transfer (the counter wraps
   to 0 and matches). Do not model a 0-byte transfer — it is not expressible.
4. **Framing.** Emit: start bit, 8 data bits **MSB-first**, stop bit, then an end
   pulse, then a separation gap. Demodulate the response by integrating the high
   samples per bit cell and thresholding (≈4 of the window). Since the SI drives its
   output **inverted**, invert the emitted value on the wire before the controller
   sees it, and put the inverted value back into your poll integrator.
5. **First response byte is 6 bits.** Capture only 6 bits into `INPUT0`; the top two
   bits of `INPUT0` are always `0`. The other 7 input bytes are full 8-bit.
6. **Double-buffering and the lock/unlock sequence.** Keep two sets of buffers:
   the CPU-visible registers and the wire-side shadows. Copy output on `SISR[WR]`,
   defer to `VSync` if `VBCPYn` is set, and suppress the copy while an output
   transfer is active on that channel (except COM). Copy input to the visible
   buffer when a poll completes, but only when the input buffer is **not** locked;
   **lock** on reading `SICnINBUFH` and **unlock** on reading `SICnINBUFL`. This is
   essential for bare correctness — without it a faster poll can overwrite a staler
   read.
7. **Poll scheduling.** Anchor polling to `VSync`. Each `VSync`, capture `X`/`Y`
   and, for each `ENn` channel, poll at `VSync` and then every `X` HSync lines until
   `Y` polls have happened (stop early if `Y = 0`). Disabling `ENn` stops polling
   after the current transaction. After a COM transfer, resume polling at the next
   `VSync` if `ENn` is set.
8. **Interrupts.** Only two events: `RDSTINT` (any channel's input buffer has new
   unread data) and `TCINT` (COM transfer complete). The line is
   `(TCINT&TCINTMSK) || (RDSTINT&RDSTINTMSK)`. `RDSTINT` is cleared implicitly when
   all `RDSTn` clear; `TCINT` is write-1-to-clear. Route this to the PI as `SIINT`
   (PI `INTSR` bit 3 / `INTMSK` `SIMSK`). Per-channel error bits and `COMERR` do not
   raise the interrupt on their own.
9. **Errors.** Set `NOREP` after the no-response time-out (no answer → likely no
   controller), `OVRUN` if a data bit appears in the overrun window, `UNRUN` if the
   end-of-response never arrives, and `COLL` if the line disagrees with the value
   being driven. Latch them; clear each on a write-1 to its bit. `NOREP` is the
   convenient "controller absent" test.
10. **SI RAM handshake and ownership.** Return the SI/`SIRAM` read with a one-cycle
    ready (or model `SiRamRdy`). While a COM transfer runs, the SI owns the RAM:
    CPU writes are dropped and CPU reads return undefined data. Model `SIRAM` as 64
    16-bit words (128 bytes).
11. **Register width.** `SIRAM` and the genuine registers are reached via the 16-bit
    high-word-first model; emulate a 2-byte or 4-byte access only. Keep `SICnINBUFH`
    to 6 bits for `INPUT0`.

## 13. References

- `HW/IO/si.htm` — register-level summary and the controller-protocol text
  (patent-derived); the bit positions in §11 are cross-checked against it.
- `specs/architecture/peripherals.md` — the higher-level EXI/SI and controller
  overview. This document is the register-accurate companion; where they differ
  the RTL wins and the differences are noted here (§9.1, §10).
- `HW/Flipper_ASIC_Block_Diagram.png` and `HW/IO/ProcessorInterface.md` — the SI
  within Flipper and the `SIINT`/`SIMSK` PI interrupt.
- `RE/OS/osexi.txt` — the OS/EXI driver notes for the EXI clock lock that
  `SIEXILK` controls.
- **US Patent 6,609,977** (External interfaces) — the SI register and
  controller-interface text used by `HW/IO/si.htm`.
- **US Patent 6,811,489** (Serial Interface) — the controller command/response and
  duty-cycle modulation description.
- Internal SI interface specification and SI-emulation notes (an internal
  ATI-oriented description of an SI-compatible controller interface) — used only
  for cross-checking the framing and the controller protocol; not cited here as a
  public path.
- Internal Flipper register-definition (FDL) and `io_Si` RTL — the authoritative
  source for the bit positions, sub-block structure and register behaviour
  summarised above.

### Note on the source of §11 bit positions

The bit positions in §11 were cross-checked three ways: the FDL register
definitions (LSB-first field order), the `io_Si.v` read datapath and the `io_*`
control registers, and the patent-derived `HW/IO/si.htm` table. All three agree.
The high-level [`peripherals.md`](peripherals.md) presents the SI as ordinary
32-bit registers; the accurate model (used here) is the 16-bit high-word-first IO
access interface, with `SIRAM` as a separate 128-byte RAM block that has its own
decode and its own ready handshake rather than being a normal status register.
