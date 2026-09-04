# GameCube — Clock Generator

> Scope: the retail **HW2 (Dolphin)** board. This document describes the board's
> clocking — the reference oscillator, the clock generator chip that produces the
> system clocks, the clock tree that distributes them, and how the clocking is
> handled by an emulator.

## 1. Role

Everything on the board is synchronised to a single low-frequency reference. From
it the clock generator derives the clock domains used by the major subsystems:

| Domain | Frequency | Driven by | Used by |
|---|---|---|---|
| Reference | **27 MHz** | 27 MHz crystal oscillator | Master timebase for the whole board |
| System / memory bus | **162 MHz** | Clock generator (PLL) | Gekko interface and Flipper |
| CPU core | **486 MHz** | Gekko on-die PLL (3 × bus) | Gekko core |
| Audio DSP / ARAM | **81 MHz** | ½ of the bus clock | Audio DSP, ARAM SDRAM |
| Video | **27 / 54 MHz** | Clock generator + TV encoder | Video Interface (VI) |

Getting these ratios right is what keeps the CPU speed, the memory bus, the video
beam and the audio sample rate mutually consistent. All subsystems are derived
from the one 27 MHz reference, so there is a single source of timing truth.

## 2. Reference oscillator (27 MHz)

The master reference is a 27 MHz crystal oscillator on the main board. Its
signals are:

| Pin | Direction | Function |
|---|---|---|
| `CLKO_p` | Output | Buffered 27 MHz clock output to the clock generator |
| `XIN_p` | Input | Crystal terminal; external clock input in bypass mode |
| `XOUT_p` | Output | Crystal drive terminal (inverted) |
| `STOP_n` | Input | Active-low — stop the oscillator (output held low) |
| `BYPASS_n` | Input | Active-low — bypass mode (an external clock is injected on `XIN_p`) |
| `AVDD` / `AGND` | Power | Analog supply / ground (model-only, not routed to a net) |

Behaviour:

- **Normal:** the crystal oscillates; `CLKO_p` carries the 27 MHz clock to the
  rest of the board, and `XOUT_p` is the inverted drive back to the crystal.
- **Stop:** asserting `STOP_n` low freezes the oscillator and drives the output
  low. Used to gate the clock while the board is idle/held in reset.
- **Bypass:** asserting `BYPASS_n` low disables the internal crystal and lets an
  external clock on `XIN_p` pass through to `CLKO_p`. A development/test notch.

## 3. Clock generator chip

The clock generator takes the 27 MHz reference and the reset state and produces
the system clocks. It is a **frequency-synthesis** block: it multiplies the fixed
reference up to the higher system clock used by the CPU and Flipper. Its
board-level signals are:

| Pin | Direction | Function |
|---|---|---|
| `osc_in` | Input | Reference input from the oscillator |
| `osc_out` | Output | Feedback to the crystal / oscillator |
| `cpuclk` | Output | Gekko system clock |
| `flipclk` | Output | Flipper system clock |
| `x54mhz` | Output | 54 MHz clock |
| `resetb` | Input | Active-low reset |

Operating notes:

- The CPU and Flipper are clocked by the **same system clock domain** — the clock
  generator drives both from one source. Flipper uses that clock directly; Gekko
  multiplies it on die with its own PLL to reach the core frequency.
- The **54 MHz** output is a separate domain, buffered to the board and fed to the
  Video Interface (`viclkin`); the TV encoder independently derives its own
  `27`/`54` MHz video clocks and returns them to Flipper's `viclk27`/`viclk54`.
- The chip is reset by `resetb` (active-low). While held in reset the clocks may
  be gated; the reference oscillator's `STOP_n` can also hold the clock off.
- In the chip's RTL model the system-clock period is a **parameter** (the tester
  can override it), so the model runs the simulation at a chosen frequency while
  the retail design runs the bus at 162 MHz. The divider/multiplier ratios them to
  the subsystems (DSP at ½ the bus clock, video at the 27/54 MHz base).

## 4. Clock tree and distribution

From the single reference the board fans out multiple clocks:

```
                 +--------------------+
  27 MHz crystal |  Clock generator   |-- CPU system clock ---> Gekko
   (osc_in/osc_out)|  (PLL, resetb)    |-- Flipper clock   ---> Flipper
                 +--------------------|-- 54 MHz          ---> VI (viclkin)
                          |
                          +-- 27/54 MHz (TV encoder) ---> VI (viclk27/viclk54)
```

On the CPU PLL path the board also carries a small **programmable PLL** and a
clock buffer/fan-out:

- The PLL is a general-purpose clock synthesizer with divider straps for the
  reference (`R`), output (`S`) and VCO (`V`) dividers, plus a power-down /
  tri-state control. Those dividers set the frequency the PLL generates.
- Its output is fanned out (and optionally zeroed) by a clock buffer that takes
  an input and produces a set of output clocks, gated by a two-bit select.
- The selected clock reaches the Gekko PLL, whose on-die reference divider and
  multiplier (`PLL_EXT` / `PLL_CFG[0:3]` strap pins, plus the CPU **PLL
  connection** pins `s_cpupllconn[0:3]`) set the core-to-bus ratio. The full
  multiplier table is in `HW/Gekko/PLL_CFG.txt`.

Other board nets in the clock area:

- Flipper's **PLL / reset** pins: `rstinb`, `rstswb`, `lock`, `byppll`, `clk`,
  `clko`. `lock` reports PLL lock, `byppll` selects PLL bypass.
- The **memory clocks** (`memclk`/`memclkb`/`memckq`) and the ARAM clock
  (`sdclk`) are generated by Flipper from its own clock and returned to the
  memories; the ARAM clock is half the system bus.

## 5. Reset and PLL behaviour

The clock generator and the rest of the board are held in reset during power-up.
A board power-on/reset watchdog asserts the reset line and releases it after a
number of system-clock cycles, so that the reference, the PLL and the memories
are stable before the Gekko starts fetching. Asserting reset again re-runs the
same sequence. PLL **lock** (reported on Flipper's `lock` pin) and the **bypass**
mode are scan/debug features rather than normal operating states.

## 6. Emulator application notes

An emulator does not need to model the clock generator silicon; it needs to
reproduce the **clock domains** it produces and the timing ratios between them.

- **Single timebase.** Derive every subsystem from one 27 MHz reference. The
  bus/CPU/DSP/video clocks are integer ratios of it, so a single master tick
  multiplied into each domain keeps them consistent. Avoid giving each subsystem
  an independent free-running clock.
- **Video timing.** The VI counts horizontal/vertical positions in the 27 / 54 MHz
  domain. An accurate beam position (needed for vertical-blank interrupts,
  light-gun latching and the display mode) comes from counting against the video
  clock relative to the real output resolution and refresh — not from a fixed
  "one line = N ticks" assumption, because the ratio depends on the mode
  (interlaced, double-strike, progressive).
- **Audio.** The AI/DSP sample rate and the ARAM DMA are tied to the audio clock
  tree. For a 32/48 kHz mix, derive the timestamps from the clock ratio rather
  than a host-time timer, so the streams stay in sync with the video.
- **CPU / bus.** Model the 162 MHz bus clock and the 486 MHz core (3 × bus).
  Instruction and memory-access timing, and the FIFO/burst behaviour that the
  CPU drives into Flipper, depend on the bus clock. The core multiplier is taken
  from the `PLL_EXT`/`PLL_CFG` and PLL-connection straps, so keep those as
  configuration rather than constants.
- **Determinism and reset.** The board re-arms after a fixed number of system
  clock cycles in reset. Reprocessing that count (rather than a wall-clock delay)
  keeps boot and reset reproducible. A cycle-accurate model can also reproduce
  the PLL lock period and the clock-stop/bypass test states; a high-level
  emulator can usually ignore them.
- **Where the models are directly used.** The `OSC27MHz`, clock-generator,
  PLL and buffer RTL models are what a board-level **testbench / FPGA** design
  instantiates to get its clocks. A software emulator replicates their *output*
  (the domains and ratios) instead of the modules themselves.

## 7. References

- Internal board RTL and top-level netlist (clock generator, oscillator, PLL and
  clock-buffer models; board clock nets) — source of the pins and the clock tree,
  summarised here.
- `HW/Gekko/PLL_CFG.txt` — on-die PLL multiplier table.
- `specs/architecture/gekko.md` (§ Clocks), `specs/architecture/flipper.md`
  (§ Video Interface), `specs/architecture/motherboard.md` (§ Board layout).
