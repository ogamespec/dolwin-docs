# GameCube — Expansion Interface (EXI)

> **EXI** (the **Expansion Interface**) is the general-purpose peripheral bus master
> inside the **Flipper** ASIC's IO block. Flipper exposes **three** independent EXI
> channels, each a serial, full-duplex, byte-oriented bus with its own clock and its
> own set of active-low chip selects. EXI is how the console talks to the on-chip
> boot ROM, the flash/RTC, the memory-card slots, the modem, the Broadband Adapter
> and a range of external development devices. The software model is a small set of
> registers per channel that drive a **transfer** (immediate or DMA) over the
> selected chip select at a selected clock rate.

This is the register-level, emulator-focused specification for the **EXI block**
itself. The facts are drawn from the repository's hardware documentation, the
Flipper EXI RTL, the register-definition (FDL) files and the system-library
reverse-engineering notes, and are summarised (paraphrased) rather than reproduced
verbatim. Where a behaviour is only implicit in the RTL — a lock, a timing edge,
the exact meaning of a field — it is called out as such. A high-level overview of
EXI and SI (the sibling **Serial Interface**) also lives in
[peripherals.md](peripherals.md); this document is the register-accurate one and
supersedes the register table there.

## 1. Overview

The EXI block is instantiated in Flipper as `io_Exi` (reached through the IO
register space, IO-module `io_addr_exi`). It contains three channel sub-instances,
`io_Exi0Ch`, `io_Exi1Ch` and `io_Exi2Ch`, each of which is built from the same set
of sub-blocks: a control/status register (**CSR** or **CPR**), a serial transfer
engine (**Xfer**), a DMA engine (**Dma**), a chip-select/clock generator (**Clk**),
and a master state machine (**SM**) that decides between the three transfer types —
immediate, DMA, and (for channel 0) the boot-ROM read. Channel 0 additionally
instantiates the ROM state machine (**ROM**) and the key/descrambler state machine
(**Key** / **LFSR**).

It is one of the four peripheral sub-blocks of the Flipper **I/O (`io`) module**
(the others being DI, SI and AI). The `io` block also contains the 16-bit register
interface (`io_Pi`, which decodes the `0x0C006000`–`0x0C006C00` module bases) and a
shared main-memory port (`io_Mem`) with a **round-robin IO-DMA arbiter** that the
EXI's three DMA engines contend for (along with the DI); each peripheral sub-block
has its own interrupt (`exi_piInt` here) that PI aggregates.

```
io  (I/O module)
├── io_Pi    — register/CPU interface (16-bit PI path, module decode)
├── io_Mem   — shared main-memory port + round-robin IO-DMA arbiter
├── io_di    — DI  (disk-drive command transport)
├── io_Si    — SI  (serial / controller interface)
├── io_Exi   — EXI (expansion, 3 channels)                  « this block »
├── io_Ai_top— AI  (audio interface)
└── io_TstMux— test/scan mux and pad output-enable control
```

### 1.1 Channels and their role

| Channel | Base | Typical devices |
|---|---|---|
| **EXI0** | `0x0C006800` | Boot ROM / flash ROM, real-time clock + settings SRAM, and the high-speed external device (modem, Broadband Adapter) on the three chip selects |
| **EXI1** | `0x0C006814` | External serial-port device (modem etc.) — one chip select |
| **EXI2** | `0x0C006828` | External debug/serial device (serial port 2) — one chip select |

Channel 0 carries the most features: it is the **16-bit / highest-bandwidth**
channel, it owns the boot-ROM **descrambler**, and it drives an *isolated internal*
32 MHz line as an EMI countermeasure. Channel 0 is the channel used by the Broadband
Adapter and the modem. Channels 1 and 2 are plain external channels with fewer
registers (they have no ROM and, for channel 2, no device-presence EXT_IN).

### 1.2 Data flow

Each channel connects to main memory through the memory interface (a 32-byte DMA
path) and to a single serial device through a small set of pads. The transfer state
machine picks one of three activities per channel: an **immediate** 1–4 byte
full-duplex transfer, a **DMA** bulk transfer staged through a 32-byte internal
FIFO, or (channel 0 only) a **boot-ROM** read.

## 2. External pins / signals

Each channel has a small, dedicated pad set. The names below use the RTL
convention (the external-facing signal is `exi<n>…`; the pad group for channel 0 is
the wide one, channels 1 and 2 are single-CS/single-clock slices).

### 2.1 Channel 0 (wide)

| Signal | Width | Direction | Description |
|---|---|---|---|
| `exi0di0` / `exi0di1` | 1 | in | Serial data **in**, two alternate input lanes (the input is muxed by `cs0b`) |
| `exi0do0` / `exi0do1` | 1 | out | Serial data **out**, two copies (internal and OE-gated external) |
| `exi0do1oe` | 1 | out | Output-enable for the external `exi0do1` lane |
| `exi0clk0` / `exi0clk1` | 1 | out | EXI clock, two copies (internal always-driven `clk0`, and OE-gated external `clk1`) |
| `exi0clk1oe` | 1 | out | Output-enable for the external clock lane |
| `exi0csb[2:0]` | 3 | out | Chip select (active-low); `[0]`=external/EXT, `[1]`=ROM, `[2]`=modem/high-speed |
| `exi0csboe[2:0]` | 3 | out | Chip-select output-enables; `[0]` is OE-gated, `[1]`/`[2]` are always enabled |
| `exi0extin` | 1 | in | Device-present / EXT_IN interrupt input |
| `exi0intb` | 1 | in | Device interrupt request input |

The serial clock (`exi0clk1`) and data-out (`exi0do1`) and the external chip-select
(`exi0csboe[0]`) output-enables are gated by `(~EXTINT && exiextin)` — the outputs
are only driven when a device is present on the line and no removal interrupt is
pending. The internal `exi0clk0` / `exi0do0` lanes and the ROM (`csb[1]`) and modem
(`csb[2]`) chip selects are always enabled.

### 2.2 Channels 1 and 2 (narrow)

| Signal | Width | Direction | Channel | Description |
|---|---|---|---|---|
| `exi1di` | 1 | in | 1 | Serial data in |
| `exi1do` | 1 | out | 1 | Serial data out |
| `exi1dooe` | 1 | out | 1 | Data output-enable |
| `exi1clk` | 1 | out | 1 | EXI clock |
| `exi1clkoe` | 1 | out | 1 | Clock output-enable |
| `exi1csb` | 1 | out | 1 | Chip select (active-low) |
| `exi1csboe` | 1 | out | 1 | Chip-select output-enable |
| `exi1extin` | 1 | in | 1 | Device-present / EXT_IN |
| `exi1intb` | 1 | in | 1 | Device interrupt request |
| `exi2di` | 1 | in | 2 | Serial data in |
| `exi2do` | 1 | out | 2 | Serial data out |
| `exi2clk` | 1 | out | 2 | EXI clock |
| `exi2csb` | 1 | out | 2 | Chip select (active-low) |
| `exi2intb` | 1 | in | 2 | Device interrupt request |

Channel 2 has **no** `extin` (no device-presence/EXT_IN); channel 1 does. Both
channels 1 and 2 have exactly one chip select and no ROM path.

The output-enable logic for channels 1/2 mirrors channel 0: `exiclkoe`, `exidooe`
and `exicsboe` are driven as `(~EXTINT && exiextin)` for the channel's EXT_IN (for
channel 2 there is no EXT_IN, so the OE is effectively tied to the presence logic).

## 3. Chip-selects & devices

Each chip-select on a channel selects a single attached device. Exactly one chip
select can be active at a time per channel — the CSR hardware enforces mutual
exclusion among `cs0b`/`cs1b`/`cs2b`. The register bit is active-**high** in the
register (a `1` selects the device), but the physical `exicsb` line is
active-low (`exicsb[n] = ~csnB`).

### 3.1 Channel 0 chip selects

| CS register bit | Line | Device |
|---|---|---|
| `cs0b` (bit 7) | `exi0csb[0]` | External high-speed device (modem, Broadband Adapter) — the "EXT" line, presence-detected via `exi0extin` |
| `cs1b` (bit 8) | `exi0csb[1]` | On-chip boot **ROM** (and the flash/RTC/SRAM device) |
| `cs2b` (bit 9) | `exi0csb[2]` | Modem / high-speed device (the "MDM" line) |

The input lane mux is `exi0di = cs0b ? exi0di1 : exi0di0`, so the two input lanes
are selected by the external chip-select state. On the boot-ROM path (`io_ExiROM`)
the ROM chip-select (`cs1b`) is driven directly by the ROM state machine, which
asserts it (`SetROMCS`) and for a ROM read also **forces the clock to 32 MHz** and
clears it back to 1 MHz afterwards (`ClrROMCS`).

### 3.2 Channel 1

A single chip select (`cs0b` = bit 7 → `exi1csb`), used for an external device on
serial port 1. It has an EXT_IN so the OS can detect device insertion/removal.

### 3.3 Channel 2

A single chip select (`cs0b` = bit 7 → `exi2csb`), used for the external device on
serial port 2. It has no EXT_IN, so there is no insertion/removal detection — the
OS uses a probe handshake (see below).

### 3.4 The "probe" handshake

Because there is no universal presence signal, the OS driver probes a device by
selecting it, performing a short immediate transfer, and checking the result. The
reverse-engineered system library (`EXIProbe` / `__EXIProbe`) shows this: it
selects device 0, reads the device ID register via a 1-cycle immediate transfer,
and uses a lock flag + a `0x800030c0+chan*4` state word to remember whether the
channel has a device. Channels 0 and 1 use EXT_IN to short-circuit the probe
(when a device is known present by hardware); channel 2 always relies on the probe.

## 4. Transfer modes

A transfer is started by writing the **transfer-control** register (`EXInCR`): set
`tstart=1` (bit 0), choose `dma` (bit 1) and `rw` (bits 3:2), and (for immediate
transfers) the `tlen` (bits 5:4) length. The master state machine (`io_ExiSM`)
selects the mode:

| Condition | Mode |
|---|---|
| `tstart && dma` | **DMA** transfer |
| `tstart && !dma` | **Immediate** transfer |
| boot-ROM read (`PiExiROM` with a cache miss) | **ROM** read (channel 0 only) |

Each mode has its own busy flag; the channel's master `ExiBusy` is the OR of them.
While a transfer is in progress, the address/length/control register loads are
suppressed (`!ExiBusy` gate); the chip-clock register load is also gated by
`!ExiBusy`.

### 4.1 Immediate transfer (1–4 bytes, full duplex)

The immediate transfer is the command/status path. It shifts **up to 4 bytes** out
of the `EXInDATA` register and simultaneously shifts the same number of received
bytes back in. The length is given by `tlen`:

| `tlen[1:0]` | Length |
|---|---|
| `00` | 1 byte |
| `01` | 2 bytes |
| `10` | 3 bytes |
| `11` | 4 bytes |

The data register is **bit 63..32** of the internal 64-bit datapath (`ExiData[63:48]`
high halfword, `ExiData[47:32]` low halfword). Transmission is **big-endian**: the
serial output is `ExiData[63]`, and each byte is shifted out MSB-first. On
completion the received bytes have been shifted into the same register, and the OS
reads `EXInDATA` back and unpacks them (the reverse-engineered driver reads the
register and extracts each byte with `(d >> ((3-i)*8)) & 0xFF`).

For an immediate **write** the OS writes the source bytes into `EXInDATA` first,
then the control register. For an immediate **read** the OS writes the control
register and reads `EXInDATA` after the transfer completes. The `rw[1:0]` direction
field (0 = read, 1 = write, 2 = read/write) selects the sense.

### 4.2 DMA transfer

The DMA transfer moves a block between **main memory** and the EXI device, staged
through a **32-byte** (4×64-bit) internal FIFO. The `EXInMAR` register gives the
32-byte-aligned memory address and `EXInLEN` gives the length; both are auto-
advanced as the DMA proceeds. See §5.

### 4.3 ROM read (channel 0 only)

A CPU read of the boot-ROM region is served by the channel-0 ROM state machine and
the descrambler, not by a normal EXI device transfer. See §7.

## 5. DMA

### 5.1 Registers

- **`EXInMAR`** (field `[25:5]`) — the 32-byte-aligned memory address. Bits 25:16 are
  in the high halfword, bits 15:5 in the low halfword (the low 5 bits read as 0).
- **`EXInLEN`** (field `[25:5]`) — the transfer length. The engine transfers data in
  **32-byte lines** and decrements the length as lines complete, incrementing `MAR`
  by 32 bytes per line.

### 5.2 The 32-byte datapath

The DMA engine is a small state machine that fills/drains a 4-entry × 64-bit FIFO
and issues 32-byte memory transactions. For an **EXI→memory** DMA (a "read" of the
device), it performs four 8-byte serial transfers into the FIFO, then commits the
32 bytes to main memory in one burst, repeating until `EXILEN` is exhausted. For a
**memory→EXI** DMA (a "write" to the device), it reads 32 bytes from main memory
into the FIFO, then emits four 8-byte serial transfers to the device.

### 5.3 Flush / completion

For a **read** DMA (EXI→memory), once the last line is written the engine asserts
the memory write-flush and waits for the flush-acknowledge, guaranteeing that the
final data is in main memory rather than sitting in a write buffer. Only then does
it signal completion. For a **write** DMA (memory→EXI) no flush is needed, since the
data came from memory.

### 5.4 Completion

When the DMA engine finishes (and the master state machine observes the DMA-busy
flag drop), it **sets the transfer-complete interrupt** (`tcint`) and **clears
`tstart`** (bit 0 of `EXInCR`). The OS waits on `tstart` going to 0 (`EXISync`
polls `control & 1`).

## 6. Clocking & the EXI clock lock

### 6.1 The CLK field

Bits **6:4** of the CSR (`clk[2:0]`) select the per-channel EXI clock. The encoding
is **monotonic** — the field value increases with the clock rate — and is confirmed
against the RTL clock generator, which divides the EXI clock by progressively
smaller factors:

| `clk[2:0]` | EXI clock |
|---|---|
| `000` | 1 MHz |
| `001` | 2 MHz |
| `010` | 4 MHz |
| `011` | 8 MHz |
| `100` | 16 MHz |
| `101` | 32 MHz |
| `110` / `111` | reserved (clock generator defaults to 1 MHz) |

This is *not* inverted: value 0 = 1 MHz (largest divider) through value 5 = 32 MHz
(smallest divider). The system library confirms it — `EXISelect` writes
`freq << 4` with `freq` in `{0..5}` for the six rates. The clock is enabled only
while a transfer is in progress (`ExiClkEnable == XferBusy`), so the line idles
low between transfers.

### 6.2 The 32 MHz lock (`SIEXILK`)

The **SI** block's `SIEXILK` register (bit 0, write any value to the *unused* upper
31 bits) locks out the **32 MHz** setting. When `SIEXILK=1`, the EXI CSR ignores any
write to `clk` whose value is 32 MHz (`EXI_CLK_32MHZ`), so software cannot set a
channel to 32 MHz while the lock is held. The serial interface shares the same
clock tree as EXI, which is why the SI block owns this. (From per the SI register
documentation: `0` = EXI clocks unlocked, 32 MHz permitted; `1` = locked, 32 MHz not
permitted.)

### 6.3 Channel-0 dual clock / isolation

Channel 0 drives **two** copies of the EXI clock: an internal `exi0clk0` that is
always driven, and an external `exi0clk1` whose output-enable is gated by device
presence. This is the electro-magnetic-interference countermeasure described at the
block level: an isolated internal 32 MHz line is kept separate from the (slower,
OE-gated) external line. Both copies carry the same `ExiClk` waveform.

## 7. Boot ROM & descrambler

The boot ROM is exposed as an EXI**0** device on chip-select `cs1b`. When the CPU
reads the boot-ROM region, the channel-0 ROM state machine (`io_ExiROM`) takes over
and performs the read on the EXI0 serial bus, then un-scrambles the data before
returning it to the Processor Interface.

### 7.1 ROM read path

- The ROM access is addressed by the PI address bits `[19:3]` (17 bits). The engine
  holds a small **cache** of one 8-byte line, tagged with `PiAddr[19:3]`.
- On a cache hit (`ExiPiROMRdy`), the ROM access is served immediately and no
  serial transfer is started.
- On a miss, the state machine asserts the ROM chip-select (`cs1b`), waits the
  `cs0`→`clk↑` address-settle delay, sends a **4-byte command** on the serial bus
  (a 32-bit ROM-read opcode + address), then reads **8 bytes** back, de-scrambles
  the inbound stream with the key, caches the line, de-asserts the chip-select and
  signals ready.
- The ROM command is a 32-bit field: 6-bit read opcode, 17-bit address, and 9 dummy
  bits (one `0` for the address bit 2 slot plus 8 dummy data bits). The command
  uses a 4-byte (`tlen=3`) transfer; the data uses an 8-byte transfer.

The ROM state machine also controls the chip-select/clock through the CSR: asserting
the ROM chip-select forces `cs1b=1` and `clk=32 MHz`, and clearing it returns both
to the deselected/1 MHz defaults.

### 7.2 ROM disable (`romdis`)

Bit **13** of the channel-0 CSR (`romdis`) disables the boot ROM and descrambler.
When `romdis=1`:

- ROM reads return `0xFFFF` for every halfword (`ExiROMData = 16'hFFFF`);
- `ExiPiROMRdy` is forced high (the ROM access is always "ready" and never starts a
  serial transfer);
- the descrambler key is forced to the identity value (`Key=0`).

The reset value of `romdis` is **0** (ROM enabled), so the boot path works at power
on. The CSR only allows *disabling* the ROM (writing a `1`); writing `0` is ignored
and the ROM cannot be re-enabled once disabled. This matches the OS behaviour, which
disables the ROM after boot.

### 7.3 The descrambler

The boot ROM is encrypted/descrambled in hardware. For channel 0, the inbound serial
bit stream is XORed with a running **key** bit. The key is generated by a 3-stage
**16-bit LFSR** cascade (each stage uses the "one-to-many" Galois form with a fixed
tap polynomial and a fixed seed). The LFSRs step once per 8-byte transfer on the
**negative** edge of the EXI clock, and only when the transfer is a ROM read or a DMA
(8-byte length) and the ROM is not disabled. The three LFSR outputs are XOR-cascaded
to form the key; the key is `0` (identity — no descrambling) when either `romdis` is
set or the `KeyEnable` "chicken bit" is clear. The seed and tap constants are fixed
in the RTL and are what an emulator must implement to reproduce the boot-ROM stream
exactly. Channels 1 and 2 use the identity key (`Key=0`) and have no ROM path.

## 8. Interrupts & device presence

### 8.1 Per-channel interrupt sources

Each channel has three interrupt **status** bits and three **mask** bits in its CSR.
A status bit is set by hardware and cleared by writing a `1` to it (write-1-to-clear);
a mask bit is a plain read/write load. The per-channel interrupt line is the
masked OR of the three sources:

| CSR bit | Status field | Mask field | Source |
|---|---|---|---|
| 0 / 1 | `exiint` | `exiintmsk` | Device **interrupt** — a falling edge on the device's `intb` line |
| 2 / 3 | `tcint` | `tcintmsk` | **Transfer complete** — set when an immediate or DMA transfer finishes |
| 10 / 11 | `extint` | `extintmsk` | **Device presence** — a falling edge on the `extin` line (device removed) |

`tcint` is set by the transfer state machine when a DMA or immediate transfer
completes. `exiint` and `extint` are edge-latched by the CSR from the synchronised
`intb` / `extin` lines (a falling edge sets the bit). Writing a `1` to the status
bit clears it; the mask bit gates whether it contributes to the channel interrupt.

### 8.2 Per-channel OR and the PI `EXINT`

Each channel's interrupt line is

```
ExiInt = (extint && extintmsk) || (tcint && tcintmsk) || (exiint && exiintmsk)
```

All three channels' lines are OR-ed together into a single `exi_piInt` signal, which
is registered and presented to the **PI** interrupt controller as its `EXINT` source.
In the PI cause/mask registers (`INTSR` / `INTMSK`) this is **bit 4** (`EXINT` /
`EXMSK`). The PI is then the single line that reaches the CPU — software must read
the per-channel CSR to determine which channel and which source fired.

### 8.3 Which channels have which source

| Channel | `exiint` | `tcint` | `extint` / `ext` |
|---|---|---|---|
| EXI0 | yes | yes | yes (with `ext` level) |
| EXI1 | yes | yes | yes (with `ext` level) |
| EXI2 | yes | yes | **no** (no `extin`) |

Channel 0 also has an `ext` (bit 12) status that mirrors the synchronised `extin`
level (a `1` = device present), used by the probe logic. Channel 1 has `ext`, the
`ext` level and its `extint`; channel 2 has neither `ext` nor `extint`.

### 8.4 Device insertion / removal

The `extin` line detects device presence. The CSR synchronises it, stores the level
in `ext`, and sets `extint` on the falling edge (device removal). Because `extin`
and `intb` are asynchronous to the EXI clock, they are run through a synchroniser
and the edge detector requires a settling delay before looking for the falling edge
— a device being removed while a transfer is in flight will not immediately produce
a spurious interrupt.

## 9. Register access

All Flipper IO registers, including the EXI block, are reached through the
Processor Interface's **16-bit** register bus (`PiData[15:0]`, halfword-granular
`PiAddr[19:1]`). There is no 32-bit register port; PI always breaks a 32-bit access
into two 16-bit transfers and is **big-endian**: the **high** halfword is at the
base (even) address and the **low** halfword at `base+2`. The register decode uses
`PiAddr[5:2]` to select the register index within a channel, and `PiAddr[1]` to
select the halfword. A register is 32-bit internally; only 2-byte and 4-byte
accesses are valid in the register space (any other size is a PI error).

For the EXI registers, only specific halfwords are loaded on write and specific
halfwords are meaningful on read (the CSR and transfer-control register are
essentially the low halfword; the address/length/data registers use both halves).

## 10. Register map

Channel n base = `0x0C006800 + n*0x14` (uncached alias `0xCC006800 + n*0x14`):
EXI0 `0x0C006800`, EXI1 `0x0C006814`, EXI2 `0x0C006828`. Each channel has the same
five registers at the same offsets. All are **32-bit** internally; the table shows
the byte offset from the channel base.

| Offset | Name | R/W | Purpose |
|---|---|---|---|
| `0x00` | `EXInCPR` | R/W | Control/status: interrupt flags + masks, clock select, chip select, EXT, ROM-disable |
| `0x04` | `EXInMAR` | R/W | DMA memory address (field `[25:5]`) |
| `0x08` | `EXInLEN` | R/W | DMA transfer length (field `[25:5]`) |
| `0x0C` | `EXInCR` | R/W | Transfer control: `tstart`, `dma`, `rw`, `tlen` |
| `0x10` | `EXInDATA` | R/W | Immediate data (bits 63:32 of the 64-bit datapath) |

> **Note — register order.** The existing high-level overview
> [peripherals.md](peripherals.md) lists the EXI registers in a different order
> (`STAT`, `DATA`, `CTRL`, `MAR`, `LEN`). That is **incorrect**. The RTL and the
> system-library driver both use the order above: **CPR / MAR / LEN / CR / DATA**
> at `+0x00 / +0x04 / +0x08 / +0x0C / +0x10`. The RTL wins; the register-address
> layout is `CPR, MAR, LENGTH, CR, DATA`, and the immediate data register is at
> **`+0x10`**, not `+0x04`.

Register indices used by the RTL/OS: EXI0 = indices 0–4, EXI1 = 5–9, EXI2 = 10–14
(`EXI1CPR` is at index 5, `EXI2CPR` at index 10), so channel n base = `n*5` registers.

## 11. Register fields

### 11.1 `EXInCPR` (control/status, `+0x00`)

Reads back the low halfword as `{2'b0, ExiCpr[13:0]}`; the high halfword reads as
`0`. Bits are LSB-first.

**All channels (EXI0/EXI1/EXI2):**

| Bit | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 0 | `exiintmsk` | R/W | `0` (masked) | Enable the device-interrupt source |
| 1 | `exiint` | R/W1C | `0` | Device interrupt status (set on `intb` falling edge) |
| 2 | `tcintmsk` | R/W | `0` (masked) | Enable the transfer-complete source |
| 3 | `tcint` | R/W1C | `0` | Transfer-complete status |
| 6:4 | `clk` | R/W | `0` (1 MHz) | EXI clock select (see §6.1); 32 MHz honoured only if not `SIEXILK`-locked |
| 7 | `cs0b` | R/W | `0` (deselected) | Chip select 0 (active-high in register, active-low on the pin) |

**EXI0 only:**

| Bit | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 8 | `cs1b` | R/W | `0` | Chip select 1 (ROM) |
| 9 | `cs2b` | R/W | `0` | Chip select 2 (modem / high-speed) |
| 10 | `extintmsk` | R/W | `0` (masked) | Enable the device-presence source |
| 11 | `extint` | R/W1C | `1` | Device-presence interrupt status (set on `extin` falling edge; resets to pending-but-masked) |
| 12 | `ext` | RO | synced `extin` | Current device-present level (1 = present) |
| 13 | `romdis` | R/W | `0` (ROM enabled) | Disable the boot ROM & descrambler |

**EXI1 only:**

| Bit | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 8:9 | — | — | `0` | Reserved (pad) |
| 10 | `extintmsk` | R/W | `0` (masked) | Enable the device-presence source |
| 11 | `extint` | R/W1C | `1` | Device-presence interrupt status |
| 12 | `ext` | RO | synced `extin` | Current device-present level |
| 13 | — | — | `0` | Reserved (pad — no ROM) |

**EXI2 only:**

| Bit | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 13:8 | — | — | `0` | Reserved (pad — no `cs1b`/`cs2b`, no EXT, no ROM) |

Notes on the CSR semantics (from the RTL):

- **`extint` resets to 1** (pending) while its mask `extintmsk` resets to 0 (masked),
  so no interrupt is delivered at power-on; software clears it during initialisation.
- The chip selects are **mutually exclusive** — the CSR refuses to set `csnb` if any
  other chip select is (or is being set at the same time).
- The `clk` load is suppressed while the channel is busy and while the 32 MHz write
  is locked; the ROM state machine forces `clk=32 MHz` during a ROM read.
- `romdis` is write-only-to-disable: writing `1` disables the ROM, writing `0` is
  ignored.

### 11.2 `EXInMAR` (DMA address, `+0x04`)

Field `[25:5]` = 32-byte-aligned memory address. Read high halfword `= {6'b0,
MAR[25:16]}`, low halfword `= {MAR[15:5], 5'b0}`. Written high halfword `= MAR[25:16]`
from `PiData[9:0]`, low halfword `= MAR[15:5]` from `PiData[15:5]`. Bounds `DmaBusy`
(the value is not loaded while a DMA is running). Reset `0`.

| Bits | Field | Description | Reset |
|---|---|---|---|
| 25:5 | `MAR` | 32-byte-aligned DMA memory address | `0` |
| 4:0, 31:26 | — | Reserved (zero) | `0` |

### 11.3 `EXInLEN` (DMA length, `+0x08`)

Field `[25:5]`. Read/written the same split as `EXInMAR`. The engine counts down the
length once per 32-byte line and increments `MAR` by 32 per line. Bounds `DmaBusy`.
Reset `0`.

| Bits | Field | Description | Reset |
|---|---|---|---|
| 25:5 | `LEN` | DMA length (decremented per 32-byte line) | `0` |
| 4:0, 31:26 | — | Reserved (zero) | `0` |

### 11.4 `EXInCR` (transfer control, `+0x0C`)

Reads back the low halfword as `{10'b0, CR[5:0]}`; the high halfword reads as `0`.
Bits are LSB-first.

| Bit | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 0 | `tstart` | R/W | `0` (done) | Write `1` to start a transfer; hardware clears it on completion |
| 1 | `dma` | R/W | `0` (immediate) | 0 = immediate transfer, 1 = DMA transfer |
| 3:2 | `rw` | R/W | `0` (read) | 0 = read, 1 = write, 2 = read-write |
| 5:4 | `tlen` | R/W | `0` (1 byte) | Immediate transfer length: 0=1B, 1=2B, 2=3B, 3=4B |

`tstart` is started by writing `1` and cleared automatically when the transfer
completes (immediate or DMA). `tlen` only affects immediate transfers (DMA always
moves 8 bytes at a time). The `rw` direction for a DMA is read/write; the
read-write (`2`) sense is only meaningful for immediate transfers.

### 11.5 `EXInDATA` (immediate data, `+0x10`)

Maps to the **upper 32 bits** of the internal 64-bit datapath: high halfword =
`ExiData[63:48]`, low halfword = `ExiData[47:32]`. Reset `0`. During a transfer the
register shifts (a complete transfer shifts out and shifts in the full selected
byte count); after an immediate read the received bytes are in this register. For a
DMA or ROM transfer the full 64-bit datapath carries 8 bytes.

## 12. Emulator notes

1. **Register width / word order.** Model each EXI register as a 32-bit value
   reached via two 16-bit halfwords, big-endian (high at the base, low at `+2`).
   The CSR (`EXInCPR`) and `EXInCR` are written/read as the low halfword only; the
   address/length/data registers use both halves. Only 2- and 4-byte accesses are
   valid.
2. **Register offsets.** Use the RTL order: `CPR` `+0x00`, `MAR` `+0x04`, `LEN`
   `+0x08`, `CR` `+0x0C`, `DATA` `+0x10`. This differs from the register table in
   [peripherals.md](peripherals.md), whose order is wrong.
3. **Chip-select & clock.** Bits 7/8/9 select `cs0b`/`cs1b`/`cs2b` (only on EXI0);
   channel 1 and 2 use bit 7 only. They are mutually exclusive. Bits 6:4 select the
   clock, using 0=1 MHz … 5=32 MHz (not inverted). Honor the `SIEXILK` lock so a 32
   MHz write is ignored while locked.
4. **Transfer start / completion.** Write `EXInCR` with `tstart=1` (bit 0) to start.
   For a DMA also set `dma` (bit 1); for an immediate transfer set `tlen` (bits 5:4)
   and `rw` (bits 3:2). `tstart` clears automatically and `tcint` (bit 3) is set on
   completion; software typically polls `tstart==0` rather than using `tcint`.
5. **Immediate transfer.** The data register is bits 63:32 of a 64-bit shift
   register, shifted MSB-first (bit 63 out). A write loads the source bytes into
   `EXInDATA` before starting; a read retrieves the received bytes from `EXInDATA`
   after it completes. Transfer is full-duplex for the selected length.
6. **DMA.** The engine moves 32-byte lines between main memory (`EXInMAR`,
   auto-incrementing by 32) and the device, staged through a 32-byte FIFO. `EXInLEN`
   decrements per line. For a read DMA (device→memory) flush the write buffer before
   signalling completion — this is the coherency requirement that the data is
   actually in RAM.
7. **ROM path (EXI0 only).** A CPU read of the boot-ROM region starts an internal
   ROM read that asserts `cs1b`, forces the clock to 32 MHz, sends a 4-byte command
   and reads 8 bytes back. Cache the last line (keyed on `PiAddr[19:3]`); serve a
   hit without a serial transfer. When `romdis` (bit 13) is set, return `0xFFFF` and
   never start a transfer.
8. **Descrambler.** For the boot ROM, XOR the inbound serial stream with the key bit.
   The key is a 3-stage 16-bit LFSR cascade that steps on the negative EXI-clock edge
   during 8-byte (ROM/DMA) transfers, with the reseed/tap constants fixed in the RTL.
   `Key` is forced to 0 (identity) when `romdis` is set or the `KeyEnable` chicken bit
   is clear. Channels 1/2 always use the identity key.
9. **Interrupts.** Maintain per-channel `exiint`/`tcint`/`extint` status bits
   (write-1-to-clear) and their masks. `exiint` and `extint` are set on a falling
   edge of `intb`/`extin`; `tcint` is set on transfer completion. `extint` resets to
   1 but is masked. OR each channel's 3 masked sources, then OR all three channels
   into the PI `EXINT` bit (bit 4 of `INTSR`/`INTMSK`).
10. **Device presence / probe.** `ext` (bit 12, EXI0/EXI1) mirrors the synchronised
    `extin` level. The OS probes a device with a short immediate transfer; model this
    as returning the device's configured/attached status. Channel 2 has no `extin`.

## 13. References

- `specs/architecture/peripherals.md` — the high-level EXI/SI overview (its EXI
  register table is superseded by this document; see §10).
- `HW/IO/Memory.txt` — the `0x0C006800` / `0xCC006800` IO-space address for the
  external interface.
- `HW/IO/ProcessorInterface.md` — the PI `INTSR`/`INTMSK` `EXINT` bit (bit 4).
- `HW/IO/si.htm` — the `SIEXILK` register that locks the EXI 32 MHz clock.
- `HW/Flipper_ASIC_Block_Diagram.png` — the EXI block within Flipper and the IO
  space map.
- `RE/OS/osexi.txt` — the system-library EXI driver (`EXIProbe`, `EXISelect`,
  `EXILock`, `EXIDma`, `EXIImm`, `EXISync`, `EXIClearInterrupts`), which confirms the
  register offsets, the chip-select/clock bit positions and the write-1-to-clear
  interrupt semantics.
- `RE/OS/osexiad16.txt` — the "AD16" EXI device on channel 2 (immediate-transfer
  command/status protocol).
- `RE/EXI/exi.c` — a compact EXI driver (select/deselect, immediate read/write,
  `EXISync` and its big-endian byte unpacking).
- **US Patent 6,609,977** (External Interfaces) — the Flipper external-interface and
  register model.
- The internal EXI interface specification — the authoritative channel, chip-select,
  clock-rate, transfer-mode and register detail summarised here.
