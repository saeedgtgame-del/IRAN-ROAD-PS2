# IRAN ROAD — Iranian 3D Driving Game for PS2

A native PlayStation 2 homebrew project written in C with PS2SDK and gsKit. It is not an HTML/browser game.

## Current repository contents
- `src/main.c`: early perspective driving prototype and controller loop.
- `src/vehicles.h`: ten Iranian vehicle gameplay profiles, per-car base speed/acceleration, five engine upgrade stages, and paint palette.
- `Makefile`: PS2SDK build target.
- `.github/workflows/ps2-build.yml`: automated build using the PS2 development container (installs GNU Make in the Alpine image).

## Vehicle roster
Samand LX, Peugeot 206, Peugeot 207, Samand Soren, Dena Plus, Peugeot Pars, Peugeot 405, Pride, Shahin, and Nissan Junior (blue pickup).

Each vehicle has a different base top speed and acceleration. Engine upgrade stages add performance, with a gameplay cap of 350 km/h.

## Current controller mapping

| PS2 button | Action |
|---|---|
| R2 | Accelerate |
| L2 | Brake |
| Left analog stick | Steer smoothly |
| Select | Select next car |
| Start | Open / close engine upgrade garage |
| X (inside garage) | Buy next engine upgrade |
| Circle (inside garage) | Close upgrade garage |
| L1 | Cycle dry / rain / fog |
| Circle, three quick presses | Send a nearby traffic challenge signal |
| R1 | Toggle headlights on / off |
| Triangle | Toggle day / night |

The upper-right blue/green bar indicates night/day mode; the yellow strip below it indicates headlights. Paint color cycling is not currently mapped to a controller button. The three-honk feature is only a visual challenge-state prototype; horn audio and full rival race behavior are not implemented yet.

## Build
Requires PS2SDK, gsKit, dmaKit and pad libraries. Run `make` in a PS2SDK environment. The target is `IRANROAD.ELF`.

An ELF is not an OPL ISO. ISO packaging and testing on PCSX2 or a real PS2 are separate steps.

## Important status
This is an early prototype and vehicle-data foundation, **not a finished game**. The current renderer uses simple low-polygon projected shapes. Vehicle selection, paint cycling, engine-stage performance values and a basic triple-honk challenge state are wired into the prototype. Dedicated accurate Iranian car meshes, full garage/customization menus, advanced traffic AI, three-honk race invitations, detailed weather, audio, collision polish, and final ISO packaging still need implementation and testing. A successful build must be confirmed from the latest Actions run.
