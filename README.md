# gbemu

A Game Boy emulator. It's a program that behaves exactly like the original
1989 Nintendo Game Boy, so real Game Boy games run inside it on your PC.

**What that means:** the "engine" here is the emulator itself — a from-scratch
software copy of the Game Boy's chips (CPU, screen, sound, cartridge). Games
are ordinary `.gb` files that run on top of it unmodified, like discs in a
virtual console. This project is the console; the games come from elsewhere.

## How to play (Windows)

Three ways to start Tetris:

- Type **tetris** in the Windows search bar and hit Enter (Start Menu shortcut).
- Type **tetris** in any terminal.
- **Drag any `.gb` game file onto `dist\gbemu-sdl.exe`** to play other games.

The `dist` folder is the whole app (`gbemu-sdl.exe` + `SDL2.dll` + `tetris.gb`).
Keep the files together; the folder can be copied anywhere.

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

## Playing the included Tetris

- **Title screen:** Up/Down picks a piece set (1-4 built in, 5-8 are ones you
  make). Enter starts the game.
- **In game:** arrows move, `Z`/`X` rotate each way.
- **Pause menu / back button:** press `Enter` — the game shows a menu.
  `Enter` again resumes, `Right Shift` quits back to the title screen.
- **Piece editor** ("Edit Sets" on the title screen):
  - Arrows move the cursor, `Z` confirms / toggles blocks, `X` cancels.
  - **Hold Right Shift and press Left/Right to switch between pieces** —
    this is the "go the other way" control.
  - Piece count is changed with the **Insert Piece** and **Delete Piece**
    icons, not by scrolling a number.
  - Save Set stores your creation in slots 5-8; it persists between sessions.

## Saving

- Games that had a battery save on real hardware (Zelda, Pokémon...) save
  automatically to a `.sav` file next to the game file. It reloads next launch.
- Save states (`F5`/`F8`) are separate and go to a `.state` file.

## The look

One fixed style: menus are pure black with thin white text; in game, each
piece keeps one color for its life, blocks get a classic bevel, and the empty
play area shows a faint beveled grid. Other games render clean white-on-black.

## Window icon

Put a 32x32 `gbemu.bmp` next to `gbemu-sdl.exe` to give the window and
taskbar an icon. A game can override it with its own: name the file after the
game plus `.icon.bmp` (for example `tetris.gb.icon.bmp` next to `tetris.gb`).
Ready-made icons live in `assets/icons/` — copy the one you like.

## Options (optional, for the curious)

Run from a terminal to use these:

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
