# GameCube — Audio Interface (AI)

> **AI** (the Audio Interface) is the block inside the **Flipper** ASIC that sits
> between the audio DSP, the disk-drive auxiliary/streaming audio path and the
> external stereo audio DAC. It owns the programmer-visible **streaming
> audio-control registers** at `0x0C006C00`, converts the streamed audio and the
> DSP output into a single 16-bit stereo sample stream, and serialises that
> stream out to the DAC. The main-memory-to-FIFO **audio DMA** that actually
> feeds the AI is a separate controller in the DSP/audio register space — it is
> controlled from the `0x0C005030`-area registers and is described here only to
> keep the two sides of the audio path distinct.

This is the register-level, emulator-focused specification. The facts are drawn
from the repository's hardware documentation, the Flipper RTL and the
system-library reverse-engineering notes, and are summarised (paraphrased) rather
than reproduced verbatim. Where a behaviour is only implicit in the RTL — a
self-clearing pulse, the exact effect of a mode flag, the width of a counter —
it is called out as such.

## 1. Overview

The AI is the output stage of the console's audio path. It brings together three
inputs:

- the **DSP** left/right sample stream (`dsp_ioL` / `dsp_ioR`),
- the **DVD / auxiliary streaming** serial input from the disk drive
  (`aisd` / `aisclk` / `aislr`),
- and a programmable **volume** for the streaming channel (`AIVR`).

and produces one 16-bit stereo sample stream that is shifted out to the stereo
DAC on `aid` / `ailr` / `aiclk`. To do this it runs the streaming input through a
sample-rate converter (so the stream is always 48 kHz at the mixer), scales it by
the volume registers, **adds** the DSP output, saturates the sum, and serialises
the result.

The AI register block is reached through the **IO-module** interface of the
Processor Interface: it is IO module `io_addr_ai` (selected by `PiAddr[11:10] ==
3`), so its registers sit at physical base **`0x0C006C00`** (uncached alias
`0xCC006C00`). The register decode inside the AI uses `PiAddr[4:2]`; each
register is 32-bit internally, at byte offsets `index*4`.

It is one of the four peripheral sub-blocks of the Flipper **I/O (`io`) module**
(the others being DI, SI and EXI). The `io` block also contains the 16-bit register
interface (`io_Pi`, which decodes the `0x0C006000`–`0x0C006C00` module bases) and a
shared main-memory port (`io_Mem`) with a round-robin IO-DMA arbiter; each
peripheral sub-block has its own interrupt (`ai_piInt` here) that PI aggregates. The
AI's register block is in `io`, but its **audio DMA** is not: that lives in the
DSP/audio register space and is described separately to keep the two sides of the
audio path distinct.

```
io  (I/O module)
├── io_Pi    — register/CPU interface (16-bit PI path, module decode)
├── io_Mem   — shared main-memory port + round-robin IO-DMA arbiter
├── io_di    — DI  (disk-drive command transport)
├── io_Si    — SI  (serial / controller interface)
├── io_Exi   — EXI (expansion, 3 channels)
├── io_Ai_top— AI  (audio interface)                        « this block »
└── io_TstMux— test/scan mux and pad output-enable control
```


### 1.1 Sub-blocks

| Block | Role |
|---|---|
| `io_Ai` | The main datapath / register interface: register decode mux, the 48 kHz DSP path, the streaming-input deserialiser, the DAC serial shifters, and the interrupt generation |
| `io_Ai_dsp32` | The parallel 32 kHz DSP path (a second, narrower SRC/mixer datapath used when `AICR.DFR` selects 32 kHz DSP audio) |
| `io_AiSRC` + `io_AiSRC_fsm` | The sample-rate converter — a 128-tap FIR resampler fed from a small sample RAM; also performs the volume multiply and the DSP add |
| `io_AiMem` | The streaming sample RAM (96×16, one read/one write port) and the L/R pointer logic that feeds the SRC |
| `io_Ai_fsm` / `io_Ai_fsm_dsp32` | The top-level mixing/datapath sequencer (start-up delay, SRC, volume, DSP add) |
| `io_AiClks` | The clock/shape generators for the DAC bit-clock (`aiclk`) and the streaming bit-clock (`aisclk`), including the 32↔48 kHz stream clock muxes |
| `io_AiCSR` | The control/status register file (AICR, AIVR, AISCNT, AIIT) and the sample-counter / interrupt match logic |
| `io_AiAdd16x16` | The 16×16 adder that sums the two SRC results and the saturation (clamp) logic in `io_Ai_top` |

The two DSP paths (`io_Ai` for 48 kHz DSP audio and `io_Ai_dsp32` for 32 kHz DSP
audio) each produce a 16-bit result; `io_Ai_top` sums them and clamps the result
before it reaches the DAC serialiser.

## 2. External pins / signals

The AI has two serial interfaces and a mode flag on the chip boundary. All serial
data is 16-bit, left-justified, most-significant-bit first.

### 2.1 DAC output (AI → external DAC)

| Signal | Width | Dir | Description |
|---|---|---|---|
| `aid` | 1 | out | Serial audio data to the DAC — the 16-bit L or R sample, MSB first, one bit per `aiclk` |
| `ailr` | 1 | out | Left/right frame — toggles once per stereo sample at the **48 kHz** output rate; an edge is also the sample-conversion strobe to the DAC |
| `aiclk` | 1 | out | Bit clock for `aid` |

`aid` is the MSB of the current L or R shift register, selected by the `ailr`
level. The output stream is always at the 48 kHz sample rate (the DAC clock is
derived from the fixed video clock), regardless of the input sample rate.

### 2.2 Streaming input (disk drive → AI)

| Signal | Width | Dir | Description |
|---|---|---|---|
| `aisd` | 1 | in | Serial audio data from the disk drive — the 16-bit L or R sample, MSB first, one bit per `aisclk` |
| `aislr` | 1 | out | Left/right frame for the stream — toggles at the **stream** sample rate (32 or 48 kHz) |
| `aisclk` | 1 | out | Bit clock for `aisd` (the AI drives it to the disk) |

`aislr` is what **gates** the flow of stream data: while `AICR.PSTAT` is set the
frame keeps toggling and the disk keeps sending; when it stops, the disk treats
the stream as paused and sends zeros, and only begins again after it receives a
high-low-high sequence (see §4).

### 2.3 DSP and mode signals

| Signal | Width | Dir | Description |
|---|---|---|---|
| `dsp_ioL` / `dsp_ioR` | 16 each | in | The DSP's left / right output samples |
| `io_dspPop` | 1 | out | Strobes the DSP to present a new L/R sample (one io-clock pulse) |
| `dvdaudio_mode` | 1 | out | DVD-audio mode flag, taken from `AIIT[31]` (see §8.4) |

`io_dspPop` is asserted briefly whenever the AI consumes a new DSP sample, and is
used by the DSP to advance its output. `dvdaudio_mode` is a programmer-set flag
that is surfaced out of the AI; its external effect is described in the repo's
hardware docs but is not elaborated further in the AI RTL itself (noted below).

## 3. Audio path and sampling

The AI must present a single 48 kHz stereo stream to the DAC no matter what the
input rate is. The pipeline is:

```
stream in (32/48 kHz) ─▶ deserialise L/R ─▶ sample RAM ─▶ SRC/FIR (→48 kHz) ─▶ volume (×AVR) ─┐
                                                                                              ├─▶ add ─▶ clamp ─▶ L/R shift ─▶ aid/ailr/aiclk
DSP L/R (32/48 kHz) ─────────────────────────────────────────▶ (dsp32 path SRC if 32 kHz) ──┘
```

### 3.1 DSP sample-rate mode (`AICR.DFR`)

The DSP normally runs at **48 kHz**, but it can run at **32 kHz** (controlled by
`AICR.DFR`, the `dsp32` field — see §8.1 for the name reconciliation). Two parallel
datapaths exist:

- The **48 kHz DSP path** (`io_Ai`, the main `io_Ai`) mixes the directly-presented
  DSP L/R into the stream (already at the DAC rate).
- The **32 kHz DSP path** (`io_Ai_dsp32`) is a narrower copy of the SRC/mixer that
  resamples a 32 kHz DSP stream up to 48 kHz before mixing.

Only one path actually carries the DSP audio at a time: the top level muxes the
DSP L/R input to one path (feeding zeros to the other) based on a registered
version of `AICR.DFR`. The two results are then added (§3.3).

### 3.2 The sample-rate converter (SRC)

When the streaming input is at **32 kHz** (`AICR.AFR = 0`), the SRC resamples it
to 48 kHz. It does so with a **128-tap FIR** filter (the filter is symmetric, so
only 64 coefficients are stored) operating over a 3:2 (48/32) polyphase
relationship: for every two input samples it produces three output samples, and
the FSM cycles a 3-phase `period` counter accordingly. Each channel is buffered
in a small sample RAM (96 × 16); the SRC reads a window of 43 samples (the value
`K` in the RTL) per channel and interpolates.

When the stream is already at **48 kHz** (`AICR.AFR = 1`), the stream still
passes through the mixing datapath but the conversion reduces to a bypass — the
stream samples are scaled and added directly without FIR interpolation.

The first ~68 output frames after play starts are intentionally suppressed: the
SRC needs a full history of stream samples before it can produce a valid output,
so the first samples out of the DAC are zeroed (the "start-up" counter in the
FSM). This is an implementation detail an emulator can reproduce or skip.

### 3.3 Volume, mixing and clamping

The streamed samples are scaled by the auxiliary volume registers (`AIVR.AVRL`
for the left channel, `AIVR.AVRR` for the right). Volume is an 8-bit value;
`0x00` mute, `0xFF` full scale. The SRC builds the 16-bit multiplier for the
volume stage from the byte, effectively scaling the sample by `volume/256`.

The scaled stream sample is then **added** to the DSP sample for the same side
(left with left, right with right). The two SRC datapath results (the 48 k path
and the 32 k path) are summed by a 16×16 adder, and the result is **clamped**:
on overflow it saturates to `0x7FFF` (positive), on underflow to `0x8000`
(negative). This is the saturation in `io_Ai_top`. One of the two addends is zero
in normal operation (because the DSP audio is muxed to only one path), so the
mixer is effectively the stream plus the DSP, but the clamp always applies to the
final 16-bit sum.

### 3.4 DAC serial output

The 16-bit L and R results are loaded into two shift registers and clocked out,
MSB first, on `aiclk`; `ailr` selects which shift register is currently driving
`aid`. `ailr` toggles once per stereo frame at 48 kHz. Data is loaded on the
negative edge of `aiclk`, so the serial stream is continuous.

## 4. Streaming audio input

The auxiliary/streaming input is the DVD-audio path: a serial PCM bitstream from
the optical drive, clocked by the AI (the AI is the clock master for this
interface).

- **Deserialisation.** `aisd` is shifted into a 16-bit register one bit per
  `aisclk` positive edge; `aislr` (delayed) selects left or right. Completed L and
  R samples are written into the sample RAM.
- **Sample rate.** The stream is either **32 kHz** or **48 kHz** as selected by
  `AICR.AFR` (§8.1). At 32 kHz it is resampled to 48 kHz by the SRC; at 48 kHz it
  passes through.
- **Start / pause gate (`AICR.PSTAT`).** `AICR.PSTAT` is the play status. When it
  is set the streaming bit-clock and the `aislr` frame are enabled; the disk keeps
  sending samples. When cleared, the frame stops toggling, the disk treats the
  stream as paused and sends zeros, and it only resumes after a high-low-high
  `aislr` sequence. So the streaming interface is effectively enabled/disabled by
  `PSTAT`, not by a separate clock-enable register.
- **Mixing.** The deserialised, resampled stream is multiplied by `AIVR`, added to
  the DSP output, clamped, and sent to the DAC (§3).

The streaming input is also what drives the **sample counter** used for the
streaming interrupt: each stereo frame produced by the streamed path (post-SRC, at
48 kHz) increments `AISCNT` while `PSTAT` is set (§8.3, §9).

## 5. Interrupts

The AI contributes **two distinct** interrupts to the host, and it is worth
separating them because they come from different places.

### 5.1 Streaming sample-counter interrupt (AIINT)

This is the interrupt the AI itself generates. It is raised when the streamed
sample counter (`AISCNT`) reaches the programmable trigger in `AIIT`:

```
AIINT is set when  (AISCNT == AIIT)  AND  AICR.AIINTVLD == 0  AND  AISCNT != 0
```

- `AIIT` holds the target stereo-sample count (bits 30:0).
- `AISCNT` counts streamed stereo frames (bits 30:0). It must be non-zero (so the
  very first sample does not fire the interrupt immediately).
- `AICR.AIINTVLD` (interrupt valid) gates the match: when it is set the interrupt
  **holds its last value** and the match is ignored; when clear the match affects
  `AIINT`.

The match produces a single-cycle pulse that **sets** `AICR.AIINT`. The pending
`AIINT` bit is then gated by `AICR.AIINTMSK` to produce the AI's request line
`ai_piInt`, which the Processor Interface carries into the PI cause register as
**`AIINT` (INTSR bit 5 / AIMSK)**.

- `AICR.AIINT` is the interrupt **status** — read to query, and **write-1 to
  clear** (the standard PowerPC convention). It asserts regardless of
  `AICR.AIINTMSK` (i.e. even if the request to the CPU is masked, the status bit
  still latches); the mask only controls whether the request is reported.
- `AICR.AIINTMSK` enables the request to the CPU (`ai_piInt`) when set.

At the PI level `AIINT` is one source among many; the OS reads `INTSR` and, on
`AIINT`, reads `AISCNT`/`AIIT` and clears `AICR.AIINT`.

### 5.2 Audio-DMA interrupt (AIDINT)

The main-memory→FIFO **audio DMA** lives in the DSP/audio register space, not in
the AI IO block. Its controller is configured with `AID_MADRH`/`AID_MADRL`/
`AID_LEN`/`AID_CNT` at `0x0C005030`–`0x0C00503A` (see §7 and §9). When its byte
counter (`AID_CNT`) reaches zero it raises the **AIDINT** interrupt, and if the
auto-restart bit in `AID_LEN` is set the DMA reloads its start address and length
and re-runs.

AIDINT is **not** the AI's `ai_piInt`; it originates in the audio subsystem's DMA
controller and is reported separately (through the DSP/audio block's interrupt
handling). Emulators must model them as two independent sources.

## 6. Register access

All AI registers are reached through the **16-bit** IO register interface, exactly
as every other Flipper register block:

- `PiData[15:0]` carries data; `PiAddr[19:1]` is a halfword-granular address.
- Only **2-byte** and **4-byte** accesses are valid; any other size (or a burst,
  or a byte) to the register space is a Processor-Interface error (see
  [processor-interface.md](processor-interface.md) §2.3).
- A **32-bit** access is split by PI into two back-to-back 16-bit transfers,
  **big-endian**: the **high** word at byte offset `N`, the **low** word at
  `N+2`.
- Each AI register is **32-bit** internally. The AI's own read mux hands each
  halfword back to PI depending on `PiAddr[1]`: the low-address halfword is the
  high 16 bits of the register, the high-address halfword is the low 16 bits.

Within the AI, `PiAddr[4:2]` selects the register (index = byte offset ÷ 4) and
`PiAddr[1]` selects the halfword. AICR, AIVR, AISCNT and AIIT are at indexes
0–3.

## 7. Register map

The AI occupies `0x0C006C00`–`0x0C006C0E` (uncached alias `0xCC006C00`). Bytes
are offsets from `0x0C006C00`.

| Addr | Name | R/W | Description |
|---|---|---|---|
| `0x00` | `AICR` | R/W | AI Control Register — play status, rates, interrupt status/mask, sample-counter reset, DSP rate |
| `0x04` | `AIVR` | R/W | AI Volume Register — left (`AVRL`) and right (`AVRR`) stream volume |
| `0x08` | `AISCNT` | RO | AI Streaming Sample Counter (stereo frame count) |
| `0x0C` | `AIIT` | R/W | AI Interrupt Timing — the stereo-count trigger; bit 31 is `DVDAUDIO` |

The **audio DMA** registers that feed the AI FIFO are *not* part of this block.
They live in the DSP/audio register space and are listed here only to distinguish
the two faces of the audio system:

| Addr | Name | R/W | Description |
|---|---|---|---|
| `0x0C005030` | `AID_MADRH` | R/W | DMA start address, bits 31:16 |
| `0x0C005032` | `AID_MADRL` | R/W | DMA start address, bits 15:0 (32-byte aligned) |
| `0x0C005036` | `AID_LEN` | R/W | Bit 15 = DMA enable/restart; bits 14:0 = length in 32-byte blocks |
| `0x0C00503A` | `AID_CNT` | RO | Counts down to zero — number of 32-byte blocks remaining |

Note: the DMA FIFO is 32 bytes (one block); the FIFO drains at 32 or 48 kHz stereo
depending on `AISetDSPSampleRate`. The four registers above are the documented
programmer-visible controls; the controllers also track a current-DMA-address
internally, but its exact location has not been pinned down and is not part of the
AI IO block.

## 8. Register fields

Bit positions below follow the LSB-first field order used by the register macros
(and match the reverse-engineering header in `RE/AI/ai.txt`). Where the hardware
documentation uses a different mnemonic for a field, both names are given.

### 8.1 `AICR` (0x00, 32-bit)

Reset value `0x0000_0000`. The fields occupy the low 7 bits; bits 31:7 read as 0.

| Bits | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 0 | `PSTAT` | R/W | 0 | Play status. `1` enables the streaming `aislr` clock / starts playback; `0` stops or pauses the stream (the disk then sends zeros). Also enables the counter increment. |
| 1 | `AFR` | R/W | 0 | Streaming (auxiliary) sample rate. `0` = 32 kHz, `1` = 48 kHz. The SRC converts a 32 kHz stream to 48 kHz. Should only be changed while `PSTAT` = 0. |
| 2 | `AIINTMSK` | R/W | 0 | Streaming-interrupt mask. `0` = masked, `1` = enabled (drives `ai_piInt`). |
| 3 | `AIINT` | R/WC | 0 | Streaming-interrupt status. Read: `0` no request, `1` request. **Write 1 to clear.** Asserts regardless of `AIINTMSK`. |
| 4 | `AIINTVLD` | R/W | 0 | Interrupt valid / hold. `0` = `AISCNT==AIIT` match affects `AIINT`; `1` = `AIINT` holds its last value (match ignored). |
| 5 | `SCRESET` | W (self-clearing) | 0 | Sample-counter reset. Write `1` to clear `AISCNT` to 0. Reads as 0. |
| 6 | `DFR` / `dsp32` | R/W | 0 | DSP sample-rate mode. `0` = 48 kHz DSP, `1` = 32 kHz DSP. Selects which DSP path is used (§8.1 note). |
| 31:7 | — | RO | 0 | Reserved |

> **Field-name reconciliation — bit 6.** The hardware documentation calls this
> field **`DFR`** ("AI DMA sample rate", "different from AFR"), and the register
> macro (`RE/AI/ai.txt`, `AICR_DFR_SHFT = 6`) and the RTL name it **`dsp32`**
> (`AI_CR_DSP32`). They are the same field, at bit 6. Its meaning is the **DSP**
> sample-rate mode (32 vs 48 kHz), which is the rate the DSP audio is produced at
> (resampled to 48 kHz at the mixer if 32 kHz). This is deliberately distinct
> from `AFR` (bit 1), which is the **streaming/auxiliary** input sample rate. The
> `DFR`/`dsp32` field is a retail-("HW2"/rev B) addition — it was not present on
> the original (rev A) part.

### 8.2 `AIVR` (0x04, 32-bit)

Reset value `0x0000_0000` (both channels muted). The two volume bytes occupy bits
15:0; bits 31:16 read as 0.

| Bits | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 7:0 | `AVRL` | R/W | `0x00` | Left-channel volume for the streaming (auxiliary) channel. `0x00` = mute, `0xFF` = maximum. |
| 15:8 | `AVRR` | R/W | `0x00` | Right-channel volume for the streaming (auxiliary) channel. `0x00` = mute, `0xFF` = maximum. |
| 31:16 | — | RO | 0 | Reserved |

### 8.3 `AISCNT` (0x08, 32-bit, read-only)

Reset value `0`. The counter is effectively **31-bit** (bits 30:0); bit 31 reads
as 0.

| Bits | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 30:0 | `AISCNT` | RO | 0 | Stereo-sample counter. Increments once per streamed stereo frame output (post-SRC) while `PSTAT` is set; cleared by reset or by `AICR.SCRESET`. |
| 31 | — | RO | 0 | Reserved |

> The counter may be read as two 16-bit halves (`AISCNT` high halfword = bits
> 30:16, low halfword = bits 15:0). Since it is only 31 bits, the most-significant
> bit of the high halfword is always 0. This differs from the hardware
> documentation which lists all 32 bits as `AISCNT`; the RTL counter is 31 bits.

### 8.4 `AIIT` (0x0C, 32-bit)

Reset value `0`. Bit 31 is the DVD-audio mode flag.

| Bits | Field | R/W | Reset | Description |
|---|---|---|---|---|
| 30:0 | `AIIT` | R/W | 0 | Interrupt-timing trigger. The AI raises the streaming interrupt when `AISCNT` matches this value (subject to `AICR.AIINTVLD` and `AISCNT != 0`). |
| 31 | `DVDAUDIO` | R/W | 0 | DVD-audio mode flag. Surfaces as the `dvdaudio_mode` signal. |

> `AIIT[31]` (`DVDAUDIO`) is written through the high halfword of the register
> (byte offset `0x0C`). Its effect outside the AI is described in the hardware
> docs but is not implemented inside the AI RTL beyond being output as
> `dvdaudio_mode`; treat it as a mode flag whose downstream use is specified
> elsewhere.

## 9. Emulator notes

1. **Register width / access.** Model each of `AICR`, `AIVR`, `AISCNT`, `AIIT` as
   a 32-bit register. A 4-byte access is split into a high-address halfword at
   byte offset `N` and a low-address halfword at `N+2` (big-endian, high-word
   first). Halfword addresses: `AICR` 0x00/0x02, `AIVR` 0x04/0x06, `AISCNT`
   0x08/0x0A, `AIIT` 0x0C/0x0E. Reject byte/burst/unsized access as a PI error.
   `AICR` and `AIVR` only use the low halfword; `AISCNT` and `AIIT` span both.
2. **Sample counter.** Keep `AISCNT` as a 31-bit value in bits 30:0 (bit 31 always
   0). Increment it by 1 for every stereo output frame produced while `AICR.PSTAT`
   is set (i.e. while streaming is playing). Clear it on reset and on a write of
   `AICR.SCRESET = 1` (which reads back 0 — self-clearing).
3. **Interrupt match.** The streaming interrupt is raised when `AISCNT` (non-zero)
   equals `AIIT[30:0]` **and** `AICR.AIINTVLD == 0`. When it fires, set
   `AICR.AIINT` (a one-cycle pulse). `AICR.AIINT` is write-1-to-clear. The request
   line to the CPU is `AICR.AIINT AND AICR.AIINTMSK` → `ai_piInt` → PI `INTSR`
   bit 5 (`AIINT`/`AIMSK`). Note: `AIINT` still latches even when `AIINTMSK` is 0;
   the mask only controls whether it is reported.
4. **Volume.** Scale the streamed sample by `AIVR.AVRL` (left) / `AIVR.AVRR`
   (right). Volume is 8-bit: `0x00` mute, `0xFF` full scale. The SRC builds a
   16-bit unsigned multiplier from the byte (a 1.15 fraction equal to
   `(VOL << 7) | 0x7F`, i.e. `0x7FFF` at full scale) and multiplies the sample by
   it (shifting right by 15). `0xFF` is therefore ≈ 1.0 full scale, while `0x00`
   leaves a small ≈ 1/256 residual rather than exact zero. For most purposes an
   emulator can multiply by `VOL / 256` and saturate, or use the exact fixed-point
   form.
5. **Sample-rate conversion.** If `AICR.AFR == 0` the stream is 32 kHz and must be
   resampled to 48 kHz (a 3:2 polyphase FIR, 128 taps / 64 symmetric
   coefficients) before it reaches the mixer. If `AFR == 1` it is already 48 kHz
   (bypass the resample). An emulator can approximate this with a good-quality
   interpolator at the same 3:2 ratio, or simply skip the stream for
   correctness-critical (non-audio) emulation.
6. **DSP sample-rate mode (`DFR`/`dsp32`).** If `AICR.DFR == 1`, the DSP stream is
   32 kHz and must be independently resampled to 48 kHz (the second datapath)
   before the mixer; if `0` it is already 48 kHz. At the mixer only one DSP path
   is active depending on this bit.
7. **Mixer and clamping.** Add the scaled streamed sample to the DSP sample for the
   same channel, then saturate the 16-bit result to `0x7FFF` on overflow and
   `0x8000` on underflow. Do not allow wrap-around.
8. **DAC serialisation.** Serialise the 16-bit L and R samples out on `aid`, MSB
   first, one bit per `aiclk`; `ailr` toggles once per stereo frame at 48 kHz and
   selects left vs right. The output is always 48 kHz.
9. **Streaming clock gating (`PSTAT`).** `AICR.PSTAT` enables the streaming
   bit-clock/`aislr`. When `PSTAT` is 0 the `aislr` frame stops, the disk pauses
   and sends zeros; the disk resumes on a high-low-high `aislr` sequence. Only
   start the streaming clock (and increment `AISCNT`) when `PSTAT` is 1.
10. **`dvdaudio_mode`.** Expose `AIIT[31]` as a read/write mode flag. Its effect is
    not implemented inside the AI RTL, so model it as a stored flag unless the
    downstream consumer is also modelled.
11. **AIDINT vs AIINT.** Do not conflate the streaming interrupt (`AIINT`, from the
    AI block, §5.1) with the audio-DMA interrupt (`AIDINT`, from the DSP/audio DMA
    controller `AID_CNT==0`, §5.2). They are independent sources with independent
    servicing.

## 10. References

- `HW/AudioSystem/AudioInterface.md` — the AI register/behaviour summary and the
  audio DMA registers (`AID_MADRH`/`AID_MADRL`/`AID_LEN`/`AID_CNT`); the source
  of the AICR/AIVR/AISCNT/AIIT field descriptions and the external-signal table.
- `HW/AudioSystem/dsp_info.md` — the audio subsystem and the distinction between
  the DSP/audio DMA interrupts and the AI stream interrupt.
- `RE/AI/ai.txt` — the OS AI library register accesses, the `AICR` bit shifts
  (confirming `DFR` at bit 6) and the DMA register addresses.
- `RE/OS/osaud.md` — the OS audio-system initialisation and DSP/audio control
  registers, for context on how the AI fits the audio subsystem.
- `specs/architecture/flipper.md` §4 — the existing high-level Audio DSP and Audio
  Interface description.
- `specs/architecture/processor-interface.md` §3–§4 — the 16-bit register-access
  model and the PI interrupt aggregation (`AIINT`/`AIMSK`, INTSR bit 5).
- **US Patent 6,609,977** (External interfaces) and **US Patent 7,369,665**
  (Mixing audio) — the AI register and response behaviour these summaries
  paraphrase.
