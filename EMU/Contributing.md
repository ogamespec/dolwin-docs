# Contributing

## Introduction

Dolwin on the one hand is an experimental emulator which aims to create easy to read and well documented source code so that programmers who are interested in emulation can use it as tutorial material.

On the user side, Dolwin is a casual emulator. There are currently no plans to add all sorts of frills, such as cheats, network play, TAS tools, video recording and the like.
The reason is that it would require adding a lot of dependencies into the source code, and since the emulator is positioned as a learning project, it won't contribute to that goal.
Also, we don't want to turn into a "borg" emulator, like Dolphin-emu, which contains poorly structured code in huge quantities.

Also, what is not planned (most likely on this list is exactly what you would like):
- Wii/Triforce support. Dolwin - GameCube emulator.
- Support for compressed disc images.

## What's Happening Now

You can check out all the major goals for upcoming versions on the Milestones page: https://github.com/ogamespec/dolwin/milestones

In a nutshell, the most undeveloped parts of the emulator are being strengthened now:
- A recompiler to speed up Gekko emulation is being created
- Work is underway to improve graphics emulation
- Working on a new DSP core, which still has bugs

After these works are done we can already talk about launching and testing the games and compatibility list.

## How you can help

Now exactly what area you can conquer, if you are an experienced developer and want to take part in development.

Take your pick :)

- Writing unit tests. Currently, unit tests reside in RnD's auxiliary repository: https://github.com/ogamespec/dolwin-rnd
- Strengthening source code security. There is a possibility of exploiting the emulator using malicious disk images (GCMs). Although Dolwin doesn't make any guarantees, following CC0-1.0 license, I don't want any crap to get through the emulator.
- Figuring out the reasons for the bugs. There is a bug with DSP that doesn't allow to run IPLs at the moment (see https://github.com/ogamespec/dolwin/issues/1 )
- Try to run GC-Linux. Ideally, it would be good if you were one of the GC-Linux developers from 2005, so that you can figure out why it doesn't boot any faster
- Develop a new debug UI. The Console UI we have had since version 0.10 is not very usable right now. Some kind of more modern UI would be cool.

And of course you can come up with your own goals and objectives or pick up the current development.

## Note for UI developers

A special service interface called JDI is used to interact between Dolwin core and UI.

The project includes a special simplified version of the emulator - Playground, in which you can find an example of interaction with the emulator core.

So, you don't need to put the UI sources, which will certainly contain a lot of dependencies, into the main project. You can make a separate repository for this purpose.

## Where to start

You can read the Dolwin Quick Start guide: https://github.com/ogamespec/dolwin/blob/master/Dolwin_Quick_Start.md
