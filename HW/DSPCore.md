# GAMECUBE DSP Core

All previous documents considered the DSP core inseparably from its hardware connection (Mailbox, Accelerator). This document concentrates specifically on the DSP processor part, without specifying which DSP core interrupts are involved, for example, for the Accelerator, or how hardware registers are mapped to the DSP core memory.

## Registers

|Index|Name|Meaning|
|---|---|---|
|0|r0|Auxiliary register 0 (circular addressing)|
|1|r1|Auxiliary register 1 (circular addressing)|
|2|r2|Auxiliary register 2 (circular addressing)|
|3|r3|Auxiliary register 3 (circular addressing)|
|4|m0|Modifier value 0 (circular addressing)|
|5|m1|Modifier value 1 (circular addressing)|
|6|m2|Modifier value 2 (circular addressing)|
|7|m3|Modifier value 3 (circular addressing)|
|8|l0|Buffer length 0 (circular addressing)|
|9|l1|Buffer length 1 (circular addressing)|
|10|l2|Buffer length 2 (circular addressing)|
|11|l3|Buffer length 3 (circular addressing)|
|12|pcs|Program counter stack|
|13|pss|Program status stack|
|14|eas|End address stack|
|15|lcs|Loop count stack|
|16|a2|40-bit accumulator `a` high 8 bits|
|17|b2|40-bit accumulator `b` high 8 bits|
|18|dpp|Used as high 8-bits of address for some load/store instructions|
|19|psr|Program status register|
|20|ps0|Product partial sum low part|
|21|ps1|Product partial sum middle part|
|22|ps2|Product partial sum high part (8 bits)|
|23|pc1|Product partial carry 1 middle part|
|24|x0|ALU/Multiplier input operand `x` low part|
|25|y0|ALU/Multiplier input operand `y` low part|
|26|x1|ALU/Multiplier input operand `x` high part|
|27|y1|ALU/Multiplier input operand `y` high part|
|28|a0|40-bit accumulator `a` low 16 bits|
|29|b0|40-bit accumulator `b` low 16 bits|
|30|a/a1|40-bit accumulator `a` middle 16 bits / Whole `a` accumulator|
|31|b/b1|40-bit accumulator `b` middle 16 bits / Whole `b` accumulator|

Not all registers are associated with simple DFF stores. Reading/writing some of them triggers the work of internal circuits, and some are freaked out by themselves. Specifically:
- Stack registers implement a transparent FIFO mechanism for implementing hardware stacks
- The result of the multiplication is stored as a set of partial sums and carry. Upon request (in some instructions) these registers are "folded" into the full result, as written in the Duddie document. Why is this done? Well, most likely this is how the multiplier is implemented in HDL (it's easier to store intermediate results). The exact algorithm of the multiplier is unknown, but you can guess that these temporary results are collected from partial sums of multiplications between the upper and lower halves of the 16-bit operands.
- Registers a1/b1 have a special logic of operation associated with the PSR.XL bit.

## Processor Status Register (PSR) bits

|Name|Mask|Meaning|
|---|---|---|
|C|0x0001|Carry|
|V|0x0002|Overflow|
|Z|0x0004|Zero|
|N|0x0008|Negative|
|E|0x0010|Extension|
|U|0x0020|Unnormalization|
|TB|0x0040|Test bit (btstl/btsth instructions)|
|SV|0x0080|Sticky overflow. Set together with the V overflow bit, can only be cleared by the CLRB instruction.|
|TE0|0x0100|Interrupt enable 0|
|TE1|0x0200|Interrupt enable 1|
|TE2|0x0400|Interrupt enable 2|
|TE3|0x0800|Interrupt enable 3|
|ET|0x1000|Global interrupt enable|
|IM|0x2000|Integer/fraction mode. 0: fraction mode, 1: integer mode. In fraction mode, the output of the multiplier is shifted left 1 bit to remove the sign.|
|XL|0x4000|Extension limit mode. Affects the loading and saving of a/b operands. See below.|
|DP|0x8000|Double precision mode. Affects mixed multiply (xxxMPY) instructions. When DP = 1, some of the operands of these instructions are signed and some are unsigned. Used to efficiently multiply two 32-bit signed numbers.|

## XL Mode

XL=0: Operands a/b are treated as registers a1/b1.

XL=1:
- Load a/b: The data is loaded into registers a1/b1 and sign extended into registers a2/b2.
- Store a/b: If (V | E) != 0 a/b output is clamped to 0x7fff/0x8000 according to sign.

This feature is used in DSP processing, for example, for clamping samples (e.g. one of the ADPCM stages).

## Circular Addressing

TBD.

## Stack operation

TBD.

## Condition code

Used in conditional flow control instructions.

|cc|Name|Meaning|
|---|---|---|
|0b0000|ge|N ^ V == 0|
|0b0001|lt|N ^ V != 0|
|0b0010|gt|Z \| (N ^ V) == 0|
|0b0011|le|Z \| (N ^ V) != 0|
|0b0100|nz|Z == 0|
|0b0101|z|Z != 0|
|0b0110|nc|C == 0|
|0b0111|c|C != 0|
|0b1000|ne|E == 0|
|0b1001|e|E != 0|
|0b1010|nm|Z \| (~U & ~E) == 0|
|0b1011|m|Z \| (~U & ~E) != 0|
|0b1100|nt|TB == 0|
|0b1101|t|TB != 0|
|0b1110|v|V != 0|
|0b1111| |Uncoditionally (always)|

## Interrupt vectors

|Address|Name|
|---|---|
|0x0000|Reset (highest prority)|
|0x0002|Error|
|0x0004|Trap|
|0x0006|Accelerator read start (ACRS) (enabled by TE1)|
|0x0008|Accelerator write end (ACWE) (enabled by TE1)|
|0x000A|Decoder read end (DCRE) (enabled by TE1)|
|0x000C|AI DMA interrupt (enabled by TE2)|
|0x000E|CPU->DSP interrupt (enabled by TE3) (lowest priority)|

ACRS, ACWE and DCRE are shared by single interrupt enable bit (TE1).

## Flag modify rules

- C1: `(Ds(39) & S(39)) | (~Dd(39) & (Ds(39) | S(39)) )`
- C2: `(Ds(39) & ~S(39)) | (~Dd(39) & (Ds(39) | ~S(39)) )`
- C3: `Ds(39) ^ S(15) != 0 ? (Ds(39) & S(15)) | (~Dd(39) & (Ds(39) | S(15)) ) : (Ds(39) & ~S(15)) | (~Dd(39) & (Ds(39) | ~S(15)) )`
- C4: `~Ds(39) & ~Dd(39)`
- C5: `Ds(39) ^ S(31) == 0 ? (Ds(39) & S(39)) | (~Dd(39) & (Ds(39) | S(39)) ) : (Ds(39) & ~S(39)) | (~Dd(39) & (Ds(39) | ~S(39)) )`
- C6: `Ds(39) & ~Dd(39)`
- C7: `P(39) & ~D(39)`
- C8: `(P(39) & S(39)) | (~D(39) & (P(39) | S(39)) )`
- V1: `(Ds(39) & S(39) & ~Dd(39)) | (~Ds(39) & ~S(39) & Dd(39))`
- V2: `(Ds(39) & ~S(39) & ~Dd(39)) | (~Ds(39) & S(39) & Dd(39))`
- V3: `Ds(39) & Dd(39)`
- V4: `~Ds(39) & Dd(39)`
- V5: `Dd(39)`
- V6: `~P(39) & D(39)`
- V7: `(P(39) & S(39) & ~D(39)) | (~P(39) & ~S(39) & D(39))`
- V8: `Ds(39) & ~Dd(39)`
- Z1: `Dd == 0`
- Z2: `Dd(31-16) == 0`
- Z3: `Dd(39-0) == 0`
- N1: `Dd(39)`
- N2: `Dd(31)`
- E1: `Dd(39-31) != (0b0'0000'0000 || 0b1'1111'1111)`
- U1: `~( Dd(31) ^ Dd(30) )`

## Regular Instructions

|Syntax|Encoding|C|V|Z|N|E|U|Operation|Cycles|
|---|---|---|---|---|---|---|---|---|---|
|jmp(cc) ta|0000 0010 1001 cccc aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Jump conditionally address|2: cc true, 3: cc false|
|jmpr(cc) rn|0001 0111 0rr0 cccc|-|-|-|-|-|-|Jump conditionally rn|2|
|call(cc) ta|0000 0010 1011 cccc aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Call conditionally address|2: cc true, 3: cc false|
|callr(cc) rn|0001 0111 0rr1 cccc|-|-|-|-|-|-|Call conditionally rn|2|
|rets(cc)|0000 0010 1101 cccc|-|-|-|-|-|-|Return conditionally|2|
|reti(cc)|0000 0010 1111 cccc|\*|\*|\*|\*|\*|\*|Return interrupt conditionally (PSR reloaded)|2|
|trap|0000 0000 0010 0000|-|-|-|-|-|-|Trap|3|
|wait|0000 0000 0010 0001|-|-|-|-|-|-|Wait any interrupt|Any|
|exec(cc)|0000 0010 0111 cccc|-|-|-|-|-|-|Execute next instruction conditionally|1|
|loop lc,ea|0001 0001 cccc cccc aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Loop by immeditate until end address|2|
|loopr reg,ea|0000 0000 011r rrrr aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Loop by reg until end address|2|
|rep rc|0001 0000 cccc cccc|-|-|-|-|-|-|Repeat next instruction by immediate|1|
|repr reg|0000 0000 010r rrrr|-|-|-|-|-|-|Repeat next instruction by register|1|
|pld d,rn,mn|0000 001d 0001 mmrr|-|-|-|-|-|-|Load from IMEM|3|
|nop|0000 0000 0000 0000|-|-|-|-|-|-|No operation|1|
|mr rn,mn|0000 0000 000m mmrr|-|-|-|-|-|-|Modify aux. register (non-parallel)|1|
|adsi d,si|0000 010d iiii iiii|C1|V1|Z1|N1|E1|U1|Add short immediate|1|
|adli d,li|0000 001d 0000 0000 iiii iiii iiii iiii|C1|V1|Z1|N1|E1|U1|Add long immediate|2|
|cmpsi s,si|0000 011s iiii iiii|C2|V2|Z1|N1|E1|U1|Compare short immediate|1|
|cmpli s,li|0000 001s 1000 0000 iiii iiii iiii iiii|C2|V2|Z1|N1|E1|U1|Compare long immediate|2|
|lsfi d,si|0001 010d 0iii iiii|0|0|Z1|N1|E1|U1|Logic shift immediate (directed by sign)|1|
|asfi d,si|0001 010d 1iii iiii|0|0|Z1|N1|E1|U1|Arithmetic shift immediate (directed by sign)|1|
|xorli d,li|0000 001d 0010 0000 iiii iiii iiii iiii|0|0|Z2|N2|E1|U1|Xor long immediate|2|
|anli d,li|0000 001d 0100 0000 iiii iiii iiii iiii|0|0|Z2|N2|E1|U1|And long immediate|2|
|orli d,li|0000 001d 0110 0000 iiii iiii iiii iiii|0|0|Z2|N2|E1|U1|Or long immediate|2|
|norm d,rn|0000 001d 0000 01rr|0|0|Z1|N1|E1|U1|Normalize step|1|
|div d,s|0000 001d 0ss0 1000|C3|0|Z3|N1|E1|U1|Division step|1|
|addc d,s|0000 0001 d10s 0110|C1|V1|Z1|N1|E1|U1|Add with carry|1|
|subc d,s|0000 001d 10s0 1101|C2|V2|Z1|N1|E1|U1|Sub with carry|1|
|negc d|0000 001d 0000 1101|C4|V3|Z1|N1|E1|U1|Negate with carry|1|
|max d,s|0000 001d 0ss0 1001|C5|0|Z1|N1|E1|U1|Max|1|
|lsf d,s|0000 001d 01s0 1010|0|0|Z1|N1|E1|U1|Logic shift directed by sign (Form 1)|1|
|lsf d,s|0000 001d 1100 1010|0|0|Z1|N1|E1|U1|Logic shift directed by sign (Form 2)|1|
|asf d,s|0000 001d 01s0 1011|0|0|Z1|N1|E1|U1|Arithmetic shift directed by sign (Form 1)|1|
|asf d,s|0000 001d 1100 1011|0|0|Z1|N1|E1|U1|Arithmetic shift directed by sign (Form 2)|1|
|ld d,r,m|0001 100m mrrd dddd|-|-|-|-|-|-|Load from DMEM|1|
|lda d,r,m|0010 1011 mmrd dddd|-|-|-|-|-|-|Load from DMEM by accumulator|1|
|st r,m,s|0001 101m mrrs ssss|-|-|-|-|-|-|Store to DMEM|1|
|sta r,m,s|0010 1111 mmrs ssss|-|-|-|-|-|-|Store to DMEM by accumulator|1|
|ldsa d,sa|0010 0ddd aaaa aaaa|-|-|-|-|-|-|Load from DMEM by short address|1|
|stsa sa,s|0010 1sss aaaa aaaa|-|-|-|-|-|-|Store to DMEM by short address|1|
|ldla d,la|0000 0000 110d dddd aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Load from DMEM by long address|2|
|stla la,s|0000 0000 111s ssss aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Store to DMEM by long address|2|
|mv d,s|0001 11dd ddds ssss|-|-|-|-|-|-|Move register (non-parallel)|1|
|mvsi d,si|0000 1ddd iiii iiii|-|-|-|-|-|-|Move short immediate|1|
|mvli d,li|0000 0000 100d dddd aaaa aaaa aaaa aaaa|-|-|-|-|-|-|Move long immediate|2|
|stli sa,li|0001 0110 aaaa aaaa iiii iiii iiii iiii|-|-|-|-|-|-|Store long immedate to DMEM by short address (high bits of address are 0xFF)|2|
|clrb b|0001 0010 0000 0bbb|-|-|-|-|-|-|Clear PSR bit|1|
|setb b|0001 0011 0000 0bbb|-|-|-|-|-|-|Set PSR bit|1|
|btstl d,bs|0000 001d 1010 0000 bbbb bbbb bbbb bbbb|-|-|-|-|-|-|Test bit clear(low)|2|
|btsth d,bs|0000 001d 1100 0000 bbbb bbbb bbbb bbbb|-|-|-|-|-|-|Test bit set(high)|2|

## Parallel Instructions

Can be mixed with parallel load/store/move instructions (see next section).

Note that instructions starting with 0b0011 (logical operations) take an extra bit (lsb7), which is usually occupied by paired load/store/move instructions. This fact was not noticed by Duddie as it is not obvious.

|Syntax|Encoding|C|V|Z|N|E|U|Operation|Cycles|
|---|---|---|---|---|---|---|---|---|---|
|add d,s|0100 sssd xxxx xxxx|C1|V1|Z1|N1|E1|U1|Add|1|
|addl d,s|0111 00sd xxxx xxxx|C6|V4|Z1|N1|E1|U1|Add to accumulator low word|1|
|sub d,s|0101 sssd xxxx xxxx|C2|V2|Z1|N1|E1|U1|Sub|1|
|amv d,s|0110 sssd xxxx xxxx|0|0|Z1|N1|E1|U1|Arithmetic move|1|
|cmp d,s|110s d001 xxxx xxxx|C2|V2|Z1|N1|E1|U1|Compare|1|
|cmp a,b|1000 0010 xxxx xxxx|C2|V2|Z1|N1|E1|U1|Compare accumulator|1|
|inc d|0111 10dd xxxx xxxx|C6|V4|Z1|N1|E1|U1|Increment|1|
|dec d|0111 11dd xxxx xxxx|C4|V8|Z1|N1|E1|U1|Decrement|1|
|abs d|1010 d001 xxxx xxxx|0|V5|Z1|N1|E1|U1|Absolute value|1|
|neg d|0111 010d xxxx xxxx|C4|V3|Z1|N1|E1|U1|Negate|1|
|negp d|0111 011d xxxx xxxx|C4|V3|Z1|N1|E1|U1|Negate by product|1|
|clr d|1000 d001 xxxx xxxx|0|0|1|0|0|1|Clear accumulator|1|
|clrp|1000 0101 xxxx xxxx|-|-|-|-|-|-|Clear product|1|
|rnd d|1111 110d xxxx xxxx|C6|V4|Z1|N1|E1|U1|Round accumulator|1|
|rndp d|1111 111d xxxx xxxx|C7|V6|Z1|N1|E1|U1|Round product|1|
|tst s|1011 s001 xxxx xxxx|0|0|Z1|N1|E1|U1|Test (Form 1)|1|
|tst s|1000 011s xxxx xxxx|0|0|Z1|N1|E1|U1|Test (Form 2)|1|
|tstp|1000 0101 xxxx xxxx|0|0|Z1|N1|E1|U1|Test product|1|
|lsl16 d|1111 000d xxxx xxxx|0|0|Z1|N1|E1|U1|Logical shift left 16|1|
|lsr16 d|1111 010d xxxx xxxx|0|0|Z1|N1|E1|U1|Logical shift right 16|1|
|asr16 d|1001 d001 xxxx xxxx|0|0|Z1|N1|E1|U1|Arithmetic shift right 16|1|
|addp d,s|1111 10sd xxxx xxxx|C8|V7|Z1|N1|E1|U1|Add x/y with product|1|
|nop2|1000 0000 xxxx xxxx|-|-|-|-|-|-|Parallel nop|1|
|clrim|1000 1010 xxxx xxxx|-|-|-|-|-|-|Clear IM|1|
|clrdp|1000 1100 xxxx xxxx|-|-|-|-|-|-|Clear DP|1|
|clrxl|1000 1110 xxxx xxxx|-|-|-|-|-|-|Clear XL|1|
|setim|1000 1011 xxxx xxxx|-|-|-|-|-|-|Set IM|1|
|setdp|1000 1101 xxxx xxxx|-|-|-|-|-|-|Set DP|1|
|setxl|1000 1111 xxxx xxxx|-|-|-|-|-|-|Set XL|1|
|mpy s1,s2|1sss s000 xxxx xxxx|-|-|-|-|-|-|Mixed multiply (Form 1)|1|
|mpy x1,x1|1000 0011 xxxx xxxx|-|-|-|-|-|-|Mixed multiply x1\*x1 (Form 2)|1|
|mac s1,s2|1110 00ss xxxx xxxx|-|-|-|-|-|-|Multiply and accumulate (Form 1)|1|
|mac s1,s2|1110 10ss xxxx xxxx|-|-|-|-|-|-|Multiply and accumulate (Form 2)|1|
|mac s1,s2|1111 001s xxxx xxxx|-|-|-|-|-|-|Multiply and accumulate (Form 3)|1|
|macn s1,s2|1110 01ss xxxx xxxx|-|-|-|-|-|-|Multiply and accumulate with negation (Form 1)|1|
|macn s1,s2|1110 11ss xxxx xxxx|-|-|-|-|-|-|Multiply and accumulate with negation (Form 2)|1|
|macn s1,s2|1111 011s xxxx xxxx|-|-|-|-|-|-|Multiply and accumulate with negation (Form 3)|1|
|mvmpy d,s1,s2|1sss s11d xxxx xxxx|0|0|Z1|N1|E1|U1|Move product and mixed multiply|1|
|rnmpy d,s1,s2|1sss s01d xxxx xxxx|C7|V6|Z1|N1|E1|U1|Round product and mixed multiply|1|
|admpy d,s1,s2|1sss s10d xxxx xxxx|C1|V1|Z1|N1|E1|U1|Add product with destination and mixed multiply|1|
|not d|0011 001d 1xxx xxxx|0|0|Z2|N2|E1|U1|Logical not|1|
|xor d,s|0011 00sd 0xxx xxxx|0|0|Z2|N2|E1|U1|Logical xor (Form 1)|1|
|xor d,s|0011 000d 1xxx xxxx|0|0|Z2|N2|E1|U1|Logical xor (Form 2)|1|
|and d,s|0011 01sd 0xxx xxxx|0|0|Z2|N2|E1|U1|Logical and (Form 1)|1|
|and d,s|0011 110d 0xxx xxxx|0|0|Z2|N2|E1|U1|Logical and (Form 2)|1|
|or d,s|0011 10sd 0xxx xxxx|0|0|Z2|N2|E1|U1|Logical or (Form 1)|1|
|or d,s|0011 111d 0xxx xxxx|0|0|Z2|N2|E1|U1|Logical or (Form 2)|1|
|lsf d,s|0011 01sd 1xxx xxxx|0|0|Z1|N1|E1|U1|Logical shift directed by sign (Form 1)|1|
|lsf d,s|0011 110d 1xxx xxxx|0|0|Z1|N1|E1|U1|Logical shift directed by sign (Form 2)|1|
|asf d,s|0011 10sd 1xxx xxxx|0|0|Z1|N1|E1|U1|Arithmetic shift directed by sign (Form 1)|1|
|asf d,s|0011 111d 1xxx xxxx|0|0|Z1|N1|E1|U1|Arithmetic shift directed by sign (Form 2)|1|

## Parallel Load/Store/Move Instructions

|Syntax|Encoding|C|V|Z|N|E|U|Operation|Cycles|
|---|---|---|---|---|---|---|---|---|---|
|ldd d1,rn,mn d2,r3,m3|1xxx xxxx 11dd mnrr|-|-|-|-|-|-|Load dual data (Form 1a)|1|
|ldd d1,rn,mn d2,r3,m3|01xx xxxx 11dd mnrr|-|-|-|-|-|-|Load dual data (Form 1b)|1|
|ldd2 d1,rn,mn d2,r3,m3|1xxx xxxx 11rd mn11|-|-|-|-|-|-|Load dual data (Form 2)|1|
|ls d,r,m r,m,s|1xxx xxxx 10dd mn0s|-|-|-|-|-|-|Load and store (Form 1a)|1|
|ls d,r,m r,m,s|01xx xxxx 10dd mn0s|-|-|-|-|-|-|Load and store (Form 1b)|1|
|ls2 d,r,m r,m,s|1xxx xxxx 10dd mn1s|-|-|-|-|-|-|Load and store (Form 2a)|1|
|ls2 d,r,m r,m,s|01xx xxxx 10dd mn1s|-|-|-|-|-|-|Load and store (Form 2b)|1|
|ld d,rn,mn|1xxx xxxx 01dd dmrr|-|-|-|-|-|-|Load (Form 1a)|1|
|ld d,rn,mn|01xx xxxx 01dd dmrr|-|-|-|-|-|-|Load (Form 1b)|1|
|ld d,rn,mn|0011 xxxx x1dd dmrr|-|-|-|-|-|-|Load (Form 1c)|1|
|st rn,mn,s|1xxx xxxx 001s smrr|-|-|-|-|-|-|Store (Form 1a)|1|
|st rn,mn,s|01xx xxxx 001s smrr|-|-|-|-|-|-|Store (Form 1b)|1|
|st rn,mn,s|0011 xxxx x01s smrr|-|-|-|-|-|-|Store (Form 1c)|1|
|mv d,s|1xxx xxxx 0001 ddss|-|-|-|-|-|-|Parallel Move (Form 1a)|1|
|mv d,s|01xx xxxx 0001 ddss|-|-|-|-|-|-|Parallel Move (Form 1b)|1|
|mv d,s|0011 xxxx x001 ddss|-|-|-|-|-|-|Parallel Move (Form 1c)|1|
|mr rn,mn|1xxx xxxx 0000 mmrr|-|-|-|-|-|-|Parallel modify aux. register (Form 1a)|1|
|mr rn,mn|01xx xxxx 0000 mmrr|-|-|-|-|-|-|Parallel modify aux. register (Form 1b)|1|
|mr rn,mn|0011 xxxx x000 mmrr|-|-|-|-|-|-|Parallel modify aux. register (Form 1c)|1|