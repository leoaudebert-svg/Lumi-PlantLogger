# PlantLogger Fresh Start

This repository is now a clean starting point for PlantLogger.

## What Is Included

- PlatformIO configuration for the M5Stack Atom S3R.
- M5Unified/M5GFX dependencies for device setup, display work, and button state.
- A small wrapper around the built-in M5 button.
- A minimal `src/main.cpp` that initializes the device and display.

## First Features To Add

- Decide which sensors PlantLogger will use.
- Add pin and hardware constants only after the hardware choice is final.
- Create focused drivers under `src/drivers/`.
- Add small test sketches under `src/tests/` when a hardware feature needs one.

## Build

Use PlatformIO:

```sh
pio run
```

If the project later needs filesystem assets, add them under `data/` and enable
the filesystem option in `platformio.ini`.
