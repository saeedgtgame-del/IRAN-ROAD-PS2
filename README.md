# IRAN ROAD — PS2 3D Driving Prototype

Native PlayStation 2 homebrew driving prototype written in C with PS2SDK and gsKit. This is a real-time perspective 3D prototype, not an HTML game.

## Current prototype
- Perspective road and lane markings
- Player car assembled from projected 3D faces
- Seven traffic cars with different colors and movement rates
- Acceleration, braking and steering
- Headlight toggle, day/night toggle and weather state
- Speed simulation capped at 350 km/h

## Controls
| PS2 button | Action |
|---|---|
| X | Accelerate |
| Square | Brake |
| Left / Right | Steer |
| R1 | Headlights on |
| L1 | Headlights off |
| Triangle | Day / night |
| Circle | Cycle weather |

## Build
Requires a PS2 development toolchain and PS2SDK with gsKit, dmaKit and pad libraries. Run make in the project directory.

The output target is IRANROAD.ELF. An ELF is a homebrew executable, not yet a bootable ISO. OPL-compatible ISO packaging requires a separate disc-image build step and testing on PCSX2 or real hardware.

## Status
Early 3D prototype source. It has not yet been compiled or tested on PS2 hardware. Detailed Iranian car meshes, customization menus, vehicle-specific tuning, advanced traffic AI, weather rendering and final ISO packaging remain planned work.
