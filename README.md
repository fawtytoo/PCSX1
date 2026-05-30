# PCSX1

## About

This is a Sony Playstation 1 emulator based on PCSX-Reloaded.

The emulator is monolithic and requires no plugins.

## Interface

Designed to be as simple to use as possible with emphasis on a clean interface and simple to use configuration files.
In order to maintain emulation quality and as bug free experience as possible, there will be no added features; no cheat system, no snapshots, etc.

See `pcsx1 -keys` for options.

## Joysticks/Mouse

There is support for joysticks but only tested with official Sony Playstation
joystick with USB adapter.

Full mouse support.

## Loading games

PCSX1 is driven via configuration files and command line switches. Configuration files specify disc images, video and audio options, controller choice, etc.
Multiple configuration files can be specified, such as using PCSX1.CFG as a base and a 2nd file for a specific game.

`pcsx1 -cfg PCSX1.CFG -cfg Tombraider1.cfg`

See `PCSX1.CFG` for example.

## Games

Games need to be ripped to bin/toc format. Both NTSC and PAL are supported.

## Future plans?

- Improve emulation quality and performance.
- Extend controller support beyond official PS1 controllers.
- Replace graphics code for high resolution output.
- To phase out any "fixes" needed for games.

There are no plans for Windows or Mac.
