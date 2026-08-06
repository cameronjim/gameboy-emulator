# gbemu

A Game Boy emulator. It's a program that behaves exactly like the original
1989 Nintendo Game Boy, so real Game Boy games run inside it on your PC.

**What that means:** the "engine" here is the emulator itself — a from-scratch
software copy of the Game Boy's chips (CPU, screen, sound, cartridge). Games
are ordinary `.gb` files that run on top of it unmodified, like discs in a
virtual console. This project is the console; the games come from elsewhere.

## How to play (Windows)

1. Open the `dist` folder. It has two files: `gbemu-sdl.exe` and `SDL2.dll`.
   Keep them together. You can copy this folder anywhere.
2. **Drag a `.gb` game file onto `gbemu-sdl.exe`.** That's it — a window opens
   and the game runs.
3. A sample game (`tetris.gb`, a free Tetris-style game — see note below) is
   included so you have something to play immediately.

Double-clicking the exe *without* a game shows a striped test screen — that's
normal, it just means no game is loaded.

## Controls

The Game Boy had 8 buttons. They map to your keyboard like this:

| Your keyboard      | Game Boy button |
| ------------------ | --------------- |
| Arrow keys         | D-pad (up/down/left/right) |
| `Z`                | A button (confirm / rotate) |
| `X`                | B button (cancel) |
| `Enter`            | Start (start game / pause menu) |
| `Right Shift`      | Select |

## Extra keys (emulator features, not Game Boy buttons)

- `Esc` — quit.
- `P` — pause the emulator. Press again to resume.
- Hold `Tab` — fast-forward (runs the game at 4x speed, sound muted while held).
- `F5` — save state: snapshots the *entire* game exactly as it is right now.
- `F8` — load state: jumps back to your last `F5` snapshot instantly.
- `T` — tile viewer: a second window showing the game's raw graphics tiles
  (a debugging tool — fun to peek at, safe to close).
- `F10` — saves a screenshot as `framebuffer.ppm` next to the exe.
- Drag the window edges to make it any size you like.

## Saving

- Games that had a battery save on real hardware (Zelda, Pokémon...) save
  automatically to a `.sav` file next to the game file. It reloads next launch.
- Save states (`F5`/`F8`) are separate and go to a `.state` file.

## Options (optional, for the curious)

Run from a terminal to use these:

- `--palette blue|green|gray` — color scheme. `blue` (cream & navy) is the
  default; `green` is the classic Game Boy look.
- `--volume 0-100` — sound volume (default 40).

## About the included game

Real Tetris is copyrighted, so it is not included. `tetris.gb` is actually
**Adjustris**, a free Tetris-style game by Dave VanEe (tbsp), released to the
public domain — renamed here for convenience. If you own a real Tetris
cartridge dump, just drag that file onto the exe instead.

## For developers

C++20, SDL2, no other dependencies. Build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

Tests: `ctest --test-dir build` plus `./tools/fetch-test-roms.sh` and
`./tools/run-test-roms.sh` for the rom gates (blargg, dmg-acid2, mooneye).
A browser build exists too: `emcmake cmake -B build-wasm` produces a static
page in `build-wasm/`. Docs live in `claude-docs/`.
