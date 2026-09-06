# GameCube — Command Processor (CP)

> **CP** (the Command Processor) is the module inside the **Flipper** ASIC that reads
> the graphics command stream and the display lists from main memory, fetches and
> caches the vertex-attribute data, and turns it into the vertex stream that is
> consumed by the **transform unit (XF)**. It is a *separate* Flipper module: it is
> not a stage of the graphics pipeline, it is the **source** that feeds it. All
> graphics commands enter the pipeline at the **XF**, and the CP is what delivers
> them there.

This is the register-level, emulator-focused specification. The facts are drawn from
the repository's hardware documentation and the Flipper chip RTL and are summarised
(paraphrased) rather than reproduced verbatim.

## 1. Overview

### 1.1 Role and connection

The CP sits between the Gekko CPU and the GFX pipeline. On its input side it is fed
by the CPU through the **Processor Interface (PI)** — the CPU writes the command
stream to the **PI FIFO** at `0x0C00_8000` (from the uncached physical map), and the
CP consumes it. On its output side the CP produces the per-vertex command/attribute
stream that the **XF** reads. The CP is a **master** on the main-memory interface: it
reads the command stream, the display lists and the vertex-attribute arrays out of
main memory through its own FIFO.

```
Gekko ──PI FIFO (0x0C008000)──▶ Command Processor ──▶ XF ──▶ (graphics pipeline)
                                      │
        ┌─────────────────────────────┼──────────────────────────────┐
        │ command FIFO (main memory)  │ call FIFO (display lists)    │ vertex cache (arrays)
        └─────────────────────────────┴──────────────────────────────┘
```

The CP is also the block that the **PI** routes the CPU's graphics-FIFO burst writes
to, and it is one of the interrupt sources in the PI interrupt controller (bit 11,
`CPINT`).

### 1.2 Sub-blocks

| Block | Role |
|---|---|
| Command FIFO | The on-chip buffer that receives and buffers the command stream read from main memory for flow control / load balancing |
| Call FIFO | The on-chip buffer that receives display-list (object) calls |
| Vertex cache | The 8 KB, 8-way set-associative cache for vertex attributes fetched from indexed arrays |
| Array / attribute engine | Computes the indexed-attribute address and unpacks each attribute per the VCD/VAT setup |
| CP→XF interface | The command/vertex output to the XF, plus the register-forwarding path (`xfAddr`/`xfData`/`numRegs`) |

## 2. Command stream and FIFOs

### 2.1 The two FIFOs

Two FIFOs carry commands to the GFX, and they are owned by different sides:

- **PI FIFO** — on the *Gekko* side. The CPU performs a **32-byte burst** store to `0x0C00_8000`; each burst advances a write pointer by 32 bytes. It is managed by the PI-side registers `CPBAS`/`CPTOP`/`CPWRT`/`CPABT` (see [processor-interface.md](processor-interface.md) §6.2).
- **CP FIFO** — on the *CP* side. The CP reads the stream from main memory using its own configuration (base/top, high/low water marks, read/write pointers, break point) through the CP host registers (§4).

The reason for the split is that the CP can only *read* memory (it always acts as a
consumer), while the PI is what handles Gekko burst operations. When the CPU writes
to the PI FIFO, a notification is sent to the CP so it can adjust its write pointer
registers and start consuming the stream.

### 2.2 CP FIFO operation

The CP FIFO (the read side) is configured purely by the CP host registers. All reads
are in **32-byte** chunks.

- **Linked mode** (`CP_ENABLE[4]` = 1) — writing another portion to `0x0C00_8000` causes the CP write pointer to advance by 32, and the CP processes the FIFO as the distance between its read and write pointers grows. The watermark logic is active in this mode.
- **Multi-buffer mode** (`CP_ENABLE[4]` = 0) — the program sets the read and write pointers itself, and the CP processes the FIFO as long as the FIFO count is greater than zero. The distance between the CP read and write pointers is the FIFO count.

Watermarks: when the FIFO count falls below the **low water mark** an **underflow** is
flagged; when it rises above the **high water mark** an **overflow** is flagged. When
the under/overflow interrupt is active the CP stops reading new data. A **break point**
(an absolute read address in the FIFO) also stops the CP and raises an interrupt when
the read pointer reaches it.

### 2.3 Call FIFO (display lists)

A **display list** is an object called from the main stream with the `Call_Object`
command: the CP pushes the current context and reads the object's bytes (a 32-byte
count) through the **call FIFO**. Display lists are **not nested** — an object cannot
call another object.

## 3. Command format

A command is a 32-bit word. The top bits select the operation and the low bits carry
the operand / vertex-attribute-table (`vat`) selector. For the draw commands the low
3 bits index the **vertex attribute table** (which `vat` to use), so there are up to
8 VATs per draw.

| Opcode | Name | Meaning |
|---|---|---|
| `00000xxx` | `NOP` | No operation |
| `00001xxx` | `CP_LoadRegs` | Load a CP (command-stream) register — address + 32-bit data |
| `00010xxx` | `XF_LoadRegs` | Load XF registers (including matrices) from immediate data |
| `00100xxx` | `XF_IndexLoadRegA` | Block-load XF matrix/light registers via index array A |
| `00101xxx` | `XF_IndexLoadRegB` | … via index array B |
| `00110xxx` | `XF_IndexLoadRegC` | … via index array C |
| `00111xxx` | `XF_IndexLoadRegD` | … via index array D |
| `01000xxx` | `Call_Object` | Call a display list (address + 32-byte count); cannot nest |
| `01001xxx` | `V$_Invalidate` | Invalidate the vertex cache |
| `0110xxxx` | `SU_ByPassCmd` | A setup-unit (bypass) register load; bypasses the XF |
| `10000vat` | `Draw_Quads` | Draw quads |
| `10001vat` | `Draw_QuadStrip` | Draw a quad strip |
| `10010vat` | `Draw_Triangles` | Draw triangles |
| `10011vat` | `Draw_TriStrip` | Draw a triangle strip |
| `10100vat` | `Draw_TriFan` | Draw a triangle fan |
| `10101vat` | `Draw_Lines` | Draw lines |
| `10110vat` | `Draw_LineStrip` | Draw a line strip |
| `10111vat` | `Draw_Points` | Draw points |

The draw commands are followed by a 16-bit **vertex count**, then the vertex
attribute stream. The register-load commands are followed by a variable number of
32-bit data words (see §5.1).

## 4. Host registers

The CP has its own register space, reached by the CPU through the 16-bit register bus
at **`0x0C00_0000`** (the `000` IO-space module, see
[processor-interface.md](processor-interface.md) §6.1). The table gives the register
index within that space (the register-select field); the data path is 16-bit like
every other Flipper register.

### 4.1 Register map (host / CPU-visible)

| Index | Name | Description |
|---|---|---|
| `0x00` | `CP_STATUS` | FIFO status — overflow, underflow, read-idle, CP-idle, breakpoint |
| `0x01` | `CP_ENABLE` | FIFO enable, breakpoint enable, overflow/underflow/break int enables, write-ptr increment |
| `0x02` | `CP_CLR` | Clear overflow/underflow interrupt, clear performance counters |
| `0x03` | `CP_MEMPERF_SEL` | Memory performance-counter select |
| `0x05` | `CP_STM_LOW` | Streaming buffer low-water mark (in 32-byte increments) |
| `0x10` | `CP_FIFO_BASEL` | FIFO base address, low |
| `0x11` | `CP_FIFO_BASEH` | FIFO base address, high |
| `0x12` | `CP_FIFO_TOPL` | FIFO top address, low |
| `0x13` | `CP_FIFO_TOPH` | FIFO top address, high |
| `0x14` | `CP_FIFO_HICNTL` | High water mark, low |
| `0x15` | `CP_FIFO_HICNTH` | High water mark, high |
| `0x16` | `CP_FIFO_LOCNTL` | Low water mark, low |
| `0x17` | `CP_FIFO_LOCNTH` | Low water mark, high |
| `0x18` | `CP_FIFO_COUNTL` | FIFO count (entries currently in the FIFO), low |
| `0x19` | `CP_FIFO_COUNTH` | FIFO count, high |
| `0x1A` | `CP_FIFO_WPTRL` | FIFO write pointer, low |
| `0x1B` | `CP_FIFO_WPTRH` | FIFO write pointer, high |
| `0x1C` | `CP_FIFO_RPTRL` | FIFO read pointer, low |
| `0x1D` | `CP_FIFO_RPTRH` | FIFO read pointer, high |
| `0x1E` | `CP_FIFO_BRKL` | FIFO read break point, low |
| `0x1F` | `CP_FIFO_BRKH` | FIFO read break point, high |
| `0x20`–`0x27` | `CP_COUNTER0..3` | Four performance counters |
| `0x28` | `CP_VC_CHKCNTL` | Vertex-cache check counter, low |
| `0x29` | `CP_VC_CHKCNTH` | Vertex-cache check counter, high |
| `0x2A` | `CP_VC_MISSL` | Vertex-cache miss counter, low |
| `0x2B` | `CP_VC_MISSH` | Vertex-cache miss counter, high |
| `0x2C` | `CP_VC_STALLL` | Vertex-cache stall counter, low |
| `0x2D` | `CP_VC_STALLH` | Vertex-cache stall counter, high |
| `0x2E` | `CP_FRCLK_CNTL` | Frame-clock counter, low |
| `0x2F` | `CP_FRCLK_CNTH` | Frame-clock counter, high |
| `0x30` | `CP_XF_ADDR` | XF register address (for the register forward/read-back path) |
| `0x31` | `CP_XF_DATAL` | XF register data, low |
| `0x32` | `CP_XF_DATAH` | XF register data, high |
| `0x33` | `CP_NUM_REGS` | Number of registers to forward |

### 4.2 `CP_STATUS` (0x00)

| Bit | Field | Meaning |
|---|---|---|
| 0 | `OVFL` | FIFO overflow — the count rose above the high water mark |
| 1 | `UNFL` | FIFO underflow — the count fell below the low water mark |
| 2 | `RDIDLE` | The FIFO read unit is idle |
| 3 | `CPIDLE` | The command processor is idle |
| 4 | `BRK` | The FIFO read pointer reached the break point (cleared by disabling the break point) |

### 4.3 `CP_ENABLE` (0x01)

| Bit | Field | Description | Reset |
|---|---|---|---|
| 0 | `FIFORD` | Enable FIFO reads | `0` (disabled) |
| 1 | `FIFOBRK` | FIFO break-point enable | `0` |
| 2 | `OVFLINT` | FIFO overflow interrupt enable | `0` |
| 3 | `UNFLINT` | FIFO underflow interrupt enable | `0` |
| 4 | `WRPTRINC` | FIFO write-pointer increment enable | `1` (enabled) |
| 5 | `FIFOBRKINT` | FIFO break-point interrupt enable | `0` |

### 4.4 `CP_CLR` (0x02)

| Bit | Field | Description |
|---|---|---|
| 0 | `OVFLINT` | Clear the FIFO overflow interrupt |
| 1 | `UNFLINT` | Clear the FIFO underflow interrupt |
| 2 | `PERFCNT` | Clear the performance counters |

### 4.5 FIFO address / watermark registers

Each FIFO address, watermark, count or pointer value is split into a **low** and a
**high** register. The low register holds bits `[15:5]` (the upper 11 bits of the low
half) and the high register holds bits `[25:16]` (10 bits), yielding a **21-bit**
value in 32-byte units (address bits `[25:5]`); the low 5 address bits are zero. The
**count**, **watermark** and **pointer** values live in the same 32-byte units.

- `CP_FIFO_BASE` — where the CP starts reading the stream (main memory).
- `CP_FIFO_TOP` — the end of the stream region.
- `CP_FIFO_HICNT` / `CP_FIFO_LOCNT` — the high/low water marks (FIFO occupancy in
  32-byte units) used to flag overflow / underflow.
- `CP_FIFO_COUNT` — the current FIFO occupancy (the distance between the write and
  read pointers).
- `CP_FIFO_WPTR` / `CP_FIFO_RPTR` — the CP's write and read pointers.
- `CP_FIFO_BRK` — the read **break point** address.

### 4.6 Counters

`CP_COUNTER0..3` and the vertex-cache / frame-clock counters are performance counters
that count events selected in `CP_MEMPERF_SEL` (e.g. the number of command-FIFO
requests, object-call requests, vertex-cache misses, or total memory requests). The
frame-clock counter (`CP_FRCLK_CNT`) counts GFX clock cycles. These are debug/bring-up
aids.

## 5. Vertex data fetch

The command stream carries the *control*; the per-vertex attribute data can either be
inline (direct) or indexed from an **array** in main memory. How a vertex is composed
is described by two pieces of per-vertex state written through the command stream
(the "command-stream registers"): the **Vertex Command Descriptor (VCD)** and the
**Vertex Attribute Table (VAT)**.

### 5.1 Command-stream registers

These CP registers are written from the stream with `CP_LoadRegs` (address + 32-bit
data), not directly by the CPU:

| Address | Register | Description |
|---|---|---|
| `0x00` | `CP_VC_STAT_RESET` | Reset the vertex-cache statistics |
| `0x01` | `CP_STAT_ENABLE` | Enable vertex-cache statistics / frame-clock |
| `0x02` | `CP_STAT_SEL` | Select which attribute / stall to count |
| `0x03` | `CP_MATINDEX_A` | Matrix index for position/normal and tex0–tex3 |
| `0x04` | `CP_MATINDEX_B` | Matrix index for tex4–tex7 |
| `0x05` | `CP_VCD_LO` | Vertex Command Descriptor, low half |
| `0x06` | `CP_VCD_HI` | Vertex Command Descriptor, high half |
| `0x07` | `CP_VAT_A` | Vertex Attribute Table, group A |
| `0x08` | `CP_VAT_B` | Vertex Attribute Table, group B |
| `0x09` | `CP_VAT_C` | Vertex Attribute Table, group C |
| `0x0A` | `CP_ARRAY_BASE` | Base address of an attribute array |
| `0x0B` | `CP_ARRAY_STRIDE` | Stride of an attribute array |

### 5.2 Vertex Command Descriptor (VCD)

The VCD says which attributes a vertex carries and how each is sourced. `CP_VCD_LO`
covers the matrix indexes, position, normal and the two colours; `CP_VCD_HI` covers
the texture coordinates. Each attribute is one of:

| Value | Source |
|---|---|
| `0` | **not present** |
| `1` | **direct** — carried inline in the stream |
| `2` | **8-bit indexed** |
| `3` | **16-bit indexed** |

A missing attribute means the hardware uses the register default (e.g. the current
matrix index or a constant colour). The matrix-index attributes (position/normal and
each texture matrix) are always direct when present. The position and normal matrix
indexes are separate RAMs in the XF but share a one-to-one correspondence: if index
`A` is used for the position, index `A` is used for the normal as well.

### 5.3 Vertex Attribute Table (VAT)

For each present attribute, the VAT gives the **component count**, the **component
width/format** and a **fixed-point shift**. It is split across three registers
(`CP_VAT_A/B/C`). The component format for scalar/vector attributes is ubyte, byte,
ushort, short or float; for colours it is one of the packed RGB/RGBA formats. The
shift field is the position of the binary point from the LSB and applies to all
u/short components (and, when `ByteDequant` is set, to u/byte components).

`CP_VAT_A[32]` also carries `ByteDequant` (must be `1` for Rev B) and
`NormalIndex3` (whether nine normals use three staggered indexes). `CP_VAT_B[31]`
carries `VCacheEnhance` (must be `1`).

### 5.4 Arrays, base and stride

Indexed attributes are fetched from an **array** in main memory. The array base and
stride are set with `CP_ARRAY_BASE`/`CP_ARRAY_STRIDE`, which select the array via a
4-bit index:

| Index | Array |
|---|---|
| `0x0`–`0xB` | attribute array for position / normal / color0 / color1 / tex0…tex7 |
| `0xC` | XF index array A |
| `0xD` | XF index array B |
| `0xE` | XF index array C |
| `0xF` | XF index array D |

The address of an indexed attribute is:

```
MemoryAddress = ArrayBase[I] + index * ArrayStride[I]
```

where `ArrayBase` is 26-bit and `ArrayStride` is 8-bit (in bytes). An index value of
`0xFF` (8-bit) or `0xFFFF` (16-bit) skips that vertex.

### 5.5 Vertex cache

Attributes fetched from arrays are cached in an **8 KB, 8-way set-associative**
cache, so a vertex shared by several triangles is fetched from memory only once. The
cache is invalidated with the `V$_Invalidate` command. The vertex-cache miss / stall /
check counters (§4.6) report cache behaviour.

## 6. Emulator notes

1. **Two register spaces.** Model the *host* registers (reached at `0x0C00_0000`, 16-bit data) and the *command-stream* registers (written via `CP_LoadRegs` with an 8-bit address and a 32-bit value) separately.
2. **FIFO setup.** Model the CP read FIFO with `base`/`top` (in 32-byte units), a high/low water mark, and read/write pointers. The FIFO count is `wptr - rptr` (clamped to `[base, top]`).
3. **Linked vs multi-buffer.** In linked mode the Gekko PI-FIFO write and the CP read pointer advance together and the CP runs whenever the count is non-zero; in multi-buffer mode the program sets both pointers. Underflow/overflow flag the water-mark crossing and stop the CP reading.
4. **Break point.** When the CP read pointer reaches the break address, stop reading and raise the break interrupt (if enabled).
5. **Command decode.** Implement the opcode table in §3. For a draw, read the 16-bit vertex count, then for each vertex unpack the attributes per the current VAT/VCD.
6. **Indexed attributes.** Compute the array address with `base + index * stride`, and treat index `0xFF`/`0xFFFF` as a skip. Optionally model the 8 KB vertex cache (associative on the array base+index).
7. **Display lists.** `Call_Object` (address, 32-byte count) switches the CP to read from the object region; do not allow nesting.
8. **XF hand-off.** The assembled vertex stream (position, normal, colour, texture coordinates, per-attribute matrix indexes) is handed to the XF, which is the entry point of the graphics pipeline.
9. **Interrupt.** The CP raises a single interrupt sense (`CPINT`) in the PI; it is masked/cleared in the CP via `CP_ENABLE`/`CP_CLR` (`OVFLINT`/`UNFLINT`/`BRKINT`).

## 7. Related specifications

- [gfx.md](gfx.md) — the GFX (graphics) subsystem; the pipeline the CP feeds and the page that lists the CP as a separate command source.
- [flipper.md](flipper.md) — the Flipper chipset as a whole.
- [processor-interface.md](processor-interface.md) — the Gekko→Flipper bus, the PI FIFO and the CP register-space base.
- [memory-interface.md](memory-interface.md) — the arbitrated main-memory path the CP uses to read the stream, display lists and arrays.

## 8. References

- `HW/GraphicsSystem/GFX.md` — the CP FIFO registers, command format, vertex cache and VCD/VAT detail.
- `HW/GraphicsSystem/GFX_FIFO.png`, `GFX_Interconnect.png` — the FIFO and interconnect diagrams.
- `RE/GX/gxfifo.txt`, `RE/GX/scissor.txt` — GX FIFO and scissor notes.
- `RE/GX/GXPrivate.h`, `RE/GX/GXInit.c` — how the system library drives the CP.
- `HW/acronyms.txt` — CP, XF, VAT, VCD, TMEM, GFX glossary.
- **US Patent 6,717,577** (vertex cache) — the vertex-cache organisation.
- **US Patent 7,199,710** (GX FIFO) — the command-FIFO mechanism.
- The Flipper chip RTL is the authority for the CP register field meanings and FIFO behaviour; all facts above are summarised from it.
