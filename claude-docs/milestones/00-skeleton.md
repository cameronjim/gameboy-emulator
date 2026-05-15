# milestone 00 — skeleton, cmake, core/frontend split

**branch:** `feat/00-skeleton` · **plan/review:** fable · **implement:** sonnet
**depends on:** nothing · **effort:** 1 weekend

## goal

a building repo with the core/frontend boundary locked in from day one. this boundary is the whole reason the wasm port (milestone 14) is a weekend instead of a rewrite.

## scope

**in:** cmake project, `gbcore` static lib, `gbemu-sdl` executable, dummy framebuffer round-trip, catch2 wired, ci workflow, clang-format config, `tools/` stubs.
**out:** any emulation. no cpu, no cartridge, nothing that reads a rom.

## spec

- top-level `CMakeLists.txt`; targets:
  - `gbcore`: static lib from `core/*.cpp`. no dependencies. `-Wall -Wextra -Werror`; debug adds asan/ubsan.
  - `gbemu-sdl`: `frontend/sdl/main.cpp`, links `gbcore` + sdl2 (find_package).
  - `gbcore_tests`: catch2 v3 via fetchcontent, registered with ctest.
- `core/gameboy.hpp/.cpp` facade with the api from architecture.md; for now `run_frame()` fills the 160×144 index buffer with a fixed test pattern (four vertical bands 0..3) and `load_rom` stores bytes and returns true.
- sdl frontend: create window (640×576), renderer with vsync, streaming texture 160×144 argb8888; each loop: pump events (esc/close quits), `run_frame`, map indices through a 4-entry rgb palette, update texture, present.
- `.clang-format`, `.gitignore` (build/, tests/roms/vendor/), github actions: build both configs, ctest, format check.
- one smoke unit test: framebuffer size is 23040 and values ∈ 0..3.

## files

`CMakeLists.txt`, `core/gameboy.{hpp,cpp}`, `frontend/sdl/main.cpp`, `tests/unit/smoke_test.cpp`, `.clang-format`, `.gitignore`, `.github/workflows/ci.yml`, `tools/fetch-test-roms.sh` (stub), `tools/run-test-roms.sh` (stub).

## tests

- `framebuffer_has_expected_size_and_range`
- ci green on linux, debug + release.

## done when

window opens showing the four-band pattern from the core's buffer, closes on esc, ctest passes, ci green.

## traps

- sdl types leaking into `core/` "just for now" — the one unforgivable sin of this milestone.
- over-engineering the api: no ibus/idevice interface hierarchies. one facade class, plain methods.
- cmake perfectionism. it builds on linux with system sdl2; move on.
