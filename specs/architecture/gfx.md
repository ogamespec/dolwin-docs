# GameCube — GFX (Graphics) Subsystem

> **GFX** is the fixed-function graphics processor embedded in the **Flipper** ASIC.
> It is the engine that turns the command stream produced by the Gekko CPU into the
> image that the **Video Interface (VI)** scans out to a TV. The GFX has no
> programmable shaders: every stage is a dedicated block with a fixed (but
> register-configurable) function, and the whole pipeline runs in parallel with the
> CPU and the DSP.

This page is the **overview / index** for the GFX pipeline. Each stage below is
described at overview level and links out to the register-level specification that
covers it. Where a dedicated page does not yet exist the stage is summarised here so
that the corresponding document can later be hung off the relevant section.

The facts are drawn from the repository's hardware documentation, chip RTL and
reverse-engineering sources and are summarised (paraphrased) rather than reproduced.

## 1. Role and connection

Physically the GFX lives entirely inside the Flipper ASIC. The **entry point of all
graphics commands is the transform unit (XF)** — that is where the graphics pipeline
begins. The command and vertex data that the XF consumes is produced by the
**Command Processor (CP)**, which is a *separate* Flipper module (see §3), not a stage
of the graphics pipeline. The CPU never talks to the GFX blocks directly: it writes a
command stream to the **PI FIFO** at `0x0C008000` (from the uncached physical map),
the CP reads it, and the CP then feeds the XF. On its output side the GFX draws into
the on-chip **embedded framebuffer (EFB)**, optionally copies / filters / scales that
into an **external framebuffer (XFB)** in main memory, and the **Video Interface (VI)**
reads the XFB and drives the video encoder.

The blocks that move data over the main-memory interface on behalf of the graphics
path are:

| Master | What it reads / writes |
|---|---|
| **CP** | the command stream and display lists from main memory; vertex attribute arrays (a separate module — see §3) |
| **TC** (texture unit) | texture source data in main memory |
| **PE** (pixel engine) | EFB read/modify/write and the EFB→XFB copy |
| **VI** | the external framebuffer (XFB) for scan-out |

The **PI** is a master too (Gekko), but only for register access and the PI FIFO. The
arbitration, queueing and bandwidth of all of these is the job of the **Memory
Interface (MI)** — see [memory-interface.md](memory-interface.md).

## 2. Pipeline overview and index

The GFX pipeline proper is a **front end** (geometry) and a **back end**
(rasterization + per-pixel colour + frame buffer). Everything is fixed-function and
the data flows once, right to left in the block diagram. The **Command Processor (CP)**
sits to the left of the pipeline — it is a separate module that supplies the command
and vertex data, and the graphics commands enter the pipeline at the **XF**:

```
  (separate module — command source)                  (graphics pipeline — entry point = XF)
  Gekko ──PI FIFO──▶ CP ──▶ command + vertex data ──▶ XF ──▶ SU ──▶ RAS ──▶ TEV ──▶ PE (EFB) ──▶ Copy ──▶ XFB ──▶ VI
                                                       ▲                              ▲
                                                  matrix/light RAM             texture unit (TC/TMEM/TF)
```

| # | Subsystem | Block(s) | Role | Dedicated spec |
|---|---|---|---|---|
| 1 | Transform / geometry | **XF** | entry point of the graphics commands; vertex transform, lighting, texture-coordinate generation, clipping/culling | `gfx-transform` *(planned)* |
| 2 | Setup / rasterizer | **SU**, **RAS0/1/2**, **Bump** | Triangle setup and the three edge/texture/colour rasterizers, plus the bump-directive unit | `gfx-rasterizer` *(planned)* |
| 3 | Texture unit | **TC / TMEM / TF** | Texture memory & cache, texture address generation, LOD and filtering | `gfx-texture` *(planned)* |
| 4 | Texture environment | **TEV** | Per-pixel colour/alpha combine, fog, indirect & Z texturing | `gfx-tev` *(planned)* |
| 5 | Pixel engine | **PE** | Colour/Z compare & blend, the EFB, and the EFB→XFB copy / scale / format convert | `gfx-pixel-engine` *(planned)* |
| 6 | Video interface | **VI** | Reads the XFB, generates TV timing, horizontal scaler | [video-interface.md](video-interface.md) |

> The **Command Processor (CP)** is deliberately absent from the table above: it is a
> separate Flipper module that *produces* the graphics command / vertex stream, not a
> stage of the pipeline. It is summarised in §3 and specified in full (register-level)
> in [command-processor.md](command-processor.md). The `gfx-*` subsystem pages listed as
> *planned* do not exist yet; this page serves as their index. Each pipeline section
> below describes the corresponding stage, and the detailed register-level document can
> later be attached to it.

The GFX stage that turns the stored image into the on-screen signal is the **VI**,
which is specified separately in [video-interface.md](video-interface.md).

## 3. Command processor (CP) — the graphics command source

The **Command Processor (CP)** is a *separate* Flipper module. It is not a stage of
the GFX pipeline; instead it is the **source** of the graphics command and vertex
data. The CP reads the command stream, resolves display lists, fetches and caches
vertex attributes, and hands the resulting vertex stream to the pipeline. Its output
is consumed at the **XF**, which is the entry point for all graphics commands. The
register-level CP specification is in
[command-processor.md](command-processor.md).

The CP fetches three kinds of data from main memory through on-chip FIFOs, always in
**32-byte** chunks:

- **Command stream** — the graphics command list, read from main memory through an on-chip buffer FIFO.
- **Display lists** — read via a separate **call FIFO**; a display list is called from the stream but cannot call another (no nesting).
- **Vertex attributes** — either carried inline in the stream, or fetched from **vertex arrays** in main memory and cached.

### 3.1 Gekko → CP FIFOs

Two FIFOs arbitrate the flow of commands between the CPU and the CP (which then feeds
the GFX):

- **PI FIFO** — on the Gekko side. The CPU performs a burst store to `0x0C008000`, the write pointer advances by 32 bytes with wrap-around control (`CPBAS`, `CPTOP`, `CPWRT`, `CPABT`). The Gekko write-gather buffer (SPR `WPAR`) can be configured to this address so that single-beat writes are collected into a single 32-byte burst.
- **CP FIFO** — on the CP side. The CP reads the stream from main memory using its own pointers and the watermark/high–low counters (`CP_FIFO_BASE/TOP`, high/low water counts, read/write pointers, break point).

The CP FIFO runs in **linked mode** or **multi-buffer mode**. Linked mode couples the
Gekko's PI-FIFO write pointer to the CP read pointer so the CP starts work as soon as
the distance between them grows; multi-buffer mode requires the program to set the
read and write pointers itself.

### 3.2 FIFO command format

A command is a 32-bit word whose high bits select the operation and the low bits carry
the operand / vertex-attribute-table (`vat`) selector. Supported verbs include
*NOP*, the draw commands (*quads, triangles, triangle strip, triangle fan, lines, line
strip, points*), CP/XF register loads, index loads, `Call_Object` (display list),
`V$_Invalidate` and the setup-unit *bypass* commands. Vertex counts follow the draw
opcode in a 16-bit field.

### 3.3 Vertex cache

For indexed vertices the CP caches attribute data fetched from main memory in an
**8 KB, 8-way set-associative** vertex cache, invalidated on demand with
`V$_Invalidate`. How a vertex is composed is described by two pieces of per-vertex
state:

- **Vertex Command Descriptor (VCD)** — which attributes are present and whether they are direct or indexed, for position, normal, two colours and up to eight texture coordinates.
- **Vertex Attribute Table (VAT)** — for each attribute, the component count (`CompCount`), the component width and fixed-point shift (`CompSize`, `Shift`), so the CP knows how to unpack the attribute stream.

## 4. Transform / geometry (XF)

The XF takes one vertex at a time and performs object-space → **screen-space**
transformation, lighting, texture-coordinate generation (including projective
coordinates and bump mapping), and polygon clipping / culling. It uses **fixed-point**
arithmetic (20-bit fraction datapath) and a microcoded sequencer rather than a general
purpose ALU.

### 4.1 Matrix & light memory

| Memory | Organisation |
|---|---|
| Geometry / texture matrix RAM | 64 entries × 4 words (model-view & per-texture matrices) |
| Normal matrix RAM | 32 rows × 3 words (for lighting) |
| Dual-texture matrix RAM | 64 entries × 4 words (Rev B dual texture transform) |
| Light parameter RAM | 8 light records × 16 words |

### 4.2 Lighting

Up to **8** hardware lights. Each light record holds an RGBA colour, the three cosine
attenuation coefficients (`A0/A1/A2`), three distance-attenuation coefficients
(`K0/K1/K2`), a position (or, for an infinite light, a direction), and a half-vector
or spotlight direction. Per-vertex lighting combines the ambient, material and
vertex colours with the lit contributions, with a selectable diffuse attenuation
function and attenuation enable/select, and supports **toon** shading. Two independent
colour channels (**colour0**, **colour1**) can be lit separately.

### 4.3 Texture-coordinate generation

Each of the up to 8 texture coordinates can be generated one of three ways: a regular
**matrix transform** of an incoming source row, a **bump-map** (embossed) form that
perturbs the coordinate from the output of the bump unit and a light vector, or a
**colour** form where the coordinate is taken from the red/green (and blue) of a
per-vertex colour. A *dual texture transform* (Rev B) applies a second matrix to the
same coordinate.

### 4.4 Clipping / culling

The XF rejects or clips geometry against the view volume. Clipping can be selectively
disabled via a control register (clip detection, trivial rejection and the
polygon-clipping acceleration are each independent).

## 5. Setup & rasterization (SU / RAS / Bump)

### 5.1 Setup unit (SU)

The SU receives transformed vertices from the XF and produces the setup information
the rasterizers need: screen-space scissor (two corners), line & point size (with a
selectable texture-offset for lines and points), the triangle parameters, and the
per-stage texture-size and "s/t" scale/shift controls. It also has a performance counter
block and the 24-bit **sub-sample mask** that controls multi-sampling coverage.
Fields in the shared **GEN_MODE** register select the number of texture coordinates,
colour values, TEV stages, bump stages, flat shading, multi-sampling and front/back
rejection that the rest of the pipeline should expect.

### 5.2 Rasterizers (RAS0/1/2)

Three parallel rasterizer blocks walk the pixels of each primitive:

- **RAS0** — edge (X/Y) rasterization, producing the per-pixel coordinates.
- **RAS1** — texture-coordinate rasterization, generating interpolated `s/t` (optionally `q`) for each texture stage, with per-stage coordinate **shifts** and the **indirect-texture reference** map.
- **RAS2** — colour rasterization, generating the interpolated per-vertex colours.

The **Bump** unit runs ahead of the texture unit and produces the bump-matrix /
directive information used by RAS1 for bump-mapped texture coordinates, and the
texture coordinate-generation of the XF. Supported primitives are points, lines,
line strips, triangles, triangle strips, triangle fans (and quads, assembled by the
host or the CP).

## 6. Texture unit (TC / TMEM / TF)

### 6.1 Texture memory (TMEM)

TMEM is **512 KB** of fast embedded memory on the Flipper die. It is the working store
for texture images and palette (TLUT) data; a texture is loaded into TMEM from main
memory, or an EFB region can be *copied* back into TMEM to be re-used as a texture.
The **tmem_offset** fields in the load / image registers are 15-bit word addresses,
i.e. the whole 512 KB is word-addressable.

Textures can be up to **1024 × 1024**. Supported texture formats include an indexed
set (CI4 / CI8 / CI14 with a TLUT) plus unpacked formats (I4, I8, IA4, IA8, RGB565,
RGB5A3, RGBA8) and a native compressed format (S3TC / CMPR, 4×4 blocks). Palettes
(TLUTs) are stored as IA8, RGB565 or RGB5A3.

### 6.2 Texture cache & addressing

The texture cache unit (TC) generates the texture address from the interpolated
coordinates, handles LOD (level of detail) selection, and **cache** lookups. It
supports wrap modes (clamp-to-edge / repeat / mirror), min/mag filters (point vs
linear, plus mip-mapped variants), a LOD bias, an optional maximum anisotropy
(1× / 2× / 4×), optional LOD clamping, and the **EFB copy as texture** path. The
cache is tag-managed and can be invalidated; a reload/refresh mechanism keeps the
texture memory up to date.

### 6.3 Texture filter (TF)

The TF performs the actual bilinear / trilinear (and anisotropic) filtering of the
texels fetched from TMEM, and the address-dependent "indirect" look-up that feeds the
TEV. Because the GFX is fixed-function, the filter modes are selected by the texture
environment / texture-unit registers rather than by a shader.

## 7. Texture environment (TEV)

The TEV is the per-pixel **colour & alpha combine** stage — the "pixel shader
equivalent" of the GFX. It runs **up to 16 independent stages**, each of which
computes a colour and an alpha result from up to four inputs (previous stage result,
texel, rasterized colour, a constant) using a selectable operator:

- Inputs are selected from the four colour registers (`cc0`–`cc3`), the two colour channels (`txc`/`txa`), the rasterized colour, or constants.
- Operator: bias (±0.5, 0), add / subtract, clamp to either range, and a left/right shift.
- Alpha combines work the same way, with a comparison mode and a component swap.
- The result feeds **fog** (none / linear / exponential / exponential-squared / backward-exponential / backward-exponential-squared), **indirect** texturing, and a **constant** colour register bank used as the "pixel" colour. A **Z texture** stage can add to or replace the reference Z with a texel value, and a **range adjustment** supports the 16-bit colour mode (Rev B).
- Final per-pixel logic: alpha function (per-channel compare + AND/OR/XOR/XNOR), so the TEV can implement the classic opacity/alpha tests without a depth read.

The number of active TEV stages, the number of bump stages and the flat-shading /
multi-sample mode are all set in the shared **GEN_MODE** register.

## 8. Pixel engine (PE) & embedded framebuffer (EFB)

The PE is the last stage of the geometry path and owns the on-chip **EFB**.

### 8.1 Z & colour compare

For every fragment the PE compares against the existing depth and colour. The Z
function (never / less / equal / less-equal / greater / not-equal / greater-equal /
always), Z mask, and the Z **format** (linear, or the reversed formats used for
early-Z / 16-bit depth) are set by `PE_ZMODE`/`PE_CONTROL`. The colour path supports
**blend** (source/dest factors, add/subtract, constant alpha), a **logic operation**
(all 16 standard raster ops), colour & alpha write masks, and dithering. The EFB pixel
type (RGB8, RGBA6, RGB_AA, Z, or a YUV output form for the copy path) is selected in
the control register.

### 8.2 Embedded frame buffer (EFB)

The EFB is a **2 MB** on-chip frame buffer with a native resolution of about
**640 × 528**. All drawing lands here. It is a fast 1T-SRAM-style SRAM on the die (not
main memory) and is the only place rasterization output is written. It supports
multi-sampling / anti-aliasing (a quad centred on the pixel, samples configured in the
`GEN_MSLOCn` registers), an **early-Z** / fast-Z path, and per-field masking for
interlaced rendering.

### 8.3 EFB → XFB copy

The PE **copy** engine (PEC) moves the EFB to an external framebuffer (XFB) in main
memory. It can:

- select the source rectangle and the destination base/stride,
- **scale** the image,
- convert formats (RGB8 / RGBA6 / RGB·AA, or YUV / YUV420 for the video path), and
- either **copy to display** (a clean framebuffer) or **copy to texture** (back into TMEM for re-use), with optional clear colour / Z, a vertical filter, gamma correction and interlaced/odd-even field selection.

The copy uses the 32-byte cache-line main-memory interface for its writes.

## 9. Video interface (VI)

The VI is the interface between the GFX output and the TV encoder. It reads the XFB
from main memory, unpacks the YUV 4:2:2 data, optionally runs it through a horizontal
resampler / anti-alias filter, and clocks it out to the digital encoder with all the
NTSC / PAL / M-PAL synchronisation and blanking. It also provides the vertical
retrace interrupt, the light-gun position latch and the 480p progressive mode.

The VI is specified separately and in full at the register level in
[video-interface.md](video-interface.md) — see that page for the complete register
map, timing tables and emulator model.

## 10. State & register organisation

The pipeline state is written through the FIFO command stream (the CPU only touches it
via the PI FIFO / display-list commands). It is held in:

- **CP registers** — the control of the (separate) command processor: FIFO pointers, watermark, vertex format, array base/stride, matrix index. Partially mirrored into the CPU-resident register map. (See §3.)
- **XF registers** — the matrix / light memory and all the transform, lighting and texture-coordinate controls of the pipeline's first stage.
- **Bypass (BP) registers** — the bulk of the pipeline stage registers, so called because they are written *bypassing* the XF. They are organised into groups: GEN (shared), SU/RAS, PE, and the texture (TX) and TEV groups, plus the pixel-emitter "finish" registers.

The GFX has no general-purpose memory-mapped register window of its own; the address
that the CPU uses (`0x0C000000` CP, `0x0C001000` PE, `0x0C002000` VI, `0x0C003000` PI,
`0x0C004000` MI, ...) exposes the parts of the GFX that the Gekko must reach directly,
while the rest of the pipeline state is driven purely through the command FIFO.

## 11. Key parameters (summary)

| Parameter | Value |
|---|---|
| Pipeline | fixed-function (no programmable shaders); microcoded transform sequencer |
| Command interface | PI FIFO → command processor (CP, a separate module) → **XF** (entry point); CP FIFO + display-list call FIFO |
| Vertex cache | 8 KB, 8-way set associative |
| Transform | fixed-point; 8 lights; 64×4 + 32×3 + 64×4 matrix RAM; up to 8 texture coords |
| Rasterization | 3 rasterizers (edge / texture-coord / colour); points, lines, triangles, strips, fans, quads |
| Texture memory (TMEM) | 512 KB; max texture 1024×1024; S3TC/CMPR, CI/I/IA/RGB565/RGB5A3/RGBA8 + TLUT |
| Texture filter | point / linear / mip-mapped; anisotropic up to 4×; wrap clamp/mirror/repeat |
| TEV stages | up to 16 colour & alpha combine stages; fog; indirect & Z texturing |
| Pixel engine | 24-bit Z & 24-bit RGB; blend + logic ops + alpha function; multi-sample / AA |
| Embedded framebuffer (EFB) | 2 MB on-chip; native ≈ 640×528; copy→XFB (display or texture) |
| External framebuffer (XFB) | in main memory, YUV 4:2:2, read by the VI |
| Video output | via the VI to an external encoder; NTSC / PAL / M-PAL / 480p |
| Throughput | ≈ 6–12 M polygons/s under real game conditions |

## 12. Related specifications

- [flipper.md](flipper.md) — the Flipper chipset as a whole (northbridge, GPU, DSP, I/O).
- [command-processor.md](command-processor.md) — the Flipper module that produces the graphics command / vertex stream (§3).
- [processor-interface.md](processor-interface.md) — how the Gekko reaches the GFX (PI FIFO, register window, memory map).
- [memory-interface.md](memory-interface.md) — the arbitrated main-memory path the GFX masters use.
- [video-interface.md](video-interface.md) — the VI scan-out stage of the pipeline.
- [clock-generator.md](clock-generator.md) — the clock domains the GFX runs in.

## 13. References

- `HW/Flipper_ASIC_Block_Diagram.png` — Flipper internal block diagram.
- `HW/GraphicsSystem/GFX.md` — GFX pipeline detail (command processor, XF, setup, rasterizer, TEV, PE register space).
- `HW/GraphicsSystem/GFX_Interconnect.png`, `GFX_Internal.png`, `GFX_FIFO.png`, `GFX_Primitives.png` — GFX interconnect, internal architecture, FIFO and primitive diagrams.
- `HW/GraphicsSystem/XF_Block_Diagram.png` — the transform-unit block diagram.
- `RE/Flipper_ASIC/flipper_floorplan.jpg` — Flipper die floor plan showing the size of each GFX area.
- US Patents 6,466,218 (Graphics Interface), 6,717,577 (Vertex Cache), 7,199,710 (GX FIFO), 6,937,245 / 6,999,100 (PE EFB / copy), 6,619,804 (Z-clamping), 6,636,214 (Hidden surface processing), 6,709,458 (Indirect Textures), 6,825,851 (Bump Mapping).
- `HW/acronyms.txt` — GFX, GX, EFB, XFB, TMEM, CP, XF, SU, RAS, TEV, PE glossary.
