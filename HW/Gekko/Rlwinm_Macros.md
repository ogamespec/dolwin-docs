# Rlwinm macros

Source: https://www.ibm.com/support/knowledgecenter/en/ssw_aix_71/assembler/idalangref_32bit_rtate_shift.html

The extended mnemonics for the rotate and shift instructions are in the POWER® family and PowerPC® intersection area.

The extended mnemonics for the rotate and shift instructions are in the POWER® family and PowerPC® intersection area (com assembly mode). A set of rotate and shift extended mnemonics provide for the following operations:

|Item|Description|
|---|---|
|Extract|Selects a field of n bits starting at bit position b in the source register. This field is right- or left-justified in the target register. All other bits of the target register are cleared to 0.|
|Insert|Selects a left- or right-justified field of n bits in the source register. This field is inserted starting at bit position b of the target register. Other bits of the target register are unchanged. No extended mnemonic is provided for insertion of a left-justified field when operating on doublewords, since such an insertion requires more than one instruction.|
|Rotate|Rotates the contents of a register right or left n bits without masking.|
|Shift|Shifts the contents of a register right or left n bits. Vacated bits are cleared to 0 (logical shift).|
|Clear|Clears the leftmost or rightmost n bits of a register to 0.|
|Clear left and shift left|Clears the leftmost b bits of a register, then shifts the register by n bits. This operation can be used to scale a known nonnegative array index by the width of an element.|

The rotate and shift extended mnemonics are shown in the following table. The N operand specifies the number of bits to be extracted, inserted, rotated, or shifted. Because expressions are introduced when the extended mnemonics are mapped to the base mnemonics, certain restrictions are imposed to prevent the result of the expression from causing an overflow in the SH, MB, or ME operand.

To maintain compatibility with previous versions of AIX®, n is not restricted to a value of 0. If n is 0, the assembler treats 32-n as a value of 0.

## 32-bit Rotate and Shift Extended Mnemonics for PowerPC®

|Operation|Extended Mnemonic|Equivalent to|Restrictions|
|---|---|---|---|
|Extract and left justify immediate|extlwi RA, RS, n, b|rlwinm RA, RS, b, 0, n-1|32 > n > 0|
|Extract and right justify immediate|extrwi RA, RS, n, b|rlwinm RA, RS, b+n, 32-n, 31|32 > n > 0 & b+n =< 32|
|Insert from left immediate|inslwi RA, RS, n, b|rlwinm RA, RS, 32-b, b, (b+n)-1|b+n <=32 & 32>n > 0 & 32 > b >= 0|
|Insert from right immediate|insrwi RA, RS, n, b|rlwinm RA, RS, 32-(b+n), b, (b+n)-1|b+n <= 32 & 32>n > 0|
|Rotate left immediate|rotlwi RA, RS, n|rlwinm RA, RS, n, 0, 31|32 > n >= 0|
|Rotate right immediate|rotrwi RA, RS, n|rlwinm RA, RS, 32-n, 0, 31|32 > n >= 0|
|Rotate left|rotlw RA, RS, b|rlwinm RA, RS, RB, 0, 31|None|
|Shift left immediate|slwi RA, RS, n|rlwinm RA, RS, n, 0, 31-n|32 > n >= 0|
|Shift right immediate|srwi RA, RS, n|rlwinm RA, RS, 32-n, n, 31|32 > n >= 0|
|Clear left immediate|clrlwi RA, RS, n|rlwinm RA, RS, 0, n, 31|32 > n >= 0|
|Clear right immediate|clrrwi RA, RS, n|rlwinm RA, RS, 0, 0, 31-n|32 > n >= 0|
|Clear left and shift left immediate|clrslwi RA, RS, b, n|rlwinm RA, RS, b-n, 31-n|b-n >= 0 & 32 > n >= 0 & 32 > b>= 0|
