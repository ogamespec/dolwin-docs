# DolwinPlayground for Ubuntu (0.132)

This is the first attempt at porting Dolwin to Linux-like operating systems.

Let me remind you that Playground is a "smaller" version of Dolwin, without a graphical interface and with Null backends (that is, no graphics, sound, etc.). All that is visible to the user is the emulator debug messages.

Porting was done in WSL2 environment on Windows, Ubuntu version 18.04.

## Launch

Command line example:

```
./DolwinPlayground ./pong.dol
```

## Build

Those who have been building projects in Linux for a long time should not encounter any problems:

```
cd ~
git clone https://github.com/ogamespec/dolwin.git
cd dolwin
cmake ./
cmake --build ./
```

After that, you can copy the resulting executable file `DolwinPlayground` from the folder `SRC/UI/Playground/Scripts` to the folder `~/dolwin` where the folder `Data` is located, which is needed for the emulator to work.

## Why there are so many .so files

There was a problem while building with static c++ classes that are used from different libraries. For some reason these classes ended up in each of the libraries and the static singleton constructor provoked Segfault.

Who knows how to properly link static libraries with static c++ classes (singletons) - please write in Discord.

## Thanks

I would like to express special gratitude to `edgbla` for the valuable tips in porting the code and for the friendly chat.
