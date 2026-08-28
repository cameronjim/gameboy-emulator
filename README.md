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
| Arrows or `WASD`   | D-pad (up/down/left/right) |
| `F` or `Space` (or `Z`) | A button (confirm / rotate / flap) |
| `G` (or `X`)       | B button (cancel) |
| `E` or `Enter`     | Start (start game / pause menu) |
| `Esc`              | Back: leaves credits/high scores, opens the pause menu in game |
| `R`                | Select (also `Backspace`; `Right Shift` works too but Windows drops shift while arrows are held) |

## Extra keys (emulator features, not Game Boy buttons)

- `P` — pause the emulator. Press again to resume.
- Quitting: close the window (or `Alt+F4`). No key quits the app by accident.
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
- **In game:** arrows or `WASD` move, `F`/`G` rotate each way.
- **Pause menu / back button:** press `Enter` — the game shows a menu.
  `Enter` again resumes, `R` quits back to the title screen.
- **Piece editor** ("Edit Sets" on the title screen):
  - Arrows move the cursor, `F` confirms / toggles blocks, `G` cancels.
  - **Hold R and press Left/Right (or A/D) to switch between pieces** —
    this is the "go the other way" control.
  - Piece count is changed with the **Insert Piece** and **Delete Piece**
    icons, not by scrolling a number.
  - The SPIN row shows a check mark when the piece spins all the way around,
    or an X when it wobbles (rotates once, then back).
  - Save Set stores your creation in slots 5-8; it persists between sessions.

## Saving

- Games that had a battery save on real hardware (Zelda, Pokémon...) save
  automatically to a `.sav` file next to the game file. It reloads next launch.
- Save states (`F5`/`F8`) are separate and go to a `.state` file.

## The look

One fixed style: menus are pure black with thin white text; in game, every
piece wears its shape's classic color (cyan I, yellow O, purple T, green S,
red Z, blue J, orange L) from the moment it appears and never changes, blocks
get a classic bevel, and the empty play area shows a faint beveled grid.
Other games render clean white-on-black.

## Window icon

Put a 32x32 `gbemu.bmp` next to `gbemu-sdl.exe` to give the window and
taskbar an icon. A game can override it with its own: name the file after the
game plus `.icon.bmp` (for example `tetris.gb.icon.bmp` next to `tetris.gb`).
The default icon lives at `assets/icons/gbemu.bmp` (a little Game Boy).

## Options (optional, for the curious)

Run from a terminal to use these:

- `--volume 0-100` — sound volume (default 25).

## Flappy Bird (our own game)

`games/flappy/` is a from-scratch Flappy Bird written in C with gbdk-2020 —
a real Game Boy rom that happens to be born on this emulator. Launch it like
any game (`gbemu-sdl.exe flappy.gb` or drag the file onto the exe).

- `F` or `Space` flaps (and starts the round — the run begins on your first
  flap, so you set the pace).
- Pass a pipe for a point; touch a pipe or the ground and it's over.
- Your best score saves automatically (battery save, `.sav` file).

To build the rom yourself, install [gbdk-2020](https://github.com/gbdk-2020/gbdk-2020)
and configure with `-DGBDK_HOME=<path>`; the `flappy` target and its emulator
driven tests appear automatically.

## Crossy Road (our own game, too)

`games/crossy/` is an endless road-crosser in the spirit of Crossy Road,
also written from scratch in gbdk-2020 C. Launch `crossy.gb` like any game.

- Arrows or `WASD` hop the chick one cell in any direction.
- Grass is safe (trees block), roads have traffic, rivers must be crossed on
  drifting logs, and train tracks give you one blinking warning before the
  train. It all gets faster the further you go.
- Don't dawdle: idle too long or fall behind the scrolling camera and the
  eagle takes you.
- Score is the furthest lane you reach; your best saves automatically.

The same `-DGBDK_HOME` configure flag builds it and its tests.

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
