# Dolwin 0.131 Release Notes

Intermediate release related to source code refactoring. It was decided to make the emulator cross-platform.

Now the whole core is in a separate executable module (for Windows it is DolwinEmu.dll), and the user interface and debugger in the main executable file.

From the user side no special improvements are noticeable, games still show only splash screens at best :-)

There may be various bugs in the interface after refactoring, they will be fixed over time.

## For developers

The source tree has changed a bit:
- The whole debug console interface went to the UI folder
- Backends folder formed

For more details, you can read the Readme for the specified folders.

If you have any questions, visit official Discord channel: https://discord.gg/Ehz8PYA
