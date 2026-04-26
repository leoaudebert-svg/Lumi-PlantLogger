# PlantLogger Contributor Notes

PlantLogger is a fresh M5Stack Atom S3R project for plant monitoring and logging.
Keep the codebase small, readable, and focused on the hardware and features that
are actually present in this repository.

## Current Scope

- M5Stack Atom S3R using the Arduino framework through PlatformIO.
- M5Unified/M5GFX for display, button state, and device initialization.
- A small button wrapper in `include/button.h` and `src/drivers/button.cpp`.
- `src/main.cpp` is intentionally a minimal startup sketch.

## Coding Guidelines

- Prefer simple, direct implementations until a repeated pattern appears.
- Keep hardware-specific code behind small wrappers when it improves clarity.
- Use `M5.update()` before reading button state.
- Avoid blocking long-running work in `loop()`.
- Use ESP32 logging macros such as `ESP_LOGI`, `ESP_LOGW`, and `ESP_LOGE`.
- Add new dependencies only when a committed feature needs them.

## Project Layout

```text
include/          Public headers and small module interfaces
src/              Application code
src/drivers/      Hardware wrappers
docs/             Project notes and setup documentation
data/             Optional filesystem assets, if the project later needs them
```
