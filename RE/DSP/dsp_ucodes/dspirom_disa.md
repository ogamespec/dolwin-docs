# Disassembled dsp_irom.bin

## Send sync mail (0x8071FEED).

```
8000 00 92 00 FF 	mvli 	dpp, #0x00FF
8002 12 06       	clr  	et
8003 12 02       	clr  	te0
8004 12 03       	clr  	te1
8005 12 04       	clr  	te2
8006 12 05       	clr  	te3
8007 8E 00       	clr  	xl              	     	
8008 8C 00       	clr  	dp              	     	
8009 8B 00       	set  	im              	     	
800A 16 FC 80 71 	stli 	$(DMBH), #0x8071
800C 16 FD FE ED 	stli 	$(DMBL), #0xFEED
```

## Microcode FEED Loop

```
800E 81 00       	clr  	a               	     	
800F 89 00       	clr  	b               	     	
8010 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
8012 00 9F 80 F3 	mvli 	b1, #0x80F3
8014 82 00       	cmp  	a, b            	     	
8015 02 95 80 1F 	jmpz 	$0x801F
8017 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
8018 16 FC FE EE 	stli 	$(DMBH), #0xFEEE
801A 2E FD       	stsa 	$0x00FD, a1 				// DMBL
801B 02 BF 80 7E 	call 	$0x807E 					// WaitDspMailbox
801D 02 9F 80 0E 	jmp  	$0x800E

801F 26 FF       	ldsa 	a1, $0x00FF 				// CMBL
8020 00 9F A0 01 	mvli 	b1, #0xA001
8022 82 00       	cmp  	a, b            	     	
8023 02 94 80 2C 	jmpnz	$0x802C
8025 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
8027 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
8028 1C 9E       	mv   	m0, a1
8029 1C BF       	mv   	m1, b1
802A 02 9F 80 0E 	jmp  	$0x800E
802C 00 9F A0 02 	mvli 	b1, #0xA002
802E 82 00       	cmp  	a, b            	     	
802F 02 94 80 37 	jmpnz	$0x8037
8031 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
8033 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
8034 1C FF       	mv   	m3, b1
8035 02 9F 80 0E 	jmp  	$0x800E
8037 00 9F C0 02 	mvli 	b1, #0xC002
8039 82 00       	cmp  	a, b            	     	
803A 02 94 80 42 	jmpnz	$0x8042
803C 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
803E 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
803F 1C DF       	mv   	m2, b1
8040 02 9F 80 0E 	jmp  	$0x800E
8042 00 9F B0 01 	mvli 	b1, #0xB001
8044 82 00       	cmp  	a, b            	     	
8045 02 94 80 4E 	jmpnz	$0x804E
8047 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
8049 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
804A 1F 5E       	mv   	x1, a1
804B 1F 1F       	mv   	x0, b1
804C 02 9F 80 0E 	jmp  	$0x800E
804E 00 9F B0 02 	mvli 	b1, #0xB002
8050 82 00       	cmp  	a, b            	     	
8051 02 94 80 59 	jmpnz	$0x8059
8053 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
8055 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
8056 1F 3F       	mv   	y0, b1
8057 02 9F 80 0E 	jmp  	$0x800E
8059 00 9F C0 01 	mvli 	b1, #0xC001
805B 82 00       	cmp  	a, b            	     	
805C 02 94 80 64 	jmpnz	$0x8064
805E 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
8060 27 FF       	ldsa 	b1, $0x00FF 				// CMBL
8061 1F 7F       	mv   	y1, b1
8062 02 9F 80 0E 	jmp  	$0x800E
8064 00 9F D0 01 	mvli 	b1, #0xD001
8066 82 00       	cmp  	a, b            	     	
8067 02 94 80 71 	jmpnz	$0x8071
8069 02 BF 80 78 	call 	$0x8078 					// WaitCpuMailbox
806B 81 00       	clr  	a               	     	
806C 26 FF       	ldsa 	a1, $0x00FF 				// CMBL
806D 1C 1E       	mv   	r0, a1
806E 02 9F 80 B5 	jmp  	$0x80B5 				// Run microcode
8070 00 21       	wait 	
8071 16 FC FA AA 	stli 	$(DMBH), #0xFAAA
8073 2E FD       	stsa 	$0x00FD, a1 				// DMBL
8074 02 BF 80 7E 	call 	$0x807E 				// WaitDspMailbox
8076 02 9F 80 0E 	jmp  	$0x800E
```

```c++

void DspMain ()         // 800E
{
    while (true)
    {
        ac0 = ac1 = 0;
        ac0m = WaitCpuMailbox ();       // High part here

        // First Mail should be always 0x80F3xxxx, following by command sequence mail

        if (ac0m != 0x80F3)
        {
            DMBH = 0xFEEE;          // Invalid 1st message (FEEE)
            DMBL = ac0m;
            WaitDspMailbox ();
            continue;
        }

        // Try command sequence mail

        ac0m = CMBL;
        switch (ac0m)
        {
            // Set ix0,1 registers      (iram_mmem_addr)
            case 0xA001:
                ac0m = WaitCpuMailbox ();
                ix0 = ac0m;
                ix1 = CMBL;
                break;

            // Set ix3          (iram_length bytes)
            case 0xA002:
                ac0m = WaitCpuMailbox();
                ix3 = CMBL;
                break;

            // Set ix2          (iram_addr)
            case 0xc002:
                ac0m = WaitCpuMailbox();
                ix2 = CMBL;         
                break;

            // Set ax1,0 Low            (not used by __DSP_boot_task)
            case 0xB001:
                ac0m = WaitCpuMailbox();
                ax1.l = ac0m;
                ax0.l = CMBL;
                break;

            // Set ax0 High             (always 0 in __DSP_boot_task)
            case 0xB002:
                ac0m = WaitCpuMailbox();
                ax0.h = CMBL;
                break;

            // Set ax1 High             (not used by __DSP_boot_task)
            case 0xC001:
                ac0m = WaitCpuMailbox();
                ax1.h = CMBL;
                break;

            // Execute microcode        (dsp_init_vector)
            case 0xD001:
                WaitCpuMailbox ();
                ar0 = CMBL;
                LoadRunUcode (ar0);

                wait;                   // Never reach here

                break;

            // Unknown command sequence
            default:
                DMBH = 0xFAAA;          // Invalid 2nd message (FAAA)
                DMBL = ac0m;        // Return command to caller

                WaitDspMailbox ();
                break;

        }
    }
}

```

## Mailbox polling

Wait for message from the processor.

```
8078 26 FE       	ldsa 	a1, $0x00FE
8079 02 C0 80 00 	btsth	a1, #0x8000
807B 02 9C 80 78 	jmpnt	$0x8078
807D 02 DF       	rets 	
```

```c++
// Wait for message from the processor
uint16_t WaitCpuMailbox()       // 8078
{
    uint16_t a1;
    while ( ((a1 = *(uint16_t *)(CMBH)) & 0x8000) == 0 )
    { }
    return a1;
}

```

Wait until processor read both the upper and lower parts of DSP Mailbox.

```
807E 26 FC       	ldsa 	a1, $0x00FC
807F 02 A0 80 00 	btstl	a1, #0x8000
8081 02 9C 80 7E 	jmpnt	$0x807E
8083 02 DF       	rets 	
```

```c++
// Wait until processor read mailbox
uint16_t WaitDspMailbox()           // 807E
{
    uint16_t a1;
    while ( ((a1 = *(uint16_t *)(DMBH)) & 0x8000) != 0 )
    { }
    return a1;
}
```

## DspMem -> MainMem

```
8084 00 21       	wait 	
8085 8E 00       	clr  	xl              	     	
8086 81 00       	clr  	a               	     	
8087 1F D9       	mv   	a1, y0
8088 B1 00       	tst  	a               	     	
8089 02 95 80 9D 	jmpz 	$0x809D
808B 00 FA FF CE 	stla 	$(DSMAH), x1
808D 00 F8 FF CF 	stla 	$(DSMAL), x0
808F 00 9E 00 01 	mvli 	a1, #0x0001
8091 00 FE FF C9 	stla 	$(DSCR), a1
8093 00 FB FF CD 	stla 	$(DSPA), y1
8095 00 F9 FF CB 	stla 	$(DSBL), y0
8097 00 DE FF C9 	ldla 	a1, $(DSCR)
8099 02 A0 00 04 	btstl	a1, #0x0004
809B 02 9C 80 97 	jmpnt	$0x8097
809D 81 00       	clr  	a               	     	
809E 1F C7       	mv   	a1, m3
809F B1 00       	tst  	a               	     	
80A0 02 95 80 B4 	jmpz 	$0x80B4
80A2 00 E4 FF CE 	stla 	$(DSMAH), m0
80A4 00 E5 FF CF 	stla 	$(DSMAL), m1
80A6 00 9E 00 03 	mvli 	a1, #0x0003
80A8 00 FE FF C9 	stla 	$(DSCR), a1
80AA 00 E6 FF CD 	stla 	$(DSPA), m2
80AC 00 E7 FF CB 	stla 	$(DSBL), m3
80AE 00 DE FF C9 	ldla 	a1, $(DSCR)
80B0 02 A0 00 04 	btstl	a1, #0x0004
80B2 02 9C 80 AE 	jmpnt	$0x80AE
80B4 02 DF       	rets 	
```

## Load and Run microcode

```
80B5 8E 00       	clr  	xl              	     	
80B6 81 00       	clr  	a               	     	
80B7 89 00       	clr  	b               	     	
80B8 1F F9       	mv   	b1, y0
80B9 B9 00       	tst  	b               	     	
80BA 02 95 80 CE 	jmpz 	$0x80CE
80BC 00 FA FF CE 	stla 	$(DSMAH), x1 			// dram_mmem_addr
80BE 00 F8 FF CF 	stla 	$(DSMAL), x0
80C0 00 9E 00 00 	mvli 	a1, #0x0000
80C2 00 FE FF C9 	stla 	$(DSCR), a1
80C4 00 FB FF CD 	stla 	$(DSPA), y1 			// dram_addr
80C6 00 F9 FF CB 	stla 	$(DSBL), y0 			// dram_length
80C8 00 DE FF C9 	ldla 	a1, $(DSCR)
80CA 02 A0 00 04 	btstl	a1, #0x0004
80CC 02 9C 80 C8 	jmpnt	$0x80C8

80CE 89 00       	clr  	b               	     	
80CF 1F E7       	mv   	b1, m3
80D0 B9 00       	tst  	b               	     	
80D1 02 95 80 E5 	jmpz 	$0x80E5
80D3 00 E4 FF CE 	stla 	$(DSMAH), m0 			// iram_mmem_addr
80D5 00 E5 FF CF 	stla 	$(DSMAL), m1
80D7 00 9E 00 02 	mvli 	a1, #0x0002
80D9 00 FE FF C9 	stla 	$(DSCR), a1
80DB 00 E6 FF CD 	stla 	$(DSPA), m2 			// iram_addr
80DD 00 E7 FF CB 	stla 	$(DSBL), m3 			// iram_length
80DF 00 DE FF C9 	ldla 	a1, $(DSCR)
80E1 02 A0 00 04 	btstl	a1, #0x0004
80E3 02 9C 80 DF 	jmpnt	$0x80DF
80E5 17 0F       	jmp  	r0 						// Goto ucode entrypoint  (dsp_init_vector)
80E6 00 21       	wait 	
```

```c++
void LoadRunUcode(ar0)          // 80B5
{
    clr40();
    ac0 = ac1 = 0;
    ac1m = ax0h;

    if (ac1)
    {
        DSMAH = ax1l;
        DSMAL = ax0l;
        DSCR = 0;           // DMEM Dma, Mmem->DSP
        DSPA = ax1h;
        DSBL = ax0h;

        while (DSCR & 4) ;      // Wait Dma
    }

    ac1 = 0;
    ac1m = ix3;

    if (ac1)
    {
        DSMAH = ix0;
        DSMAL = ix1;
        DSCR = 2;           // IMEM Dma, Mmem->DSP
        DSPA = ix2;
        DSBL = ix3;

        while (DSCR & 4) ;      // Wait Dma
    }

    jmpr r0;       // Goto ucode entrypoint
    wait;
}

```

## Mixing Routines

They only look scary, in fact, they simply mix 32 samples with a duration of 1 ms.

This trick is called unroll the loop.


```
80E7 81 50       	clr  	a               	ld   	x1, r0, +1
80E8 89 49       	clr  	b               	ld   	y0, r1, +1
80E9 B0 72       	mpy  	x1, y0          	ld   	a1, r2, +1
80EA 89 62       	clr  	b               	ld   	a0, r2, +1
80EB F0 7A       	lsl16	a               	ld   	b1, r2, +1
80EC 19 1A       	ld   	x1, r0, +1
80ED B4 6A       	admpy	a, x1, y0       	ld   	b0, r2, +1
80EE 91 00       	asr16	a               	     	
80EF F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
80F0 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
80F1 99 72       	asr16	b               	ld   	a1, r2, +1
80F2 19 5C       	ld   	a0, r2, +1
80F3 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
80F4 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
80F5 91 7A       	asr16	a               	ld   	b1, r2, +1
80F6 19 5D       	ld   	b0, r2, +1
80F7 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
80F8 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
80F9 99 72       	asr16	b               	ld   	a1, r2, +1
80FA 19 5C       	ld   	a0, r2, +1
80FB F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
80FC B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
80FD 91 7A       	asr16	a               	ld   	b1, r2, +1
80FE 19 5D       	ld   	b0, r2, +1
80FF F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8100 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8101 99 72       	asr16	b               	ld   	a1, r2, +1
8102 19 5C       	ld   	a0, r2, +1
8103 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8104 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8105 91 7A       	asr16	a               	ld   	b1, r2, +1
8106 19 5D       	ld   	b0, r2, +1
8107 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8108 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8109 99 72       	asr16	b               	ld   	a1, r2, +1
810A 19 5C       	ld   	a0, r2, +1
810B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
810C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
810D 91 7A       	asr16	a               	ld   	b1, r2, +1
810E 19 5D       	ld   	b0, r2, +1
810F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8110 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8111 99 72       	asr16	b               	ld   	a1, r2, +1
8112 19 5C       	ld   	a0, r2, +1
8113 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8114 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8115 91 7A       	asr16	a               	ld   	b1, r2, +1
8116 19 5D       	ld   	b0, r2, +1
8117 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8118 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8119 99 72       	asr16	b               	ld   	a1, r2, +1
811A 19 5C       	ld   	a0, r2, +1
811B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
811C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
811D 91 7A       	asr16	a               	ld   	b1, r2, +1
811E 19 5D       	ld   	b0, r2, +1
811F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8120 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8121 99 72       	asr16	b               	ld   	a1, r2, +1
8122 19 5C       	ld   	a0, r2, +1
8123 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8124 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8125 91 7A       	asr16	a               	ld   	b1, r2, +1
8126 19 5D       	ld   	b0, r2, +1
8127 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8128 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8129 99 72       	asr16	b               	ld   	a1, r2, +1
812A 19 5C       	ld   	a0, r2, +1
812B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
812C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
812D 91 7A       	asr16	a               	ld   	b1, r2, +1
812E 19 5D       	ld   	b0, r2, +1
812F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8130 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8131 99 72       	asr16	b               	ld   	a1, r2, +1
8132 19 5C       	ld   	a0, r2, +1
8133 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8134 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8135 91 7A       	asr16	a               	ld   	b1, r2, +1
8136 19 5D       	ld   	b0, r2, +1
8137 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8138 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8139 99 72       	asr16	b               	ld   	a1, r2, +1
813A 19 5C       	ld   	a0, r2, +1
813B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
813C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
813D 91 7A       	asr16	a               	ld   	b1, r2, +1
813E 19 5D       	ld   	b0, r2, +1
813F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8140 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8141 99 72       	asr16	b               	ld   	a1, r2, +1
8142 19 5C       	ld   	a0, r2, +1
8143 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8144 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8145 91 7A       	asr16	a               	ld   	b1, r2, +1
8146 19 5D       	ld   	b0, r2, +1
8147 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8148 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8149 99 72       	asr16	b               	ld   	a1, r2, +1
814A 19 5C       	ld   	a0, r2, +1
814B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
814C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
814D 91 7A       	asr16	a               	ld   	b1, r2, +1
814E 19 5D       	ld   	b0, r2, +1
814F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8150 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8151 99 72       	asr16	b               	ld   	a1, r2, +1
8152 19 5C       	ld   	a0, r2, +1
8153 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8154 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8155 91 7A       	asr16	a               	ld   	b1, r2, +1
8156 19 5D       	ld   	b0, r2, +1
8157 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8158 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8159 99 72       	asr16	b               	ld   	a1, r2, +1
815A 19 5C       	ld   	a0, r2, +1
815B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
815C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
815D 91 7A       	asr16	a               	ld   	b1, r2, +1
815E 19 5D       	ld   	b0, r2, +1
815F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8160 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8161 99 72       	asr16	b               	ld   	a1, r2, +1
8162 19 5C       	ld   	a0, r2, +1
8163 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8164 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8165 91 7A       	asr16	a               	ld   	b1, r2, +1
8166 19 5D       	ld   	b0, r2, +1
8167 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8168 1B 7C       	st   	r3, +1, a0
8169 6E 00       	amv  	a, p            	     	
816A B5 12       	admpy	b, x1, y0       	mv   	x0, a1
816B 99 09       	asr16	b               	mr   	r1, +1
816C 1B 7F       	st   	r3, +1, b1
816D 81 2B       	clr  	a               	st   	r3, +1, b0
816E 1C 04       	mv   	r0, m0
816F 1C 45       	mv   	r2, m1
8170 1C 62       	mv   	r3, r2
8171 81 50       	clr  	a               	ld   	x1, r0, +1
8172 89 49       	clr  	b               	ld   	y0, r1, +1
8173 B0 72       	mpy  	x1, y0          	ld   	a1, r2, +1
8174 89 62       	clr  	b               	ld   	a0, r2, +1
8175 F0 7A       	lsl16	a               	ld   	b1, r2, +1
8176 19 1A       	ld   	x1, r0, +1
8177 B4 6A       	admpy	a, x1, y0       	ld   	b0, r2, +1
8178 91 00       	asr16	a               	     	
8179 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
817A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
817B 99 72       	asr16	b               	ld   	a1, r2, +1
817C 19 5C       	ld   	a0, r2, +1
817D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
817E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
817F 91 7A       	asr16	a               	ld   	b1, r2, +1
8180 19 5D       	ld   	b0, r2, +1
8181 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8182 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8183 99 72       	asr16	b               	ld   	a1, r2, +1
8184 19 5C       	ld   	a0, r2, +1
8185 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8186 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8187 91 7A       	asr16	a               	ld   	b1, r2, +1
8188 19 5D       	ld   	b0, r2, +1
8189 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
818A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
818B 99 72       	asr16	b               	ld   	a1, r2, +1
818C 19 5C       	ld   	a0, r2, +1
818D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
818E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
818F 91 7A       	asr16	a               	ld   	b1, r2, +1
8190 19 5D       	ld   	b0, r2, +1
8191 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8192 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8193 99 72       	asr16	b               	ld   	a1, r2, +1
8194 19 5C       	ld   	a0, r2, +1
8195 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8196 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8197 91 7A       	asr16	a               	ld   	b1, r2, +1
8198 19 5D       	ld   	b0, r2, +1
8199 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
819A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
819B 99 72       	asr16	b               	ld   	a1, r2, +1
819C 19 5C       	ld   	a0, r2, +1
819D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
819E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
819F 91 7A       	asr16	a               	ld   	b1, r2, +1
81A0 19 5D       	ld   	b0, r2, +1
81A1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81A2 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81A3 99 72       	asr16	b               	ld   	a1, r2, +1
81A4 19 5C       	ld   	a0, r2, +1
81A5 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81A6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81A7 91 7A       	asr16	a               	ld   	b1, r2, +1
81A8 19 5D       	ld   	b0, r2, +1
81A9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81AA B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81AB 99 72       	asr16	b               	ld   	a1, r2, +1
81AC 19 5C       	ld   	a0, r2, +1
81AD F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81AE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81AF 91 7A       	asr16	a               	ld   	b1, r2, +1
81B0 19 5D       	ld   	b0, r2, +1
81B1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81B2 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81B3 99 72       	asr16	b               	ld   	a1, r2, +1
81B4 19 5C       	ld   	a0, r2, +1
81B5 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81B6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81B7 91 7A       	asr16	a               	ld   	b1, r2, +1
81B8 19 5D       	ld   	b0, r2, +1
81B9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81BA B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81BB 99 72       	asr16	b               	ld   	a1, r2, +1
81BC 19 5C       	ld   	a0, r2, +1
81BD F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81BE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81BF 91 7A       	asr16	a               	ld   	b1, r2, +1
81C0 19 5D       	ld   	b0, r2, +1
81C1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81C2 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81C3 99 72       	asr16	b               	ld   	a1, r2, +1
81C4 19 5C       	ld   	a0, r2, +1
81C5 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81C6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81C7 91 7A       	asr16	a               	ld   	b1, r2, +1
81C8 19 5D       	ld   	b0, r2, +1
81C9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81CA B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81CB 99 72       	asr16	b               	ld   	a1, r2, +1
81CC 19 5C       	ld   	a0, r2, +1
81CD F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81CE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81CF 91 7A       	asr16	a               	ld   	b1, r2, +1
81D0 19 5D       	ld   	b0, r2, +1
81D1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81D2 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81D3 99 72       	asr16	b               	ld   	a1, r2, +1
81D4 19 5C       	ld   	a0, r2, +1
81D5 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81D6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81D7 91 7A       	asr16	a               	ld   	b1, r2, +1
81D8 19 5D       	ld   	b0, r2, +1
81D9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81DA B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81DB 99 72       	asr16	b               	ld   	a1, r2, +1
81DC 19 5C       	ld   	a0, r2, +1
81DD F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81DE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81DF 91 7A       	asr16	a               	ld   	b1, r2, +1
81E0 19 5D       	ld   	b0, r2, +1
81E1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81E2 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81E3 99 72       	asr16	b               	ld   	a1, r2, +1
81E4 19 5C       	ld   	a0, r2, +1
81E5 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81E6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81E7 91 7A       	asr16	a               	ld   	b1, r2, +1
81E8 19 5D       	ld   	b0, r2, +1
81E9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81EA B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
81EB 99 72       	asr16	b               	ld   	a1, r2, +1
81EC 19 5C       	ld   	a0, r2, +1
81ED F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
81EE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
81EF 91 7A       	asr16	a               	ld   	b1, r2, +1
81F0 19 5D       	ld   	b0, r2, +1
81F1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
81F2 1B 7C       	st   	r3, +1, a0
81F3 6E 00       	amv  	a, p            	     	
81F4 B5 1E       	admpy	b, x1, y0       	mv   	y1, a1
81F5 99 09       	asr16	b               	mr   	r1, +1
81F6 1B 7F       	st   	r3, +1, b1
81F7 81 2B       	clr  	a               	st   	r3, +1, b0
81F8 02 DF       	rets 	
```

```
81F9 81 50       	clr  	a               	ld   	x1, r0, +1
81FA 89 49       	clr  	b               	ld   	y0, r1, +1
81FB B0 72       	mpy  	x1, y0          	ld   	a1, r2, +1
81FC 89 62       	clr  	b               	ld   	a0, r2, +1
81FD F0 7A       	lsl16	a               	ld   	b1, r2, +1
81FE 19 1A       	ld   	x1, r0, +1
81FF B4 6A       	admpy	a, x1, y0       	ld   	b0, r2, +1
8200 91 00       	asr16	a               	     	
8201 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8202 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8203 99 72       	asr16	b               	ld   	a1, r2, +1
8204 19 5C       	ld   	a0, r2, +1
8205 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8206 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8207 91 7A       	asr16	a               	ld   	b1, r2, +1
8208 19 5D       	ld   	b0, r2, +1
8209 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
820A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
820B 99 72       	asr16	b               	ld   	a1, r2, +1
820C 19 5C       	ld   	a0, r2, +1
820D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
820E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
820F 91 7A       	asr16	a               	ld   	b1, r2, +1
8210 19 5D       	ld   	b0, r2, +1
8211 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8212 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8213 99 72       	asr16	b               	ld   	a1, r2, +1
8214 19 5C       	ld   	a0, r2, +1
8215 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8216 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8217 91 7A       	asr16	a               	ld   	b1, r2, +1
8218 19 5D       	ld   	b0, r2, +1
8219 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
821A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
821B 99 72       	asr16	b               	ld   	a1, r2, +1
821C 19 5C       	ld   	a0, r2, +1
821D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
821E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
821F 91 7A       	asr16	a               	ld   	b1, r2, +1
8220 19 5D       	ld   	b0, r2, +1
8221 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8222 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8223 99 72       	asr16	b               	ld   	a1, r2, +1
8224 19 5C       	ld   	a0, r2, +1
8225 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8226 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8227 91 7A       	asr16	a               	ld   	b1, r2, +1
8228 19 5D       	ld   	b0, r2, +1
8229 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
822A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
822B 99 72       	asr16	b               	ld   	a1, r2, +1
822C 19 5C       	ld   	a0, r2, +1
822D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
822E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
822F 91 7A       	asr16	a               	ld   	b1, r2, +1
8230 19 5D       	ld   	b0, r2, +1
8231 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8232 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8233 99 72       	asr16	b               	ld   	a1, r2, +1
8234 19 5C       	ld   	a0, r2, +1
8235 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8236 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8237 91 7A       	asr16	a               	ld   	b1, r2, +1
8238 19 5D       	ld   	b0, r2, +1
8239 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
823A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
823B 99 72       	asr16	b               	ld   	a1, r2, +1
823C 19 5C       	ld   	a0, r2, +1
823D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
823E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
823F 91 7A       	asr16	a               	ld   	b1, r2, +1
8240 19 5D       	ld   	b0, r2, +1
8241 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8242 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8243 99 72       	asr16	b               	ld   	a1, r2, +1
8244 19 5C       	ld   	a0, r2, +1
8245 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8246 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8247 91 7A       	asr16	a               	ld   	b1, r2, +1
8248 19 5D       	ld   	b0, r2, +1
8249 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
824A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
824B 99 72       	asr16	b               	ld   	a1, r2, +1
824C 19 5C       	ld   	a0, r2, +1
824D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
824E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
824F 91 7A       	asr16	a               	ld   	b1, r2, +1
8250 19 5D       	ld   	b0, r2, +1
8251 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8252 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8253 99 72       	asr16	b               	ld   	a1, r2, +1
8254 19 5C       	ld   	a0, r2, +1
8255 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8256 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8257 91 7A       	asr16	a               	ld   	b1, r2, +1
8258 19 5D       	ld   	b0, r2, +1
8259 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
825A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
825B 99 72       	asr16	b               	ld   	a1, r2, +1
825C 19 5C       	ld   	a0, r2, +1
825D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
825E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
825F 91 7A       	asr16	a               	ld   	b1, r2, +1
8260 19 5D       	ld   	b0, r2, +1
8261 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8262 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8263 99 72       	asr16	b               	ld   	a1, r2, +1
8264 19 5C       	ld   	a0, r2, +1
8265 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8266 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8267 91 7A       	asr16	a               	ld   	b1, r2, +1
8268 19 5D       	ld   	b0, r2, +1
8269 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
826A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
826B 99 72       	asr16	b               	ld   	a1, r2, +1
826C 19 5C       	ld   	a0, r2, +1
826D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
826E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
826F 91 7A       	asr16	a               	ld   	b1, r2, +1
8270 19 5D       	ld   	b0, r2, +1
8271 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8272 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8273 99 72       	asr16	b               	ld   	a1, r2, +1
8274 19 5C       	ld   	a0, r2, +1
8275 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8276 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8277 91 7A       	asr16	a               	ld   	b1, r2, +1
8278 19 5D       	ld   	b0, r2, +1
8279 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
827A 1B 7C       	st   	r3, +1, a0
827B 6E 00       	amv  	a, p            	     	
827C B5 12       	admpy	b, x1, y0       	mv   	x0, a1
827D 99 09       	asr16	b               	mr   	r1, +1
827E 1B 7F       	st   	r3, +1, b1
827F 81 2B       	clr  	a               	st   	r3, +1, b0
8280 1F 63       	mv   	y1, r3
8281 02 DF       	rets 	
```

```
8282 1C E3       	mv   	m3, r3
8283 81 00       	clr  	a               	     	
8284 89 71       	clr  	b               	ld   	a1, r1, +1
8285 18 BF       	ld   	b1, r1, -1
8286 1B 7E       	st   	r3, +1, a1
8287 4C 00       	add  	a, b            	     	
8288 1B 7E       	st   	r3, +1, a1
8289 4C 00       	add  	a, b            	     	
828A 1B 7E       	st   	r3, +1, a1
828B 4C 00       	add  	a, b            	     	
828C 1B 7E       	st   	r3, +1, a1
828D 4C 00       	add  	a, b            	     	
828E 1B 7E       	st   	r3, +1, a1
828F 4C 00       	add  	a, b            	     	
8290 1B 7E       	st   	r3, +1, a1
8291 4C 00       	add  	a, b            	     	
8292 1B 7E       	st   	r3, +1, a1
8293 4C 00       	add  	a, b            	     	
8294 1B 7E       	st   	r3, +1, a1
8295 4C 00       	add  	a, b            	     	
8296 1B 7E       	st   	r3, +1, a1
8297 4C 00       	add  	a, b            	     	
8298 1B 7E       	st   	r3, +1, a1
8299 4C 00       	add  	a, b            	     	
829A 1B 7E       	st   	r3, +1, a1
829B 4C 00       	add  	a, b            	     	
829C 1B 7E       	st   	r3, +1, a1
829D 4C 00       	add  	a, b            	     	
829E 1B 7E       	st   	r3, +1, a1
829F 4C 00       	add  	a, b            	     	
82A0 1B 7E       	st   	r3, +1, a1
82A1 4C 00       	add  	a, b            	     	
82A2 1B 7E       	st   	r3, +1, a1
82A3 4C 00       	add  	a, b            	     	
82A4 1B 7E       	st   	r3, +1, a1
82A5 4C 00       	add  	a, b            	     	
82A6 1B 7E       	st   	r3, +1, a1
82A7 4C 00       	add  	a, b            	     	
82A8 1B 7E       	st   	r3, +1, a1
82A9 4C 00       	add  	a, b            	     	
82AA 1B 7E       	st   	r3, +1, a1
82AB 4C 00       	add  	a, b            	     	
82AC 1B 7E       	st   	r3, +1, a1
82AD 4C 00       	add  	a, b            	     	
82AE 1B 7E       	st   	r3, +1, a1
82AF 4C 00       	add  	a, b            	     	
82B0 1B 7E       	st   	r3, +1, a1
82B1 4C 00       	add  	a, b            	     	
82B2 1B 7E       	st   	r3, +1, a1
82B3 4C 00       	add  	a, b            	     	
82B4 1B 7E       	st   	r3, +1, a1
82B5 4C 00       	add  	a, b            	     	
82B6 1B 7E       	st   	r3, +1, a1
82B7 4C 00       	add  	a, b            	     	
82B8 1B 7E       	st   	r3, +1, a1
82B9 4C 00       	add  	a, b            	     	
82BA 1B 7E       	st   	r3, +1, a1
82BB 4C 00       	add  	a, b            	     	
82BC 1B 7E       	st   	r3, +1, a1
82BD 4C 00       	add  	a, b            	     	
82BE 1B 7E       	st   	r3, +1, a1
82BF 4C 00       	add  	a, b            	     	
82C0 1B 7E       	st   	r3, +1, a1
82C1 4C 00       	add  	a, b            	     	
82C2 1B 7E       	st   	r3, +1, a1
82C3 4C 00       	add  	a, b            	     	
82C4 1B 7E       	st   	r3, +1, a1
82C5 4C 00       	add  	a, b            	     	
82C6 89 31       	clr  	b               	st   	r1, +1, a1
82C7 81 09       	clr  	a               	mr   	r1, +1
82C8 19 3E       	ld   	a1, r1, +1
82C9 18 BF       	ld   	b1, r1, -1
82CA 1B 7E       	st   	r3, +1, a1
82CB 4C 00       	add  	a, b            	     	
82CC 1B 7E       	st   	r3, +1, a1
82CD 4C 00       	add  	a, b            	     	
82CE 1B 7E       	st   	r3, +1, a1
82CF 4C 00       	add  	a, b            	     	
82D0 1B 7E       	st   	r3, +1, a1
82D1 4C 00       	add  	a, b            	     	
82D2 1B 7E       	st   	r3, +1, a1
82D3 4C 00       	add  	a, b            	     	
82D4 1B 7E       	st   	r3, +1, a1
82D5 4C 00       	add  	a, b            	     	
82D6 1B 7E       	st   	r3, +1, a1
82D7 4C 00       	add  	a, b            	     	
82D8 1B 7E       	st   	r3, +1, a1
82D9 4C 00       	add  	a, b            	     	
82DA 1B 7E       	st   	r3, +1, a1
82DB 4C 00       	add  	a, b            	     	
82DC 1B 7E       	st   	r3, +1, a1
82DD 4C 00       	add  	a, b            	     	
82DE 1B 7E       	st   	r3, +1, a1
82DF 4C 00       	add  	a, b            	     	
82E0 1B 7E       	st   	r3, +1, a1
82E1 4C 00       	add  	a, b            	     	
82E2 1B 7E       	st   	r3, +1, a1
82E3 4C 00       	add  	a, b            	     	
82E4 1B 7E       	st   	r3, +1, a1
82E5 4C 00       	add  	a, b            	     	
82E6 1B 7E       	st   	r3, +1, a1
82E7 4C 00       	add  	a, b            	     	
82E8 1B 7E       	st   	r3, +1, a1
82E9 4C 00       	add  	a, b            	     	
82EA 1B 7E       	st   	r3, +1, a1
82EB 4C 00       	add  	a, b            	     	
82EC 1B 7E       	st   	r3, +1, a1
82ED 4C 00       	add  	a, b            	     	
82EE 1B 7E       	st   	r3, +1, a1
82EF 4C 00       	add  	a, b            	     	
82F0 1B 7E       	st   	r3, +1, a1
82F1 4C 00       	add  	a, b            	     	
82F2 1B 7E       	st   	r3, +1, a1
82F3 4C 00       	add  	a, b            	     	
82F4 1B 7E       	st   	r3, +1, a1
82F5 4C 00       	add  	a, b            	     	
82F6 1B 7E       	st   	r3, +1, a1
82F7 4C 00       	add  	a, b            	     	
82F8 1B 7E       	st   	r3, +1, a1
82F9 4C 00       	add  	a, b            	     	
82FA 1B 7E       	st   	r3, +1, a1
82FB 4C 00       	add  	a, b            	     	
82FC 1B 7E       	st   	r3, +1, a1
82FD 4C 00       	add  	a, b            	     	
82FE 1B 7E       	st   	r3, +1, a1
82FF 4C 00       	add  	a, b            	     	
8300 1B 7E       	st   	r3, +1, a1
8301 4C 00       	add  	a, b            	     	
8302 1B 7E       	st   	r3, +1, a1
8303 4C 00       	add  	a, b            	     	
8304 1B 7E       	st   	r3, +1, a1
8305 4C 00       	add  	a, b            	     	
8306 1B 7E       	st   	r3, +1, a1
8307 4C 00       	add  	a, b            	     	
8308 1B 7E       	st   	r3, +1, a1
8309 4C 00       	add  	a, b            	     	
830A 1B 3E       	st   	r1, +1, a1
830B 1C 27       	mv   	r1, m3
830C 1C 62       	mv   	r3, r2
830D 81 50       	clr  	a               	ld   	x1, r0, +1
830E 89 49       	clr  	b               	ld   	y0, r1, +1
830F B0 72       	mpy  	x1, y0          	ld   	a1, r2, +1
8310 89 62       	clr  	b               	ld   	a0, r2, +1
8311 F0 7A       	lsl16	a               	ld   	b1, r2, +1
8312 19 1A       	ld   	x1, r0, +1
8313 19 39       	ld   	y0, r1, +1
8314 B4 6A       	admpy	a, x1, y0       	ld   	b0, r2, +1
8315 91 00       	asr16	a               	     	
8316 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8317 19 39       	ld   	y0, r1, +1
8318 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8319 99 72       	asr16	b               	ld   	a1, r2, +1
831A 19 5C       	ld   	a0, r2, +1
831B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
831C 19 39       	ld   	y0, r1, +1
831D B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
831E 91 7A       	asr16	a               	ld   	b1, r2, +1
831F 19 5D       	ld   	b0, r2, +1
8320 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8321 19 39       	ld   	y0, r1, +1
8322 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8323 99 72       	asr16	b               	ld   	a1, r2, +1
8324 19 5C       	ld   	a0, r2, +1
8325 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8326 19 39       	ld   	y0, r1, +1
8327 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8328 91 7A       	asr16	a               	ld   	b1, r2, +1
8329 19 5D       	ld   	b0, r2, +1
832A F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
832B 19 39       	ld   	y0, r1, +1
832C B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
832D 99 72       	asr16	b               	ld   	a1, r2, +1
832E 19 5C       	ld   	a0, r2, +1
832F F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8330 19 39       	ld   	y0, r1, +1
8331 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8332 91 7A       	asr16	a               	ld   	b1, r2, +1
8333 19 5D       	ld   	b0, r2, +1
8334 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8335 19 39       	ld   	y0, r1, +1
8336 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8337 99 72       	asr16	b               	ld   	a1, r2, +1
8338 19 5C       	ld   	a0, r2, +1
8339 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
833A 19 39       	ld   	y0, r1, +1
833B B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
833C 91 7A       	asr16	a               	ld   	b1, r2, +1
833D 19 5D       	ld   	b0, r2, +1
833E F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
833F 19 39       	ld   	y0, r1, +1
8340 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8341 99 72       	asr16	b               	ld   	a1, r2, +1
8342 19 5C       	ld   	a0, r2, +1
8343 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8344 19 39       	ld   	y0, r1, +1
8345 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8346 91 7A       	asr16	a               	ld   	b1, r2, +1
8347 19 5D       	ld   	b0, r2, +1
8348 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8349 19 39       	ld   	y0, r1, +1
834A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
834B 99 72       	asr16	b               	ld   	a1, r2, +1
834C 19 5C       	ld   	a0, r2, +1
834D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
834E 19 39       	ld   	y0, r1, +1
834F B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8350 91 7A       	asr16	a               	ld   	b1, r2, +1
8351 19 5D       	ld   	b0, r2, +1
8352 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8353 19 39       	ld   	y0, r1, +1
8354 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8355 99 72       	asr16	b               	ld   	a1, r2, +1
8356 19 5C       	ld   	a0, r2, +1
8357 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8358 19 39       	ld   	y0, r1, +1
8359 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
835A 91 7A       	asr16	a               	ld   	b1, r2, +1
835B 19 5D       	ld   	b0, r2, +1
835C F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
835D 19 39       	ld   	y0, r1, +1
835E B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
835F 99 72       	asr16	b               	ld   	a1, r2, +1
8360 19 5C       	ld   	a0, r2, +1
8361 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8362 19 39       	ld   	y0, r1, +1
8363 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8364 91 7A       	asr16	a               	ld   	b1, r2, +1
8365 19 5D       	ld   	b0, r2, +1
8366 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8367 19 39       	ld   	y0, r1, +1
8368 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8369 99 72       	asr16	b               	ld   	a1, r2, +1
836A 19 5C       	ld   	a0, r2, +1
836B F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
836C 19 39       	ld   	y0, r1, +1
836D B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
836E 91 7A       	asr16	a               	ld   	b1, r2, +1
836F 19 5D       	ld   	b0, r2, +1
8370 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8371 19 39       	ld   	y0, r1, +1
8372 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8373 99 72       	asr16	b               	ld   	a1, r2, +1
8374 19 5C       	ld   	a0, r2, +1
8375 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8376 19 39       	ld   	y0, r1, +1
8377 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8378 91 7A       	asr16	a               	ld   	b1, r2, +1
8379 19 5D       	ld   	b0, r2, +1
837A F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
837B 19 39       	ld   	y0, r1, +1
837C B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
837D 99 72       	asr16	b               	ld   	a1, r2, +1
837E 19 5C       	ld   	a0, r2, +1
837F F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8380 19 39       	ld   	y0, r1, +1
8381 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8382 91 7A       	asr16	a               	ld   	b1, r2, +1
8383 19 5D       	ld   	b0, r2, +1
8384 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8385 19 39       	ld   	y0, r1, +1
8386 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8387 99 72       	asr16	b               	ld   	a1, r2, +1
8388 19 5C       	ld   	a0, r2, +1
8389 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
838A 19 39       	ld   	y0, r1, +1
838B B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
838C 91 7A       	asr16	a               	ld   	b1, r2, +1
838D 19 5D       	ld   	b0, r2, +1
838E F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
838F 19 39       	ld   	y0, r1, +1
8390 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8391 99 72       	asr16	b               	ld   	a1, r2, +1
8392 19 5C       	ld   	a0, r2, +1
8393 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8394 19 39       	ld   	y0, r1, +1
8395 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8396 91 7A       	asr16	a               	ld   	b1, r2, +1
8397 19 5D       	ld   	b0, r2, +1
8398 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8399 19 39       	ld   	y0, r1, +1
839A B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
839B 99 72       	asr16	b               	ld   	a1, r2, +1
839C 19 5C       	ld   	a0, r2, +1
839D F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
839E 19 39       	ld   	y0, r1, +1
839F B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83A0 91 7A       	asr16	a               	ld   	b1, r2, +1
83A1 19 5D       	ld   	b0, r2, +1
83A2 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83A3 19 39       	ld   	y0, r1, +1
83A4 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83A5 99 72       	asr16	b               	ld   	a1, r2, +1
83A6 19 5C       	ld   	a0, r2, +1
83A7 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83A8 19 39       	ld   	y0, r1, +1
83A9 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83AA 91 7A       	asr16	a               	ld   	b1, r2, +1
83AB 19 5D       	ld   	b0, r2, +1
83AC F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83AD 1B 7C       	st   	r3, +1, a0
83AE 6E 00       	amv  	a, p            	     	
83AF B5 12       	admpy	b, x1, y0       	mv   	x0, a1
83B0 99 00       	asr16	b               	     	
83B1 1B 7F       	st   	r3, +1, b1
83B2 81 2B       	clr  	a               	st   	r3, +1, b0
83B3 1C 04       	mv   	r0, m0
83B4 1C 45       	mv   	r2, m1
83B5 1C 62       	mv   	r3, r2
83B6 81 50       	clr  	a               	ld   	x1, r0, +1
83B7 89 49       	clr  	b               	ld   	y0, r1, +1
83B8 B0 72       	mpy  	x1, y0          	ld   	a1, r2, +1
83B9 89 62       	clr  	b               	ld   	a0, r2, +1
83BA F0 7A       	lsl16	a               	ld   	b1, r2, +1
83BB 19 1A       	ld   	x1, r0, +1
83BC 19 39       	ld   	y0, r1, +1
83BD B4 6A       	admpy	a, x1, y0       	ld   	b0, r2, +1
83BE 91 00       	asr16	a               	     	
83BF F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83C0 19 39       	ld   	y0, r1, +1
83C1 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83C2 99 72       	asr16	b               	ld   	a1, r2, +1
83C3 19 5C       	ld   	a0, r2, +1
83C4 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83C5 19 39       	ld   	y0, r1, +1
83C6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83C7 91 7A       	asr16	a               	ld   	b1, r2, +1
83C8 19 5D       	ld   	b0, r2, +1
83C9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83CA 19 39       	ld   	y0, r1, +1
83CB B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83CC 99 72       	asr16	b               	ld   	a1, r2, +1
83CD 19 5C       	ld   	a0, r2, +1
83CE F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83CF 19 39       	ld   	y0, r1, +1
83D0 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83D1 91 7A       	asr16	a               	ld   	b1, r2, +1
83D2 19 5D       	ld   	b0, r2, +1
83D3 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83D4 19 39       	ld   	y0, r1, +1
83D5 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83D6 99 72       	asr16	b               	ld   	a1, r2, +1
83D7 19 5C       	ld   	a0, r2, +1
83D8 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83D9 19 39       	ld   	y0, r1, +1
83DA B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83DB 91 7A       	asr16	a               	ld   	b1, r2, +1
83DC 19 5D       	ld   	b0, r2, +1
83DD F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83DE 19 39       	ld   	y0, r1, +1
83DF B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83E0 99 72       	asr16	b               	ld   	a1, r2, +1
83E1 19 5C       	ld   	a0, r2, +1
83E2 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83E3 19 39       	ld   	y0, r1, +1
83E4 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83E5 91 7A       	asr16	a               	ld   	b1, r2, +1
83E6 19 5D       	ld   	b0, r2, +1
83E7 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83E8 19 39       	ld   	y0, r1, +1
83E9 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83EA 99 72       	asr16	b               	ld   	a1, r2, +1
83EB 19 5C       	ld   	a0, r2, +1
83EC F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83ED 19 39       	ld   	y0, r1, +1
83EE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83EF 91 7A       	asr16	a               	ld   	b1, r2, +1
83F0 19 5D       	ld   	b0, r2, +1
83F1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83F2 19 39       	ld   	y0, r1, +1
83F3 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83F4 99 72       	asr16	b               	ld   	a1, r2, +1
83F5 19 5C       	ld   	a0, r2, +1
83F6 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
83F7 19 39       	ld   	y0, r1, +1
83F8 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
83F9 91 7A       	asr16	a               	ld   	b1, r2, +1
83FA 19 5D       	ld   	b0, r2, +1
83FB F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
83FC 19 39       	ld   	y0, r1, +1
83FD B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
83FE 99 72       	asr16	b               	ld   	a1, r2, +1
83FF 19 5C       	ld   	a0, r2, +1
8400 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8401 19 39       	ld   	y0, r1, +1
8402 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8403 91 7A       	asr16	a               	ld   	b1, r2, +1
8404 19 5D       	ld   	b0, r2, +1
8405 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8406 19 39       	ld   	y0, r1, +1
8407 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8408 99 72       	asr16	b               	ld   	a1, r2, +1
8409 19 5C       	ld   	a0, r2, +1
840A F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
840B 19 39       	ld   	y0, r1, +1
840C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
840D 91 7A       	asr16	a               	ld   	b1, r2, +1
840E 19 5D       	ld   	b0, r2, +1
840F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8410 19 39       	ld   	y0, r1, +1
8411 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8412 99 72       	asr16	b               	ld   	a1, r2, +1
8413 19 5C       	ld   	a0, r2, +1
8414 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8415 19 39       	ld   	y0, r1, +1
8416 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8417 91 7A       	asr16	a               	ld   	b1, r2, +1
8418 19 5D       	ld   	b0, r2, +1
8419 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
841A 19 39       	ld   	y0, r1, +1
841B B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
841C 99 72       	asr16	b               	ld   	a1, r2, +1
841D 19 5C       	ld   	a0, r2, +1
841E F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
841F 19 39       	ld   	y0, r1, +1
8420 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8421 91 7A       	asr16	a               	ld   	b1, r2, +1
8422 19 5D       	ld   	b0, r2, +1
8423 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8424 19 39       	ld   	y0, r1, +1
8425 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8426 99 72       	asr16	b               	ld   	a1, r2, +1
8427 19 5C       	ld   	a0, r2, +1
8428 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8429 19 39       	ld   	y0, r1, +1
842A B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
842B 91 7A       	asr16	a               	ld   	b1, r2, +1
842C 19 5D       	ld   	b0, r2, +1
842D F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
842E 19 39       	ld   	y0, r1, +1
842F B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8430 99 72       	asr16	b               	ld   	a1, r2, +1
8431 19 5C       	ld   	a0, r2, +1
8432 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8433 19 39       	ld   	y0, r1, +1
8434 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8435 91 7A       	asr16	a               	ld   	b1, r2, +1
8436 19 5D       	ld   	b0, r2, +1
8437 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8438 19 39       	ld   	y0, r1, +1
8439 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
843A 99 72       	asr16	b               	ld   	a1, r2, +1
843B 19 5C       	ld   	a0, r2, +1
843C F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
843D 19 39       	ld   	y0, r1, +1
843E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
843F 91 7A       	asr16	a               	ld   	b1, r2, +1
8440 19 5D       	ld   	b0, r2, +1
8441 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8442 19 39       	ld   	y0, r1, +1
8443 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8444 99 72       	asr16	b               	ld   	a1, r2, +1
8445 19 5C       	ld   	a0, r2, +1
8446 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8447 19 39       	ld   	y0, r1, +1
8448 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8449 91 7A       	asr16	a               	ld   	b1, r2, +1
844A 19 5D       	ld   	b0, r2, +1
844B F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
844C 19 39       	ld   	y0, r1, +1
844D B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
844E 99 72       	asr16	b               	ld   	a1, r2, +1
844F 19 5C       	ld   	a0, r2, +1
8450 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8451 19 39       	ld   	y0, r1, +1
8452 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8453 91 7A       	asr16	a               	ld   	b1, r2, +1
8454 19 5D       	ld   	b0, r2, +1
8455 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8456 1B 7C       	st   	r3, +1, a0
8457 6E 00       	amv  	a, p            	     	
8458 B5 1E       	admpy	b, x1, y0       	mv   	y1, a1
8459 99 00       	asr16	b               	     	
845A 1B 7F       	st   	r3, +1, b1
845B 81 2B       	clr  	a               	st   	r3, +1, b0
845C 02 DF       	rets 	
```

```
845D 1C E3       	mv   	m3, r3
845E 81 00       	clr  	a               	     	
845F 89 71       	clr  	b               	ld   	a1, r1, +1
8460 18 BF       	ld   	b1, r1, -1
8461 1B 7E       	st   	r3, +1, a1
8462 4C 00       	add  	a, b            	     	
8463 1B 7E       	st   	r3, +1, a1
8464 4C 00       	add  	a, b            	     	
8465 1B 7E       	st   	r3, +1, a1
8466 4C 00       	add  	a, b            	     	
8467 1B 7E       	st   	r3, +1, a1
8468 4C 00       	add  	a, b            	     	
8469 1B 7E       	st   	r3, +1, a1
846A 4C 00       	add  	a, b            	     	
846B 1B 7E       	st   	r3, +1, a1
846C 4C 00       	add  	a, b            	     	
846D 1B 7E       	st   	r3, +1, a1
846E 4C 00       	add  	a, b            	     	
846F 1B 7E       	st   	r3, +1, a1
8470 4C 00       	add  	a, b            	     	
8471 1B 7E       	st   	r3, +1, a1
8472 4C 00       	add  	a, b            	     	
8473 1B 7E       	st   	r3, +1, a1
8474 4C 00       	add  	a, b            	     	
8475 1B 7E       	st   	r3, +1, a1
8476 4C 00       	add  	a, b            	     	
8477 1B 7E       	st   	r3, +1, a1
8478 4C 00       	add  	a, b            	     	
8479 1B 7E       	st   	r3, +1, a1
847A 4C 00       	add  	a, b            	     	
847B 1B 7E       	st   	r3, +1, a1
847C 4C 00       	add  	a, b            	     	
847D 1B 7E       	st   	r3, +1, a1
847E 4C 00       	add  	a, b            	     	
847F 1B 7E       	st   	r3, +1, a1
8480 4C 00       	add  	a, b            	     	
8481 1B 7E       	st   	r3, +1, a1
8482 4C 00       	add  	a, b            	     	
8483 1B 7E       	st   	r3, +1, a1
8484 4C 00       	add  	a, b            	     	
8485 1B 7E       	st   	r3, +1, a1
8486 4C 00       	add  	a, b            	     	
8487 1B 7E       	st   	r3, +1, a1
8488 4C 00       	add  	a, b            	     	
8489 1B 7E       	st   	r3, +1, a1
848A 4C 00       	add  	a, b            	     	
848B 1B 7E       	st   	r3, +1, a1
848C 4C 00       	add  	a, b            	     	
848D 1B 7E       	st   	r3, +1, a1
848E 4C 00       	add  	a, b            	     	
848F 1B 7E       	st   	r3, +1, a1
8490 4C 00       	add  	a, b            	     	
8491 1B 7E       	st   	r3, +1, a1
8492 4C 00       	add  	a, b            	     	
8493 1B 7E       	st   	r3, +1, a1
8494 4C 00       	add  	a, b            	     	
8495 1B 7E       	st   	r3, +1, a1
8496 4C 00       	add  	a, b            	     	
8497 1B 7E       	st   	r3, +1, a1
8498 4C 00       	add  	a, b            	     	
8499 1B 7E       	st   	r3, +1, a1
849A 4C 00       	add  	a, b            	     	
849B 1B 7E       	st   	r3, +1, a1
849C 4C 00       	add  	a, b            	     	
849D 1B 7E       	st   	r3, +1, a1
849E 4C 00       	add  	a, b            	     	
849F 1B 7E       	st   	r3, +1, a1
84A0 4C 00       	add  	a, b            	     	
84A1 89 31       	clr  	b               	st   	r1, +1, a1
84A2 1C 27       	mv   	r1, m3
84A3 1C 62       	mv   	r3, r2
84A4 81 50       	clr  	a               	ld   	x1, r0, +1
84A5 19 39       	ld   	y0, r1, +1
84A6 B0 72       	mpy  	x1, y0          	ld   	a1, r2, +1
84A7 89 62       	clr  	b               	ld   	a0, r2, +1
84A8 F0 7A       	lsl16	a               	ld   	b1, r2, +1
84A9 19 1A       	ld   	x1, r0, +1
84AA 19 39       	ld   	y0, r1, +1
84AB B4 6A       	admpy	a, x1, y0       	ld   	b0, r2, +1
84AC 91 00       	asr16	a               	     	
84AD F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84AE 19 39       	ld   	y0, r1, +1
84AF B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84B0 99 72       	asr16	b               	ld   	a1, r2, +1
84B1 19 5C       	ld   	a0, r2, +1
84B2 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84B3 19 39       	ld   	y0, r1, +1
84B4 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84B5 91 7A       	asr16	a               	ld   	b1, r2, +1
84B6 19 5D       	ld   	b0, r2, +1
84B7 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84B8 19 39       	ld   	y0, r1, +1
84B9 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84BA 99 72       	asr16	b               	ld   	a1, r2, +1
84BB 19 5C       	ld   	a0, r2, +1
84BC F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84BD 19 39       	ld   	y0, r1, +1
84BE B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84BF 91 7A       	asr16	a               	ld   	b1, r2, +1
84C0 19 5D       	ld   	b0, r2, +1
84C1 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84C2 19 39       	ld   	y0, r1, +1
84C3 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84C4 99 72       	asr16	b               	ld   	a1, r2, +1
84C5 19 5C       	ld   	a0, r2, +1
84C6 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84C7 19 39       	ld   	y0, r1, +1
84C8 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84C9 91 7A       	asr16	a               	ld   	b1, r2, +1
84CA 19 5D       	ld   	b0, r2, +1
84CB F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84CC 19 39       	ld   	y0, r1, +1
84CD B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84CE 99 72       	asr16	b               	ld   	a1, r2, +1
84CF 19 5C       	ld   	a0, r2, +1
84D0 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84D1 19 39       	ld   	y0, r1, +1
84D2 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84D3 91 7A       	asr16	a               	ld   	b1, r2, +1
84D4 19 5D       	ld   	b0, r2, +1
84D5 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84D6 19 39       	ld   	y0, r1, +1
84D7 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84D8 99 72       	asr16	b               	ld   	a1, r2, +1
84D9 19 5C       	ld   	a0, r2, +1
84DA F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84DB 19 39       	ld   	y0, r1, +1
84DC B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84DD 91 7A       	asr16	a               	ld   	b1, r2, +1
84DE 19 5D       	ld   	b0, r2, +1
84DF F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84E0 19 39       	ld   	y0, r1, +1
84E1 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84E2 99 72       	asr16	b               	ld   	a1, r2, +1
84E3 19 5C       	ld   	a0, r2, +1
84E4 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84E5 19 39       	ld   	y0, r1, +1
84E6 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84E7 91 7A       	asr16	a               	ld   	b1, r2, +1
84E8 19 5D       	ld   	b0, r2, +1
84E9 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84EA 19 39       	ld   	y0, r1, +1
84EB B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84EC 99 72       	asr16	b               	ld   	a1, r2, +1
84ED 19 5C       	ld   	a0, r2, +1
84EE F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84EF 19 39       	ld   	y0, r1, +1
84F0 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84F1 91 7A       	asr16	a               	ld   	b1, r2, +1
84F2 19 5D       	ld   	b0, r2, +1
84F3 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84F4 19 39       	ld   	y0, r1, +1
84F5 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
84F6 99 72       	asr16	b               	ld   	a1, r2, +1
84F7 19 5C       	ld   	a0, r2, +1
84F8 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
84F9 19 39       	ld   	y0, r1, +1
84FA B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
84FB 91 7A       	asr16	a               	ld   	b1, r2, +1
84FC 19 5D       	ld   	b0, r2, +1
84FD F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
84FE 19 39       	ld   	y0, r1, +1
84FF B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8500 99 72       	asr16	b               	ld   	a1, r2, +1
8501 19 5C       	ld   	a0, r2, +1
8502 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8503 19 39       	ld   	y0, r1, +1
8504 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8505 91 7A       	asr16	a               	ld   	b1, r2, +1
8506 19 5D       	ld   	b0, r2, +1
8507 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8508 19 39       	ld   	y0, r1, +1
8509 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
850A 99 72       	asr16	b               	ld   	a1, r2, +1
850B 19 5C       	ld   	a0, r2, +1
850C F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
850D 19 39       	ld   	y0, r1, +1
850E B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
850F 91 7A       	asr16	a               	ld   	b1, r2, +1
8510 19 5D       	ld   	b0, r2, +1
8511 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8512 19 39       	ld   	y0, r1, +1
8513 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8514 99 72       	asr16	b               	ld   	a1, r2, +1
8515 19 5C       	ld   	a0, r2, +1
8516 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8517 19 39       	ld   	y0, r1, +1
8518 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8519 91 7A       	asr16	a               	ld   	b1, r2, +1
851A 19 5D       	ld   	b0, r2, +1
851B F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
851C 19 39       	ld   	y0, r1, +1
851D B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
851E 99 72       	asr16	b               	ld   	a1, r2, +1
851F 19 5C       	ld   	a0, r2, +1
8520 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8521 19 39       	ld   	y0, r1, +1
8522 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8523 91 7A       	asr16	a               	ld   	b1, r2, +1
8524 19 5D       	ld   	b0, r2, +1
8525 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8526 19 39       	ld   	y0, r1, +1
8527 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8528 99 72       	asr16	b               	ld   	a1, r2, +1
8529 19 5C       	ld   	a0, r2, +1
852A F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
852B 19 39       	ld   	y0, r1, +1
852C B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
852D 91 7A       	asr16	a               	ld   	b1, r2, +1
852E 19 5D       	ld   	b0, r2, +1
852F F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8530 19 39       	ld   	y0, r1, +1
8531 B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
8532 99 72       	asr16	b               	ld   	a1, r2, +1
8533 19 5C       	ld   	a0, r2, +1
8534 F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
8535 19 39       	ld   	y0, r1, +1
8536 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8537 91 7A       	asr16	a               	ld   	b1, r2, +1
8538 19 5D       	ld   	b0, r2, +1
8539 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
853A 19 39       	ld   	y0, r1, +1
853B B5 23       	admpy	b, x1, y0       	st   	r3, +1, a0
853C 99 72       	asr16	b               	ld   	a1, r2, +1
853D 19 5C       	ld   	a0, r2, +1
853E F0 A1       	lsl16	a               	ls   	x1, r0, +1, r3, +1, b1
853F 19 39       	ld   	y0, r1, +1
8540 B4 2B       	admpy	a, x1, y0       	st   	r3, +1, b0
8541 91 7A       	asr16	a               	ld   	b1, r2, +1
8542 19 5D       	ld   	b0, r2, +1
8543 F1 A0       	lsl16	b               	ls   	x1, r0, +1, r3, +1, a1
8544 1B 7C       	st   	r3, +1, a0
8545 6E 00       	amv  	a, p            	     	
8546 B5 12       	admpy	b, x1, y0       	mv   	x0, a1
8547 99 00       	asr16	b               	     	
8548 1B 7F       	st   	r3, +1, b1
8549 81 2B       	clr  	a               	st   	r3, +1, b0
854A 02 DF       	rets 	
```

```
854B 8E 00       	clr  	xl              	     	
854C 00 80 08 00 	mvli 	r0, #0x0800
854E 00 92 00 FF 	mvli 	dpp, #0x00FF
8550 00 C4 04 03 	ldla 	m0, $0x0403
8552 1F E4       	mv   	b1, m0
8553 05 03       	adsi 	b, 3
8554 15 6E       	lsfi 	b, -18
8555 15 02       	lsfi 	b, 2
8556 29 C9       	stsa 	$0x00C9, b2
8557 00 DE 04 00 	ldla 	a1, $0x0400
8559 2E CE       	stsa 	$0x00CE, a1
855A 00 DE 04 01 	ldla 	a1, $0x0401
855C 2E CF       	stsa 	$0x00CF, a1
855D 00 E0 FF CD 	stla 	$(DSPA), r0
855F 2D CB       	stsa 	$0x00CB, b0
8560 02 BF 86 3D 	call 	$0x863D
8562 29 D1       	stsa 	$0x00D1, b2
8563 29 D4       	stsa 	$0x00D4, b2
8564 29 D5       	stsa 	$0x00D5, b2
8565 16 D6 01 FF 	stli 	$(ACEAH), #0x01FF
8567 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
8569 00 DF 04 04 	ldla 	b1, $0x0404
856B 00 DD 04 05 	ldla 	b0, $0x0405
856D 15 7F       	lsfi 	b, -1
856E 03 60 80 00 	orli 	b, #0x8000
8570 2F D8       	stsa 	$0x00D8, b1
8571 2D D9       	stsa 	$0x00D9, b0
8572 00 82 FF D3 	mvli 	r2, #0xFFD3
8574 00 86 00 00 	mvli 	m2, #0x0000
8576 1F E4       	mv   	b1, m0
8577 03 C0 00 01 	btsth	b1, #0x0001
8579 15 7F       	lsfi 	b, -1
857A 1C BF       	mv   	m1, b1
857B 00 9A FF F8 	mvli 	x1, #0xFFF8
857D 00 9B 00 18 	mvli 	y1, #0x0018
857F 81 78       	clr  	a               	ld   	b1, r0, +1
8580 00 65 85 86 	loop 	m1, $0x8586
8582 35 BE       	lsf  	b, x1           	st   	r2, +m2, b1
8583 37 93       	lsf  	b, y1           	mv   	x0, b1
8584 F5 00       	lsr16	b               	     	
8585 70 17       	addl 	a, x0           	mv   	y0, b1
8586 72 78       	addl 	a, y0           	ld   	b1, r0, +1
8587 02 9C 85 8C 	jmpnt	$0x858C
8589 35 BE       	lsf  	b, x1           	st   	r2, +m2, b1
858A 1F 1F       	mv   	x0, b1
858B 70 00       	addl 	a, x0           	     	
858C 6D 00       	amv  	b, a            	     	
858D 00 80 04 08 	mvli 	r0, #0x0408
858F 00 9A 12 DF 	mvli 	x1, #0x12DF
8591 00 98 AC BD 	mvli 	x0, #0xACBD
8593 48 00       	add  	a, x            	     	
8594 1B 1E       	st   	r0, +1, a1
8595 1B 1C       	st   	r0, +1, a0
8596 00 9E FB CA 	mvli 	a1, #0xFBCA
8598 1B 1E       	st   	r0, +1, a1
8599 00 9E DE B0 	mvli 	a1, #0xDEB0
859B 1B 1E       	st   	r0, +1, a1
859C 00 9E FD E1 	mvli 	a1, #0xFDE1
859E 1B 1E       	st   	r0, +1, a1
859F 00 9E FA CB 	mvli 	a1, #0xFACB
85A1 1B 1E       	st   	r0, +1, a1
85A2 00 9E DE AD 	mvli 	a1, #0xDEAD
85A4 1B 1E       	st   	r0, +1, a1
85A5 00 9E BE EF 	mvli 	a1, #0xBEEF
85A7 08 0D       	mvsi 	x0, 13
85A8 71 30       	addl 	b, x0           	st   	r0, +1, a1
85A9 1B 1D       	st   	r0, +1, b0
85AA 1B 11       	st   	r0, +1, b2
85AB 00 80 08 00 	mvli 	r0, #0x0800
85AD 00 81 04 09 	mvli 	r1, #0x0409
85AF 00 82 04 0F 	mvli 	r2, #0x040F
85B1 00 85 04 10 	mvli 	m1, #0x0410
85B3 00 86 04 0E 	mvli 	m2, #0x040E
85B5 00 87 FF FE 	mvli 	m3, #0xFFFE
85B7 16 D1 00 05 	stli 	$(ACFMT), #0x0005
85B9 16 D4 00 00 	stli 	$(ACSAH), #0x0000
85BB 16 D5 00 00 	stli 	$(ACSAL), #0x0000
85BD 16 D6 00 00 	stli 	$(ACEAH), #0x0000
85BF 16 D7 00 FF 	stli 	$(ACEAL), #0x00FF
85C1 16 D8 00 00 	stli 	$(ACCAH), #0x0000
85C3 16 D9 00 00 	stli 	$(ACCAL), #0x0000
85C5 16 DA 00 00 	stli 	$(ACPDS), #0x0000
85C7 16 A0 F9 B8 	stli 	$(ADPCM_A00), #0xF9B8
85C9 16 A1 FE C7 	stli 	$(ADPCM_A10), #0xFEC7
85CB 16 DE 08 00 	stli 	$(ACGAN), #0x0800
85CD 16 DB 00 00 	stli 	$(ACYN1), #0x0000
85CF 16 DC 00 00 	stli 	$(ACYN2), #0x0000
85D1 1F E4       	mv   	b1, m0
85D2 19 18       	ld   	x0, r0, +1
85D3 00 F8 FF DF 	stla 	$(UnkHW_FFDF), x0
85D5 1C 65       	mv   	r3, m1
85D6 18 BC       	ld   	a0, r1, -1
85D7 19 3E       	ld   	a1, r1, +1
85D8 00 D8 FF DD 	ldla 	x0, $(ACDAT)
85DA 70 00       	addl 	a, x0           	     	
85DB 1A BC       	st   	r1, -1, a0
85DC 79 31       	dec  	b1              	st   	r1, +1, a1
85DD 15 7F       	lsfi 	b, -1
85DE 00 7F 85 FD 	loop 	b1, $0x85FD
85E0 02 BF 86 11 	call 	$0x8611
85E2 19 1E       	ld   	a1, r0, +1
85E3 31 60       	xor  	b1, x1          	ld   	a0, r0, +1
85E4 14 78       	lsfi 	a, -8
85E5 00 FC FF DF 	stla 	$(UnkHW_FFDF), a0
85E7 1C 65       	mv   	r3, m1
85E8 18 BC       	ld   	a0, r1, -1
85E9 33 71       	xor  	b1, y1          	ld   	a1, r1, +1
85EA 00 D8 FF DD 	ldla 	x0, $(ACDAT)
85EC 70 2A       	addl 	a, x0           	st   	r2, +1, b0
85ED 1A 5F       	st   	r2, 0, b1
85EE 1A BC       	st   	r1, -1, a0
85EF 1B 3E       	st   	r1, +1, a1
85F0 02 BF 86 11 	call 	$0x8611
85F2 31 40       	xor  	b1, x1          	ld   	x0, r0, +1
85F3 00 F8 FF DF 	stla 	$(UnkHW_FFDF), x0
85F5 1C 65       	mv   	r3, m1
85F6 18 BC       	ld   	a0, r1, -1
85F7 33 71       	xor  	b1, y1          	ld   	a1, r1, +1
85F8 00 D8 FF DD 	ldla 	x0, $(ACDAT)
85FA 70 2A       	addl 	a, x0           	st   	r2, +1, b0
85FB 1A 5F       	st   	r2, 0, b1
85FC 1A BC       	st   	r1, -1, a0
85FD 1B 3E       	st   	r1, +1, a1
85FE 02 9D 86 02 	jmpt 	$0x8602
8600 02 BF 86 11 	call 	$0x8611
8602 16 C9 00 01 	stli 	$(DSCR), #0x0001
8604 00 DE 04 06 	ldla 	a1, $0x0406
8606 2E CE       	stsa 	$0x00CE, a1
8607 00 DE 04 07 	ldla 	a1, $0x0407
8609 2E CF       	stsa 	$0x00CF, a1
860A 16 CD 04 0A 	stli 	$(DSPA), #0x040A
860C 16 CB 00 04 	stli 	$(DSBL), #0x0004
860E 02 BF 86 3D 	call 	$0x863D
8610 02 DF       	rets 	
8611 18 DA       	ld   	x1, r2, -1
8612 18 DB       	ld   	y1, r2, -1
8613 18 DD       	ld   	b0, r2, -1
8614 18 DF       	ld   	b1, r2, -1
8615 4C 04       	add  	a, b            	mr   	r0, -1
8616 1F FC       	mv   	b1, a0
8617 31 43       	xor  	b1, x1          	ld   	x0, r3, +1
8618 F5 63       	lsr16	b               	ld   	a0, r3, +1
8619 1F FE       	mv   	b1, a1
861A 76 07       	inc  	a               	mr   	r3, -1
861B 33 23       	xor  	b1, y1          	st   	r3, +1, a0
861C 70 42       	addl 	a, x0           	ld   	x0, r2, +1
861D 14 23       	lsfi 	a, 35
861E 14 6D       	lsfi 	a, -19
861F 1F 5E       	mv   	x1, a1
8620 04 E0       	adsi 	a, -32
8621 6C 1E       	amv  	a, b            	mv   	y1, a1
8622 1C 66       	mv   	r3, m2
8623 34 86       	lsf  	a, x1           	mr   	r2, -1
8624 37 86       	lsf  	b, y1           	mr   	r2, -1
8625 4C 52       	add  	a, b            	ld   	x1, r2, +1
8626 48 6B       	add  	a, x            	ld   	b0, r3, +1
8627 1A DC       	st   	r2, -1, a0
8628 1A 5E       	st   	r2, 0, a1
8629 18 3E       	ld   	a1, r1, 0
862A 18 BF       	ld   	b1, r1, -1
862B 33 D2       	not  	b1              	ld   	x1, r2, +1
862C 19 5B       	ld   	y1, r2, +1
862D 36 5F       	and  	a1, y1          	ld   	y1, r3, +m3
862E 37 1E       	and  	b1, y1          	mv   	y1, a1
862F 3B 1D       	or   	b1, y1          	mv   	y1, b0
8630 1A FF       	st   	r3, -1, b1
8631 18 3E       	ld   	a1, r1, 0
8632 34 79       	and  	a1, x1          	ld   	b1, r1, +1
8633 33 9A       	not  	b1              	mv   	x1, a1
8634 37 05       	and  	b1, y1          	mr   	r1, -1
8635 39 0A       	or   	b1, x1          	mr   	r2, +1
8636 1B FF       	st   	r3, +m3, b1
8637 19 7B       	ld   	y1, r3, +1
8638 33 59       	xor  	b1, y1          	ld   	y1, r1, +1
8639 33 5A       	xor  	b1, y1          	ld   	y1, r2, +1
863A F5 57       	lsr16	b               	ld   	x1, r3, +m3
863B 19 7F       	ld   	b1, r3, +1
863C 02 DF       	rets 	
863D 00 DF FF C9 	ldla 	b1, $(DSCR)
863F 03 C0 00 04 	btsth	b1, #0x0004
8641 02 9D 86 3D 	jmpt 	$0x863D
8643 02 DF       	rets 	
8644 8E 00       	clr  	xl              	     	
8645 00 81 08 00 	mvli 	r1, #0x0800
8647 00 92 00 FF 	mvli 	dpp, #0x00FF
8649 00 DF 04 03 	ldla 	b1, $0x0403
864B 05 03       	adsi 	b, 3
864C 15 6E       	lsfi 	b, -18
864D 15 02       	lsfi 	b, 2
864E 29 C9       	stsa 	$0x00C9, b2
864F 00 DE 04 00 	ldla 	a1, $0x0400
8651 2E CE       	stsa 	$0x00CE, a1
8652 00 DE 04 01 	ldla 	a1, $0x0401
8654 2E CF       	stsa 	$0x00CF, a1
8655 00 E1 FF CD 	stla 	$(DSPA), r1
8657 2D CB       	stsa 	$0x00CB, b0
8658 02 BF 86 3D 	call 	$0x863D
865A 29 D1       	stsa 	$0x00D1, b2
865B 29 D4       	stsa 	$0x00D4, b2
865C 29 D5       	stsa 	$0x00D5, b2
865D 16 D6 01 FF 	stli 	$(ACEAH), #0x01FF
865F 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
8661 00 DF 04 04 	ldla 	b1, $0x0404
8663 00 DD 04 05 	ldla 	b0, $0x0405
8665 15 7F       	lsfi 	b, -1
8666 03 60 80 00 	orli 	b, #0x8000
8668 2F D8       	stsa 	$0x00D8, b1
8669 2D D9       	stsa 	$0x00D9, b0
866A 00 80 FF D3 	mvli 	r0, #0xFFD3
866C 00 84 00 00 	mvli 	m0, #0x0000
866E 00 DF 04 03 	ldla 	b1, $0x0403
8670 03 C0 00 01 	btsth	b1, #0x0001
8672 15 7F       	lsfi 	b, -1
8673 1C DF       	mv   	m2, b1
8674 00 9A FF F8 	mvli 	x1, #0xFFF8
8676 00 9B 00 18 	mvli 	y1, #0x0018
8678 81 79       	clr  	a               	ld   	b1, r1, +1
8679 00 66 86 7F 	loop 	m2, $0x867F
867B 35 BC       	lsf  	b, x1           	st   	r0, +m0, b1
867C 37 93       	lsf  	b, y1           	mv   	x0, b1
867D F5 00       	lsr16	b               	     	
867E 70 17       	addl 	a, x0           	mv   	y0, b1
867F 72 79       	addl 	a, y0           	ld   	b1, r1, +1
8680 02 9C 86 85 	jmpnt	$0x8685
8682 35 BC       	lsf  	b, x1           	st   	r0, +m0, b1
8683 1F 1F       	mv   	x0, b1
8684 70 00       	addl 	a, x0           	     	
8685 6D 00       	amv  	b, a            	     	
8686 00 81 04 08 	mvli 	r1, #0x0408
8688 00 9A 17 0A 	mvli 	x1, #0x170A
868A 00 98 74 89 	mvli 	x0, #0x7489
868C 48 00       	add  	a, x            	     	
868D 1B 3E       	st   	r1, +1, a1
868E 1B 3C       	st   	r1, +1, a0
868F 00 9E 05 EF 	mvli 	a1, #0x05EF
8691 1B 3E       	st   	r1, +1, a1
8692 00 9E E0 AA 	mvli 	a1, #0xE0AA
8694 1B 3E       	st   	r1, +1, a1
8695 00 9E DA F4 	mvli 	a1, #0xDAF4
8697 1B 3E       	st   	r1, +1, a1
8698 00 9E B1 57 	mvli 	a1, #0xB157
869A 1B 3E       	st   	r1, +1, a1
869B 00 9E 6B BE 	mvli 	a1, #0x6BBE
869D 1B 3E       	st   	r1, +1, a1
869E 00 9E C3 B6 	mvli 	a1, #0xC3B6
86A0 08 08       	mvsi 	x0, 8
86A1 71 31       	addl 	b, x0           	st   	r1, +1, a1
86A2 1B 3D       	st   	r1, +1, b0
86A3 1B 31       	st   	r1, +1, b2
86A4 28 D1       	stsa 	$0x00D1, a2
86A5 28 D4       	stsa 	$0x00D4, a2
86A6 28 D5       	stsa 	$0x00D5, a2
86A7 16 D6 07 FF 	stli 	$(ACEAH), #0x07FF
86A9 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
86AB 00 DE 04 04 	ldla 	a1, $0x0404
86AD 00 DC 04 05 	ldla 	a0, $0x0405
86AF 14 01       	lsfi 	a, 1
86B0 2E D8       	stsa 	$0x00D8, a1
86B1 2C D9       	stsa 	$0x00D9, a0
86B2 00 81 04 09 	mvli 	r1, #0x0409
86B4 00 82 04 0E 	mvli 	r2, #0x040E
86B6 00 85 04 10 	mvli 	m1, #0x0410
86B8 00 87 FF FE 	mvli 	m3, #0xFFFE
86BA 00 88 04 0E 	mvli 	l0, #0x040E
86BC 00 DF 04 03 	ldla 	b1, $0x0403
86BE 79 00       	dec  	b1              	     	
86BF 15 7F       	lsfi 	b, -1
86C0 1F 3F       	mv   	y0, b1
86C1 19 9D       	ld   	b0, r0, +m0
86C2 19 9A       	ld   	x1, r0, +m0
86C3 1C 65       	mv   	r3, m1
86C4 00 79 86 CF 	loop 	y0, $0x86CF
86C6 02 BF 86 E5 	call 	$0x86E5
86C8 1F B9       	mv   	b0, y0
86C9 1F 46       	mv   	x1, m2
86CA 1C 65       	mv   	r3, m1
86CB 02 BF 86 E5 	call 	$0x86E5
86CD 1F B9       	mv   	b0, y0
86CE 1F 46       	mv   	x1, m2
86CF 1C 65       	mv   	r3, m1
86D0 02 9D 86 D4 	jmpt 	$0x86D4
86D2 02 BF 86 E5 	call 	$0x86E5
86D4 00 88 FF FF 	mvli 	l0, #0xFFFF
86D6 16 C9 00 01 	stli 	$(DSCR), #0x0001
86D8 00 DE 04 06 	ldla 	a1, $0x0406
86DA 2E CE       	stsa 	$0x00CE, a1
86DB 00 DE 04 07 	ldla 	a1, $0x0407
86DD 2E CF       	stsa 	$0x00CF, a1
86DE 16 CD 04 0A 	stli 	$(DSPA), #0x040A
86E0 16 CB 00 04 	stli 	$(DSBL), #0x0004
86E2 02 BF 86 3D 	call 	$0x863D
86E4 02 DF       	rets 	
86E5 19 99       	ld   	y0, r0, +m0
86E6 19 9C       	ld   	a0, r0, +m0
86E7 1C DC       	mv   	m2, a0
86E8 14 14       	lsfi 	a, 20
86E9 38 5A       	or   	a1, x1          	ld   	y1, r2, +1
86EA F0 52       	lsl16	a               	ld   	x1, r2, +1
86EB 91 06       	asr16	a               	mr   	r2, -1
86EC 15 18       	lsfi 	b, 24
86ED 30 86       	xor  	a1, b1          	mr   	r2, -1
86EE 1F F9       	mv   	b1, y0
86EF 15 0C       	lsfi 	b, 12
86F0 30 86       	xor  	a1, b1          	mr   	r2, -1
86F1 1F 1E       	mv   	x0, a1
86F2 18 BC       	ld   	a0, r1, -1
86F3 19 3E       	ld   	a1, r1, +1
86F4 70 00       	addl 	a, x0           	     	
86F5 1A BC       	st   	r1, -1, a0
86F6 18 DF       	ld   	b1, r2, -1
86F7 31 31       	xor  	b1, x1          	st   	r1, +1, a1
86F8 F5 43       	lsr16	b               	ld   	x0, r3, +1
86F9 18 DF       	ld   	b1, r2, -1
86FA 33 00       	xor  	b1, y1          	     	
86FB 4D 63       	add  	b, a            	ld   	a0, r3, +1
86FC 76 07       	inc  	a               	mr   	r3, -1
86FD 1B 7C       	st   	r3, +1, a0
86FE 70 42       	addl 	a, x0           	ld   	x0, r2, +1
86FF 14 23       	lsfi 	a, 35
8700 14 5D       	lsfi 	a, -35
8701 7C 00       	neg  	a               	     	
8702 F0 00       	lsl16	a               	     	
8703 04 F8       	adsi 	a, -8
8704 1F 5E       	mv   	x1, a1
8705 04 28       	adsi 	a, 40
8706 6C 1E       	amv  	a, b            	mv   	y1, a1
8707 14 08       	lsfi 	a, 8
8708 1C 68       	mv   	r3, l0
8709 34 86       	lsf  	a, x1           	mr   	r2, -1
870A 37 86       	lsf  	b, y1           	mr   	r2, -1
870B 4C 52       	add  	a, b            	ld   	x1, r2, +1
870C 48 6B       	add  	a, x            	ld   	b0, r3, +1
870D 1A DC       	st   	r2, -1, a0
870E 1A 5E       	st   	r2, 0, a1
870F 18 3E       	ld   	a1, r1, 0
8710 18 BF       	ld   	b1, r1, -1
8711 33 D2       	not  	b1              	ld   	x1, r2, +1
8712 19 FB       	ld   	y1, r3, +m3
8713 36 5A       	and  	a1, y1          	ld   	y1, r2, +1
8714 37 1E       	and  	b1, y1          	mv   	y1, a1
8715 3B 1D       	or   	b1, y1          	mv   	y1, b0
8716 1A FF       	st   	r3, -1, b1
8717 18 3E       	ld   	a1, r1, 0
8718 36 79       	and  	a1, y1          	ld   	b1, r1, +1
8719 33 9E       	not  	b1              	mv   	y1, a1
871A 35 05       	and  	b1, x1          	mr   	r1, -1
871B 3B 0A       	or   	b1, y1          	mr   	r2, +1
871C 1B FF       	st   	r3, +m3, b1
871D 19 7B       	ld   	y1, r3, +1
871E 33 59       	xor  	b1, y1          	ld   	y1, r1, +1
871F 33 5A       	xor  	b1, y1          	ld   	y1, r2, +1
8720 F5 57       	lsr16	b               	ld   	x1, r3, +m3
8721 19 7F       	ld   	b1, r3, +1
8722 31 2A       	xor  	b1, x1          	st   	r2, +1, b0
8723 33 00       	xor  	b1, y1          	     	
8724 1A DF       	st   	r2, -1, b1
8725 02 DF       	rets 	
8726 8E 00       	clr  	xl              	     	
8727 00 81 08 00 	mvli 	r1, #0x0800
8729 00 92 00 FF 	mvli 	dpp, #0x00FF
872B 00 DF 04 03 	ldla 	b1, $0x0403
872D F5 00       	lsr16	b               	     	
872E 29 C9       	stsa 	$0x00C9, b2
872F 00 DE 04 00 	ldla 	a1, $0x0400
8731 2E CE       	stsa 	$0x00CE, a1
8732 00 DE 04 01 	ldla 	a1, $0x0401
8734 2E CF       	stsa 	$0x00CF, a1
8735 00 E1 FF CD 	stla 	$(DSPA), r1
8737 2D CB       	stsa 	$0x00CB, b0
8738 02 BF 86 3D 	call 	$0x863D
873A 29 D1       	stsa 	$0x00D1, b2
873B 29 D4       	stsa 	$0x00D4, b2
873C 29 D5       	stsa 	$0x00D5, b2
873D 16 D6 01 FF 	stli 	$(ACEAH), #0x01FF
873F 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
8741 00 DF 04 04 	ldla 	b1, $0x0404
8743 00 DD 04 05 	ldla 	b0, $0x0405
8745 15 7F       	lsfi 	b, -1
8746 03 60 80 00 	orli 	b, #0x8000
8748 2F D8       	stsa 	$0x00D8, b1
8749 2D D9       	stsa 	$0x00D9, b0
874A 00 80 FF D3 	mvli 	r0, #0xFFD3
874C 00 84 00 00 	mvli 	m0, #0x0000
874E 00 DF 04 03 	ldla 	b1, $0x0403
8750 15 7F       	lsfi 	b, -1
8751 1C DF       	mv   	m2, b1
8752 00 9A FF F8 	mvli 	x1, #0xFFF8
8754 00 9B 00 18 	mvli 	y1, #0x0018
8756 81 79       	clr  	a               	ld   	b1, r1, +1
8757 00 66 87 5D 	loop 	m2, $0x875D
8759 35 BC       	lsf  	b, x1           	st   	r0, +m0, b1
875A 37 93       	lsf  	b, y1           	mv   	x0, b1
875B F5 00       	lsr16	b               	     	
875C 70 17       	addl 	a, x0           	mv   	y0, b1
875D 72 79       	addl 	a, y0           	ld   	b1, r1, +1
875E 6D 00       	amv  	b, a            	     	
875F 00 81 04 08 	mvli 	r1, #0x0408
8761 00 9A 29 8F 	mvli 	x1, #0x298F
8763 00 98 0B 7F 	mvli 	x0, #0x0B7F
8765 48 00       	add  	a, x            	     	
8766 1B 3E       	st   	r1, +1, a1
8767 1B 3C       	st   	r1, +1, a0
8768 00 9E 4B F9 	mvli 	a1, #0x4BF9
876A 1B 3E       	st   	r1, +1, a1
876B 00 9E C9 B1 	mvli 	a1, #0xC9B1
876D 1B 3E       	st   	r1, +1, a1
876E 00 9E D3 0D 	mvli 	a1, #0xD30D
8770 1B 3E       	st   	r1, +1, a1
8771 00 9E 6B 99 	mvli 	a1, #0x6B99
8773 1B 3E       	st   	r1, +1, a1
8774 00 9E 19 1D 	mvli 	a1, #0x191D
8776 1B 3E       	st   	r1, +1, a1
8777 00 9E 31 DD 	mvli 	a1, #0x31DD
8779 08 12       	mvsi 	x0, 18
877A 71 31       	addl 	b, x0           	st   	r1, +1, a1
877B 1B 3D       	st   	r1, +1, b0
877C 1B 31       	st   	r1, +1, b2
877D 28 D1       	stsa 	$0x00D1, a2
877E 28 D4       	stsa 	$0x00D4, a2
877F 28 D5       	stsa 	$0x00D5, a2
8780 16 D6 07 FF 	stli 	$(ACEAH), #0x07FF
8782 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
8784 00 DE 04 04 	ldla 	a1, $0x0404
8786 00 DC 04 05 	ldla 	a0, $0x0405
8788 76 00       	inc  	a               	     	
8789 14 01       	lsfi 	a, 1
878A 2E D8       	stsa 	$0x00D8, a1
878B 2C D9       	stsa 	$0x00D9, a0
878C 00 DE 08 00 	ldla 	a1, $0x0800
878E 14 78       	lsfi 	a, -8
878F 2E DA       	stsa 	$0x00DA, a1
8790 16 A0 01 BA 	stli 	$(ADPCM_A00), #0x01BA
8792 16 A1 04 B0 	stli 	$(ADPCM_A10), #0x04B0
8794 16 A2 04 4D 	stli 	$(ADPCM_A20), #0x044D
8796 16 A3 01 E7 	stli 	$(ADPCM_A30), #0x01E7
8798 16 A4 02 DA 	stli 	$(ADPCM_A40), #0x02DA
879A 16 A5 04 52 	stli 	$(ADPCM_A50), #0x0452
879C 16 A6 05 7A 	stli 	$(ADPCM_A60), #0x057A
879E 16 A7 01 BF 	stli 	$(ADPCM_A70), #0x01BF
87A0 28 DB       	stsa 	$0x00DB, a2
87A1 28 DC       	stsa 	$0x00DC, a2
87A2 00 80 FF DD 	mvli 	r0, #0xFFDD
87A4 00 81 04 09 	mvli 	r1, #0x0409
87A6 00 82 04 0F 	mvli 	r2, #0x040F
87A8 00 85 04 10 	mvli 	m1, #0x0410
87AA 00 86 FF FF 	mvli 	m2, #0xFFFF
87AC 00 87 FF FE 	mvli 	m3, #0xFFFE
87AE 8B 00       	set  	im              	     	
87AF 8C 00       	clr  	dp              	     	
87B0 00 DE 04 03 	ldla 	a1, $0x0403
87B2 14 7D       	lsfi 	a, -3
87B3 0A 07       	mvsi 	x1, 7
87B4 C0 00       	mpy  	a1, x1          	     	
87B5 6E 00       	amv  	a, p            	     	
87B6 7A 00       	dec  	a               	     	
87B7 1F 3C       	mv   	y0, a0
87B8 19 9D       	ld   	b0, r0, +m0
87B9 18 BC       	ld   	a0, r1, -1
87BA 19 3E       	ld   	a1, r1, +1
87BB 19 DA       	ld   	x1, r2, +m2
87BC 1C 65       	mv   	r3, m1
87BD 19 9F       	ld   	b1, r0, +m0
87BE 4C 5E       	add  	a, b            	ld   	y1, r2, +m2
87BF 1A BC       	st   	r1, -1, a0
87C0 1B 3E       	st   	r1, +1, a1
87C1 00 79 87 CD 	loop 	y0, $0x87CD
87C3 02 BF 87 DF 	call 	$0x87DF
87C5 19 9D       	ld   	b0, r0, +m0
87C6 18 BC       	ld   	a0, r1, -1
87C7 19 3E       	ld   	a1, r1, +1
87C8 19 DA       	ld   	x1, r2, +m2
87C9 1C 65       	mv   	r3, m1
87CA 19 9F       	ld   	b1, r0, +m0
87CB 4C 5E       	add  	a, b            	ld   	y1, r2, +m2
87CC 1A BC       	st   	r1, -1, a0
87CD 1B 3E       	st   	r1, +1, a1
87CE 02 BF 87 DF 	call 	$0x87DF
87D0 16 C9 00 01 	stli 	$(DSCR), #0x0001
87D2 00 DE 04 06 	ldla 	a1, $0x0406
87D4 2E CE       	stsa 	$0x00CE, a1
87D5 00 DE 04 07 	ldla 	a1, $0x0407
87D7 2E CF       	stsa 	$0x00CF, a1
87D8 16 CD 04 0A 	stli 	$(DSPA), #0x040A
87DA 16 CB 00 04 	stli 	$(DSBL), #0x0004
87DC 02 BF 86 3D 	call 	$0x863D
87DE 02 DF       	rets 	
87DF 1F FC       	mv   	b1, a0
87E0 31 66       	xor  	b1, x1          	ld   	a0, r2, +m2
87E1 F5 43       	lsr16	b               	ld   	x0, r3, +1
87E2 1F FE       	mv   	b1, a1
87E3 33 76       	xor  	b1, y1          	ld   	a1, r2, +m2
87E4 4D 63       	add  	b, a            	ld   	a0, r3, +1
87E5 76 07       	inc  	a               	mr   	r3, -1
87E6 1B 7C       	st   	r3, +1, a0
87E7 70 46       	addl 	a, x0           	ld   	x0, r2, +m2
87E8 14 23       	lsfi 	a, 35
87E9 14 5D       	lsfi 	a, -35
87EA 7C 0F       	neg  	a               	mr   	r3, +m3
87EB F0 0F       	lsl16	a               	mr   	r3, +m3
87EC 04 F8       	adsi 	a, -8
87ED 1F 5E       	mv   	x1, a1
87EE 04 28       	adsi 	a, 40
87EF 6C 1E       	amv  	a, b            	mv   	y1, a1
87F0 14 08       	lsfi 	a, 8
87F1 34 85       	lsf  	a, x1           	mr   	r1, -1
87F2 37 D9       	lsf  	b, y1           	ld   	y1, r1, +1
87F3 4C 52       	add  	a, b            	ld   	x1, r2, +1
87F4 48 53       	add  	a, x            	ld   	x1, r3, +1
87F5 1B DC       	st   	r2, +m2, a0
87F6 1B 5E       	st   	r2, +1, a1
87F7 32 5F       	xor  	a1, y1          	ld   	y1, r3, +m3
87F8 30 51       	xor  	a1, x1          	ld   	x1, r1, +1
87F9 00 0A       	mr   	r2, +1
87FA F0 32       	lsl16	a               	st   	r2, +1, a1
87FB 30 05       	xor  	a1, x1          	mr   	r1, -1
87FC 32 0F       	xor  	a1, y1          	mr   	r3, +m3
87FD 1B 5E       	st   	r2, +1, a1
87FE 18 3B       	ld   	y1, r1, 0
87FF 36 53       	and  	a1, y1          	ld   	x1, r3, +1
8800 18 BF       	ld   	b1, r1, -1
8801 33 9E       	not  	b1              	mv   	y1, a1
8802 35 71       	and  	b1, x1          	ld   	a1, r1, +1
8803 3B 05       	or   	b1, y1          	mr   	r1, -1
8804 F5 57       	lsr16	b               	ld   	x1, r3, +m3
8805 19 3F       	ld   	b1, r1, +1
8806 34 5F       	and  	a1, x1          	ld   	y1, r3, +m3
8807 33 9A       	not  	b1              	mv   	x1, a1
8808 37 0A       	and  	b1, y1          	mr   	r2, +1
8809 39 2E       	or   	b1, x1          	st   	r2, +m2, b0
880A 1B 5F       	st   	r2, +1, b1
880B 02 DF       	rets 	
880C 8E 00       	clr  	xl              	     	
880D 00 81 08 00 	mvli 	r1, #0x0800
880F 00 92 00 FF 	mvli 	dpp, #0x00FF
8811 00 DF 04 03 	ldla 	b1, $0x0403
8813 05 03       	adsi 	b, 3
8814 15 6E       	lsfi 	b, -18
8815 15 02       	lsfi 	b, 2
8816 29 C9       	stsa 	$0x00C9, b2
8817 00 DE 04 00 	ldla 	a1, $0x0400
8819 2E CE       	stsa 	$0x00CE, a1
881A 00 DE 04 01 	ldla 	a1, $0x0401
881C 2E CF       	stsa 	$0x00CF, a1
881D 00 E1 FF CD 	stla 	$(DSPA), r1
881F 2D CB       	stsa 	$0x00CB, b0
8820 02 BF 86 3D 	call 	$0x863D
8822 29 D1       	stsa 	$0x00D1, b2
8823 29 D4       	stsa 	$0x00D4, b2
8824 29 D5       	stsa 	$0x00D5, b2
8825 16 D6 01 FF 	stli 	$(ACEAH), #0x01FF
8827 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
8829 00 DF 04 04 	ldla 	b1, $0x0404
882B 00 DD 04 05 	ldla 	b0, $0x0405
882D 15 7F       	lsfi 	b, -1
882E 03 60 80 00 	orli 	b, #0x8000
8830 2F D8       	stsa 	$0x00D8, b1
8831 2D D9       	stsa 	$0x00D9, b0
8832 00 80 FF D3 	mvli 	r0, #0xFFD3
8834 00 84 00 00 	mvli 	m0, #0x0000
8836 00 DF 04 03 	ldla 	b1, $0x0403
8838 03 C0 00 01 	btsth	b1, #0x0001
883A 15 7F       	lsfi 	b, -1
883B 1C DF       	mv   	m2, b1
883C 00 9A FF F8 	mvli 	x1, #0xFFF8
883E 00 9B 00 18 	mvli 	y1, #0x0018
8840 81 79       	clr  	a               	ld   	b1, r1, +1
8841 00 66 88 47 	loop 	m2, $0x8847
8843 35 BC       	lsf  	b, x1           	st   	r0, +m0, b1
8844 37 93       	lsf  	b, y1           	mv   	x0, b1
8845 F5 00       	lsr16	b               	     	
8846 70 17       	addl 	a, x0           	mv   	y0, b1
8847 72 79       	addl 	a, y0           	ld   	b1, r1, +1
8848 02 9C 88 4D 	jmpnt	$0x884D
884A 35 BC       	lsf  	b, x1           	st   	r0, +m0, b1
884B 1F 1F       	mv   	x0, b1
884C 70 00       	addl 	a, x0           	     	
884D 6D 00       	amv  	b, a            	     	
884E 00 81 04 08 	mvli 	r1, #0x0408
8850 00 9A 4E A2 	mvli 	x1, #0x4EA2
8852 00 98 1E 71 	mvli 	x0, #0x1E71
8854 48 00       	add  	a, x            	     	
8855 1B 3E       	st   	r1, +1, a1
8856 1B 3C       	st   	r1, +1, a0
8857 00 9E CC 0A 	mvli 	a1, #0xCC0A
8859 1B 3E       	st   	r1, +1, a1
885A 00 9E 14 4B 	mvli 	a1, #0x144B
885C 1B 3E       	st   	r1, +1, a1
885D 00 9E F5 41 	mvli 	a1, #0xF541
885F 1B 3E       	st   	r1, +1, a1
8860 00 9E 87 8D 	mvli 	a1, #0x878D
8862 1B 3E       	st   	r1, +1, a1
8863 00 9E A3 BC 	mvli 	a1, #0xA3BC
8865 1B 3E       	st   	r1, +1, a1
8866 00 9E 64 E4 	mvli 	a1, #0x64E4
8868 08 03       	mvsi 	x0, 3
8869 71 31       	addl 	b, x0           	st   	r1, +1, a1
886A 1B 3D       	st   	r1, +1, b0
886B 1B 31       	st   	r1, +1, b2
886C 16 D1 00 18 	stli 	$(ACFMT), #0x0018
886E 28 D4       	stsa 	$0x00D4, a2
886F 28 D5       	stsa 	$0x00D5, a2
8870 16 D6 07 FF 	stli 	$(ACEAH), #0x07FF
8872 16 D7 FF FF 	stli 	$(ACEAL), #0xFFFF
8874 00 DE 04 04 	ldla 	a1, $0x0404
8876 00 DC 04 05 	ldla 	a0, $0x0405
8878 14 01       	lsfi 	a, 1
8879 2E D8       	stsa 	$0x00D8, a1
887A 2C D9       	stsa 	$0x00D9, a0
887B 28 DA       	stsa 	$0x00DA, a2
887C 16 A0 09 78 	stli 	$(ADPCM_A00), #0x0978
887E 16 A1 E5 41 	stli 	$(ADPCM_A10), #0xE541
8880 16 DE FC 82 	stli 	$(ACGAN), #0xFC82
8882 28 DB       	stsa 	$0x00DB, a2
8883 00 80 FF DD 	mvli 	r0, #0xFFDD
8885 00 81 04 09 	mvli 	r1, #0x0409
8887 00 82 04 0F 	mvli 	r2, #0x040F
8889 00 85 04 10 	mvli 	m1, #0x0410
888B 00 86 FF FF 	mvli 	m2, #0xFFFF
888D 00 87 FF FC 	mvli 	m3, #0xFFFC
888F 28 DC       	stsa 	$0x00DC, a2
8890 00 DE 04 03 	ldla 	a1, $0x0403
8892 78 00       	dec  	a1              	     	
8893 1F 3E       	mv   	y0, a1
8894 19 9F       	ld   	b1, r0, +m0
8895 18 BC       	ld   	a0, r1, -1
8896 19 3E       	ld   	a1, r1, +1
8897 19 DA       	ld   	x1, r2, +m2
8898 1C 65       	mv   	r3, m1
8899 19 9D       	ld   	b0, r0, +m0
889A 4C 5A       	add  	a, b            	ld   	y1, r2, +1
889B 1A BC       	st   	r1, -1, a0
889C 1B 3E       	st   	r1, +1, a1
889D 00 79 88 A9 	loop 	y0, $0x88A9
889F 02 BF 88 BB 	call 	$0x88BB
88A1 19 9F       	ld   	b1, r0, +m0
88A2 18 BC       	ld   	a0, r1, -1
88A3 19 3E       	ld   	a1, r1, +1
88A4 19 DA       	ld   	x1, r2, +m2
88A5 1C 65       	mv   	r3, m1
88A6 19 9D       	ld   	b0, r0, +m0
88A7 4C 5A       	add  	a, b            	ld   	y1, r2, +1
88A8 1A BC       	st   	r1, -1, a0
88A9 1B 3E       	st   	r1, +1, a1
88AA 02 BF 88 BB 	call 	$0x88BB
88AC 16 C9 00 01 	stli 	$(DSCR), #0x0001
88AE 00 DE 04 06 	ldla 	a1, $0x0406
88B0 2E CE       	stsa 	$0x00CE, a1
88B1 00 DE 04 07 	ldla 	a1, $0x0407
88B3 2E CF       	stsa 	$0x00CF, a1
88B4 16 CD 04 0A 	stli 	$(DSPA), #0x040A
88B6 16 CB 00 04 	stli 	$(DSBL), #0x0004
88B8 02 BF 86 3D 	call 	$0x863D
88BA 02 DF       	rets 	
88BB 19 D8       	ld   	x0, r2, +m2
88BC 19 DA       	ld   	x1, r2, +m2
88BD 48 56       	add  	a, x            	ld   	x1, r2, +m2
88BE 1F FC       	mv   	b1, a0
88BF 31 56       	xor  	b1, x1          	ld   	x1, r2, +m2
88C0 F5 43       	lsr16	b               	ld   	x0, r3, +1
88C1 1F FE       	mv   	b1, a1
88C2 31 63       	xor  	b1, x1          	ld   	a0, r3, +1
88C3 76 07       	inc  	a               	mr   	r3, -1
88C4 1B 7C       	st   	r3, +1, a0
88C5 70 46       	addl 	a, x0           	ld   	x0, r2, +m2
88C6 14 23       	lsfi 	a, 35
88C7 14 6D       	lsfi 	a, -19
88C8 1F 5E       	mv   	x1, a1
88C9 04 E0       	adsi 	a, -32
88CA 00 1F       	mr   	r3, +m3
88CB 6C 1E       	amv  	a, b            	mv   	y1, a1
88CC 34 85       	lsf  	a, x1           	mr   	r1, -1
88CD 37 D9       	lsf  	b, y1           	ld   	y1, r1, +1
88CE 4C 52       	add  	a, b            	ld   	x1, r2, +1
88CF 48 53       	add  	a, x            	ld   	x1, r3, +1
88D0 1B DC       	st   	r2, +m2, a0
88D1 1B 5E       	st   	r2, +1, a1
88D2 32 5F       	xor  	a1, y1          	ld   	y1, r3, +m3
88D3 30 51       	xor  	a1, x1          	ld   	x1, r1, +1
88D4 00 0A       	mr   	r2, +1
88D5 F0 32       	lsl16	a               	st   	r2, +1, a1
88D6 30 05       	xor  	a1, x1          	mr   	r1, -1
88D7 32 00       	xor  	a1, y1          	     	
88D8 1B 5E       	st   	r2, +1, a1
88D9 18 3F       	ld   	b1, r1, 0
88DA 33 9E       	not  	b1              	mv   	y1, a1
88DB 18 BE       	ld   	a1, r1, -1
88DC 37 53       	and  	b1, y1          	ld   	x1, r3, +1
88DD 34 1F       	and  	a1, x1          	mv   	y1, b1
88DE 3A 79       	or   	a1, y1          	ld   	b1, r1, +1
88DF F4 05       	lsr16	a               	mr   	r1, -1
88E0 33 D3       	not  	b1              	ld   	x1, r3, +1
88E1 35 71       	and  	b1, x1          	ld   	a1, r1, +1
88E2 00 09       	mr   	r1, +1
88E3 18 3B       	ld   	y1, r1, 0
88E4 36 1B       	and  	a1, y1          	mv   	x1, b1
88E5 38 7A       	or   	a1, x1          	ld   	b1, r2, +1
88E6 18 DD       	ld   	b0, r2, -1
88E7 4C 05       	add  	a, b            	mr   	r1, -1
88E8 1B 5E       	st   	r2, +1, a1
88E9 1A 5C       	st   	r2, 0, a0
88EA 02 DF       	rets 	
88EB 00 00       	nop  	
88EC 00 00       	nop  	
88ED 00 00       	nop  	
88EE 00 00       	nop  	
88EF 00 00       	nop  	
... [Address] = Address (filler)
```

## Possible checksum

```
8FFE 06 E2
8FFF 88 45
```
