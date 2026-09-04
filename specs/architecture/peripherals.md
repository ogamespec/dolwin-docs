# GameCube — Peripheral Devices (EXI, SI)

> This section covers the two on-chip peripheral interfaces in the Flipper IO
> block: the **Expansion Interface (EXI)** and the **Serial Interface (SI)**. The
> controller (SI) and memory-card/extended (EXI) hardware lives inside Flipper.

## 1. Expanded Interface (EXI)

EXI is the general-purpose peripheral bus. It is wired to the GameCube's external
connectors and to a few on-chip peripherals.

### 1.1 Channels and devices

There are **three** independent EXI channels, each with its own selectable clock
rate (16 / 8 / 4 / 2 / 1 MHz) and its own set of chip selects (/CS);

| Channel | Typical device in the production console |
|---|---|
| **EXI0** | CS0_0 = internal boot ROM / RTC; CS0_1 = internal flash ROM; CS0_2 = external high-speed device (modem, broadband adapter) |
| **EXI1** | CS1_0 = external device (modem, etc. — serial port 1) |
| **EXI2** | CS2_0 = external debugger (serial port 2) |

EXI0 is the **16-bit / highest-bandwidth** channel and is the one used by the
Broadband Adapter and the modem. It also contains the **descrambler** that
decrypts the boot ROM, and it drives an internal **32 MHz** line that is
electromagnetically isolated from the slower external line (an EMI countermeasure).

### 1.2 Transfer modes

- **Immediate** transfer — small (1–4 byte) transfers between a CPU register and
  the device, full-duplex so send and receive can happen at the same time. This
  is the mode used for command/status register traffic.
- **DMA** transfer — bulk data between **main memory** and the device, staged
  through a **32-byte** internal buffer. Useful for large blocks and for the
  memory cards.

### 1.3 Interrupts and presence

Each channel can generate a **transfer-complete** interrupt and, for the external
device channels, an **EXT_INT** on `EXT_IN` edge to detect device insertion/
removal. A device must be **selected** (through its chip select and a clock
frequency) before a transfer; locking/unlocking and a `probe` sequence handshake
with a device are provided by the OS driver.

### 1.4 Register map

EXI registers live at physical address `0x0C006800`, one 20-byte (5-word) block
per channel:

| Word | Register | Purpose |
|---|---|---|
| +0x00 | `EXInSTAT` | Status/control: clock, device select, interrupt flags |
| +0x04 | `EXInDATA` | Immediate-mode data/command |
| +0x08 | `EXInCTRL` | Transfer control (length, immediate/DMA, start) |
| +0x0C | `EXInMAR` | DMA memory address (32-byte aligned) |
| +0x10 | `EXInLEN` | DMA length |

## 2. Serial Interface (SI)

SI handles the four **controller** ports and the keyboard. It is located at
physical address `0x0C006400` (32-bit registers).

### 2.1 Channels and buffers

There are **four** SI channels. Each channel has:

- An **output buffer** — one command byte plus two output data bytes
  (`CMD`, `OUTPUT0`, `OUTPUT1`), double buffered.
- An **input buffer** (high + low, 32-bit each) — up to **8** response bytes plus
  per-transfer error/status bits, double buffered with a locking read sequence.

### 2.2 Polling and communication modes

- **Polling** — the SI hardware periodically polls enabled channels every `X`
  horizontal video lines, `Y` times per frame, starting from vertical blank
  (`SIPOLL`). A channel is polled by writing a command to its output buffer and
  reading the response from its input buffer. `VBCPYn` controls whether output
  buffer writes are deferred to the next blank (used to synchronise to shutter
  glasses / 3D).
- **Communication (SICOM)** — a general-purpose multi-byte transfer to any single
  channel's registers, with programmable input/output lengths (0 = 128 bytes)
  and transfer start/complete reporting.

### 2.3 Errors

`SISR` reports, per channel: no-response (device absent), collision, overrun and
underrun, plus write/read status bits for the double-buffered registers.

### 2.4 SI/EXI clock lock

`SIEXILK` prevents the software from setting the EXI clock frequencies to 32 MHz;
the serial interface uses the same clock tree as EXI.

## 3. Controller (the SI device)

The GameCube controller is an **intelligent** device: it responds to short
command/response packets. Communication is serial, **half-duplex**, over a
**single data line** using **pulse-width (duty-cycle) modulation**, with
**big-endian** byte ordering. The controller has:

- **Two analog sticks** (the left "main" stick and the right "C" stick).
- **Two analog triggers** (L and R) that also give a digital "click" when fully
  depressed.
- **Digital buttons** A, B, X, Y, Z and Start, plus a **D-pad**.
- A **rumble motor** (controlled by the host).
- Auto-calibration of the analog peripherals.

### 3.1 SI register summary (base `0x0C006400`)

| Register | Offset | Purpose |
|---|---|---|
| `SIC0OUTBUF` .. `SIC3OUTBUF` | 0x00, 0x0C, 0x18, 0x24 | Per-channel output command/data |
| `SICxINBUFH` / `SICxINBUFL` | 0x04/0x08, … | Per-channel response + error bits |
| `SIPOLL` | 0x30 | Polling X/Y and per-channel enable / vblank-copy |
| `SICOMCSR` | 0x34 | Communication control/status |
| `SISR` | 0x38 | Per-channel status/error bits |
| `SIEXILK` | 0x3C | EXI clock lock |

## 4. Devices on the peripheral bus

- **Memory cards** (two slots) — EXI devices; 512 KB / 2 MB / 8 MB at retail.
- **Broadband Adapter (BBA)** — 10BASE-T Ethernet, on the parallel/EXI0 port.
- **Modem** — serial port / EXI1.
- **Boot ROM, flash ROM, real-time clock and settings SRAM** — on-chip EXI0 devices.
- **Keyboard** — via SI.
- **Game Boy Advance** — connected through the controller/SI port and used as a
  second screen/controller.
- **Game Boy Player** — the add-on that emulates Game Boy consoles (plugs into the
  Expansion Slot, using EXI).

## 5. References

- `HW/IO/si.htm` — SI register descriptions (patent-derived).
- `RE/OS/osexi.txt`, `RE/OS/osexiad16.txt`, `RE/EXI/exi.c` — EXI driver behaviour
  (probe/select/lock/immediate/DMA) and the "AD16" EXI device.
- `HW/acronyms.txt` — EXI, SI, SI channel, EXI device, memory card, BBA entries.
- Internal EXI/SI specification and controller-interface documentation — source of
  the channel, chip-select, clock-rate, transfer-mode and register detail,
  summarised here.
- US Patent 6,609,977 (External Interfaces), US Patent 6,811,489 (Serial
  Interface).
