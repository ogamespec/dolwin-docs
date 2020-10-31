# Universal von Neumann Approach (UVNA)

The concept that Dolwin emulator uses to emulate processors (DSP, Gekko).

## Foreword

All modern hardware architectures are built on the von Neumann architecture (https://en.wikipedia.org/wiki/Von_Neumann_architecture).

And since this is a universal approach, the approach to emulation can also be made universal.

## UVNA in a Nutshell

Emulation of any processor can be represented as the following diagram:

![UVNA](UVNA.png)

- The stream of instructions of architecture `A` is fed to the analyzer input
- At the output of the analyzer, a structure is obtained, which contains all information about the current instruction (`AnalyzeInfo`)
- The `AnalyzeInfo` structure can be used by consumers: disassembler, interpreter, recompiler or assembler

## Disassembler

Translates information into a human-readable format (string).

## Interpreter

Emulates the execution of instructions, with the required accuracy, as is done on a real processor.

## Recompiler

Converts the `AnalyzeInfo` structure of the `A` architecture to the `AnalyzeInfo` structure of the `B` architecture.

## Assembler

Does the opposite transformation: from the structure `AnalyzeInfo`, the executable code of the corresponding architecture is obtained.
