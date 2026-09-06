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
the eight texture coordinates. Each 2-bit field uses the same source encoding:

| Value | Source |
|---|---|
| `0` | **not present** |
| `1` | **direct** — carried inline in the stream |
| `2` | **8-bit indexed** |
| `3` | **16-bit indexed** |

`CP_VCD_LO` bit layout:

| Bits | Field | Meaning |
|---|---|---|
| 0 | `PosMatIdx` | Position/normal matrix index present (0/1, always direct) |
| 1 | `Tex0MatIdx` | Tex0 matrix index present |
| 2..8 | `Tex1MatIdx`..`Tex7MatIdx` | Tex1..Tex7 matrix index present |
| 10:9 | `Position` | Position source (2-bit) |
| 12:11 | `Normal` | Normal source |
| 14:13 | `Color0` | Colour0 source |
| 16:15 | `Color1` | Colour1 source |

`CP_VCD_HI` bit layout:

| Bits | Field | Meaning |
|---|---|---|
| 1:0 | `Tex0Coord` | Tex0 source |
| 3:2 | `Tex1Coord` | Tex1 source |
| 5:4 | `Tex2Coord` | Tex2 source |
| 7:6 | `Tex3Coord` | Tex3 source |
| 9:8 | `Tex4Coord` | Tex4 source |
| 11:10 | `Tex5Coord` | Tex5 source |
| 13:12 | `Tex6Coord` | Tex6 source |
| 15:14 | `Tex7Coord` | Tex7 source |

A missing attribute means the hardware uses the register default (e.g. the current
matrix index or a constant colour). The matrix-index attributes (position/normal and
each texture matrix) are always direct when present. The position and normal matrix
indexes are separate RAMs in the XF but share a one-to-one correspondence: if index
`A` is used for the position, index `A` is used for the normal as well.

### 5.3 Vertex Attribute Table (VAT)

For each present attribute the VAT gives the **component count**, the **component
width/format** and a **fixed-point shift**. One VAT covers one *attribute format* and
is selected per draw by the low 3 bits of the draw opcode (the `vat` selector), so up
to 8 VATs can be active. It is split across three registers — `CP_VAT_A`, `CP_VAT_B`
and `CP_VAT_C`.

**Component format** (`CompSize`) for scalar/vector attributes (position, normal,
texture coordinate):

| Value | Format |
|---|---|
| `0` | ubyte |
| `1` | byte |
| `2` | ushort |
| `3` | short |
| `4` | float |
| `5`–`7` | reserved |

**Colour format** (`CompSize`) for the two colour channels:

| Value | Format | Components |
|---|---|---|
| `0` | RGB565 | 3 (r,g,b) |
| `1` | RGB888 | 3 |
| `2` | RGB888x | 3 |
| `3` | RGBA4444 | 4 |
| `4` | RGBA6666 | 4 |
| `5` | RGBA8888 | 4 |

**Component count** (`CompCount`, 1 bit) selects 2 vs 3 components for position, 1 vs
2 for a texture coordinate, 3 vs 9 for a normal, and 3 vs 4 for a colour.

`CP_VAT_A` (attribute group A — position, normal, colors, tex0):

| Bits | Field | Meaning |
|---|---|---|
| 0 | `PosCnt` | Position components: 0 = (x,y), 1 = (x,y,z) |
| 3:1 | `PosFmt` | Position component format |
| 8:4 | `PosShft` | Position fixed-point shift |
| 9 | `NrmCnt` | Normal count: 0 = 3, 1 = 9 |
| 12:10 | `NrmFmt` | Normal component format |
| 13 | `Col0Cnt` | Colour0 components: 0 = RGB, 1 = RGBA |
| 16:14 | `Col0Fmt` | Colour0 format |
| 17 | `Col1Cnt` | Colour1 components |
| 20:18 | `Col1Fmt` | Colour1 format |
| 21 | `Tex0Cnt` | Tex0 components: 0 = (s), 1 = (s,t) |
| 24:22 | `Tex0Fmt` | Tex0 component format |
| 29:25 | `Tex0Shft` | Tex0 fixed-point shift |
| 30 | `ByteDequant` | Rev B: the shift applies to u/byte and u/short components |
| 31 | `NormalIndex3` | Rev B: nine normals use three staggered indexes |

`CP_VAT_B` (attribute group B — tex1..tex4):

| Bits | Field | Meaning |
|---|---|---|
| 0 | `Tex1Cnt` | Tex1 components |
| 3:1 | `Tex1Fmt` | Tex1 format |
| 8:4 | `Tex1Shft` | Tex1 shift |
| 9 | `Tex2Cnt` | Tex2 components |
| 12:10 | `Tex2Fmt` | Tex2 format |
| 17:13 | `Tex2Shft` | Tex2 shift |
| 18 | `Tex3Cnt` | Tex3 components |
| 21:19 | `Tex3Fmt` | Tex3 format |
| 26:22 | `Tex3Shft` | Tex3 shift |
| 27 | `Tex4Cnt` | Tex4 components |
| 30:28 | `Tex4Fmt` | Tex4 format |
| 31 | `VCacheEnhance` | Must be `1` |

`CP_VAT_C` (attribute group C — tex4 shift, tex5..tex7):

| Bits | Field | Meaning |
|---|---|---|
| 4:0 | `Tex4Shft` | Tex4 shift |
| 5 | `Tex5Cnt` | Tex5 components |
| 8:6 | `Tex5Fmt` | Tex5 format |
| 13:9 | `Tex5Shft` | Tex5 shift |
| 14 | `Tex6Cnt` | Tex6 components |
| 17:15 | `Tex6Fmt` | Tex6 format |
| 22:18 | `Tex6Shft` | Tex6 shift |
| 23 | `Tex7Cnt` | Tex7 components |
| 26:24 | `Tex7Fmt` | Tex7 format |
| 31:27 | `Tex7Shft` | Tex7 shift |

The **shift** is the position of the binary point counted from the LSB and applies to
all u/short components (and, when `ByteDequant` is set, to u/byte components). It is
how fixed-point vertex data is scaled to the hardware's internal fractional format.
`VCacheEnhance` and `ByteDequant`/`NormalIndex3` are Rev-B fields that must be set to
`1` when using the corresponding features.

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

Attributes fetched from an indexed array are cached so a vertex that is shared by
several primitives is read from main memory only once. The cache is the subject of a
dedicated patent (**US 6,717,577**, "Vertex Cache for 3D Computer Graphics"), which is
the authority for its design.

#### 5.5.1 Organization

The cache is a small, low-latency on-chip memory local to the CP, built as a
**512 × 128-bit dual-ported RAM** organised as an **8-way set-associative** cache
(8 KB total, 64 sets of 8 ways). It is *not* a general-purpose cache: the tags are
customised to deliver vertex data to the graphics engine, and each cache line holds
the packed attribute data for an indexed vertex (or part of it).

#### 5.5.2 Why it exists

The cache exists to exploit the **temporal locality** of vertex data. A primitive list
references vertices by index, and a given vertex is usually referenced again within a
few adjacent primitives. Because the vertex data itself is stored in main memory as
separate component *arrays* (position, normal, colour, texture coordinate), the cache
lets the CP build each vertex on the fly and keep recently used ones resident, instead
of re-fetching them from memory for every reference. This removes the need either to
pre-sort the vertex data in display order or to re-specify each polygon each time it
is used.

#### 5.5.3 Access, fill and invalidation

- The vertex stream parser addresses the cache **as if it were the whole of main memory**, keyed by the vertex index and the attribute-array base. On a hit the packed attribute data is returned immediately; on a **miss** the CP fetches the relevant block(s) from the array in main memory (typically via DMA) and fills the line, often **prefetching** the next vertex to hide the memory latency.
- Because the cached data is still in its packed, quantised form, it is passed through an **inverse-quantizer** before use, which converts any of the supported vertex formats (8-bit / 16-bit fixed-point, or float) into the uniform float representation that the graphics pipeline consumes.
- The cache is invalidated on demand with the `V$_Invalidate` command, which flushes the tags so the next indexed access re-fetches from memory.
- The check / miss / stall counters (§4.6) report cache behaviour: how many accesses were checked, how many missed, and how many stalled waiting for a fill.

#### 5.5.4 The indexed data it serves

The cache serves the *indexed* representation, which is built from three levels:

- a **primitive list** — the sequence of primitives (triangles, quads, strips, …), each described by a list of vertex indices;
- a **vertex list** — the indexed vertices those primitives reference, where a single vertex may be used by several primitives;
- the **component arrays** in main memory (position, normal, two colours, up to eight texture coordinates), each addressed by an index and the per-attribute base/stride of §5.4.

A **vertex descriptor** (the VCD/VAT of §5.2/§5.3) defines, for the whole draw, which
attributes are present, their component count and size, and whether each is direct or
indexed. Every vertex in a draw uses the same vertex-attribute format, so the CP can
unpack, inverse-quantise and assemble the stream uniformly.

## 6. Emulator notes

1. **Two register spaces.** Model the *host* registers (reached at `0x0C00_0000`, 16-bit data) and the *command-stream* registers (written via `CP_LoadRegs` with an 8-bit address and a 32-bit value) separately.
2. **FIFO setup.** Model the CP read FIFO with `base`/`top` (in 32-byte units), a high/low water mark, and read/write pointers. The FIFO count is `wptr - rptr` (clamped to `[base, top]`).
3. **Linked vs multi-buffer.** In linked mode the Gekko PI-FIFO write and the CP read pointer advance together and the CP runs whenever the count is non-zero; in multi-buffer mode the program sets both pointers. Underflow/overflow flag the water-mark crossing and stop the CP reading.
4. **Break point.** When the CP read pointer reaches the break address, stop reading and raise the break interrupt (if enabled).
5. **Command decode.** Implement the opcode table in §3. For a draw, read the 16-bit vertex count, then for each vertex unpack the attributes per the current VAT/VCD.
6. **Indexed attributes.** Compute the array address with `base + index * stride`, and treat index `0xFF`/`0xFFFF` as a skip. Model the 8 KB vertex cache as an 8-way set-associative table keyed on (array base, index); on a miss fetch+fill the line (optionally prefetching), and invalidate it on `V$_Invalidate`. Feed the fetched packed data through an inverse-quantiser to the XF.
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
- **US Patent 6,717,577** (vertex cache) — the 512 × 128-bit dual-ported RAM, the 8-way set-associative organisation, the customised tags, the on-demand/prefetch fill and the inverse-quantiser (§5.5).
- **US Patent 7,199,710** (GX FIFO) — the command-FIFO mechanism.
- The Flipper chip RTL is the authority for the CP register field meanings and FIFO behaviour; all facts above are summarised from it.
