# Dolwin 0.14 Release Notes

This release is the beginning of work on improving the graphics system.

- A new component has been added to the emulator - GX, which contains the general code for emulating the command processor (CP)
- Old DolwinVideo code has been cleaned up from old stuff. It is still used as a reserve parachute until a more advanced backend appears.
- In parallel, improvements have been made in DSP emulation, but there is still a lot of work to be done in this area
- Fixed minor bugs that got out after splitting the emulator core and user interface.

Some games show slight progress: Luigi's Mansion now shows 1 screen further from the main menu, and Wario Ware even reaches the game, but with big slowdowns and graphical bugs. You can even hear how Wario is unhappy with this circumstance (there is sound in the game).
