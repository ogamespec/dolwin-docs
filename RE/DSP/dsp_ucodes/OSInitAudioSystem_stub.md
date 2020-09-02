# Disassembled OSInitAudioSystem dsp_stub

```
0000 02 9F 00 10 	jmp  	$0x0010
0002 02 9F 00 35 	jmp  	$0x0035
0004 02 9F 00 36 	jmp  	$0x0036
0006 02 9F 00 37 	jmp  	$0x0037
0008 02 9F 00 38 	jmp  	$0x0038
000A 02 9F 00 39 	jmp  	$0x0039
000C 02 9F 00 3A 	jmp  	$0x003A
000E 02 9F 00 3B 	jmp  	$0x003B
```

## Reset

```
0010 12 06       	clr  	et
0011 12 03       	clr  	te1
0012 12 04       	clr  	te2
0013 12 05       	clr  	te3
0014 8E 00       	clr  	xl              	     	
0015 00 92 00 FF 	mvli 	dpp, #0x00FF

0017 00 80 80 00 	mvli 	r0, #0x8000  				// Probe IROM
0019 00 88 FF FF 	mvli 	l0, #0xFFFF
001B 00 84 10 00 	mvli 	m0, #0x1000
001D 00 64 00 20 	loop 	m0, $0x0020
001F 02 18       	pld  	a, r0, +1
0020 00 00       	nop

0021 00 80 10 00 	mvli 	r0, #0x1000 				// Probe DROM
0023 00 88 FF FF 	mvli 	l0, #0xFFFF
0025 00 84 08 00 	mvli 	m0, #0x0800
0027 00 64 00 2A 	loop 	m0, $0x002A
0029 19 1E       	ld   	a1, r0, +1
002A 00 00       	nop

002B 26 FC       	ldsa 	a1, $0x00FC 			// DMBH
002C 02 A0 80 00 	btstl	a1, #0x8000
002E 02 9C 00 2B 	jmpnt	$0x002B

0030 16 FC 00 54 	stli 	$(DMBH), #0x0054
0032 16 FD 43 48 	stli 	$(DMBL), #0x4348
0034 00 21       	wait 	
```

```c++
void Reset () 		// 0x0010
{
	// Disable all interrupts

	psr.et = 0;
	psr.te1 = 0;
	psr.te2 = 0;
	psr.te3 = 0;

	xl = 0; 		// a/b acts as a1/b1.
	dpp = 0xFF;

	// Probe 0x8000 reads (8 Kbytes)  IROM

	r0 = 0x8000;
	l0 = 0xFFFF;
	m0 = 0x1000;
	while (m0--)
	{
		a1 = *r0++; 			// via pld instruction
	}

	// Probe 0x1000 reads (4 Kbytes)   DROM

	r0 = 0x1000;
	l0 = 0xFFFF;
	m0 = 0x800;
	while (m0--)
	{
		a1 = *r0++;
	}

	// Send a message that everything is ready and go to sleep.

	while ( (DMBH & 0x8000) != 0 ) ;

	DMBH = 0x0054; 		// 'TCH'
	DMBL = 0x4348;

	wait;
}
``` 	

## Interrupt stubs

```
0035 02 FF       	reti 	
0036 02 FF       	reti 	
0037 02 FF       	reti 	
0038 02 FF       	reti 	
0039 02 FF       	reti 	
003A 02 FF       	reti 	
003B 02 FF       	reti 	
003C 00 00       	nop
003D 00 00       	nop  	
003E 00 00       	nop  	
003F 00 00       	nop  	
```
