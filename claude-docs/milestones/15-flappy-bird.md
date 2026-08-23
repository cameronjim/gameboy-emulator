# milestone 15 — flappy bird, our first game rom

**branch:** main (direct) · **plan/review:** fable · **implement:** opus (game logic), sonnet (assets/boilerplate)
**depends on:** 00-14 (the emulator is the test rig) · **effort:** 2-3 sessions

## why c, not c++

the emulator executes sm83 machine code; the game must be a real game boy rom.
no c++ toolchain targets the sm83 — the options are rgbds assembly or c via
gbdk-2020 (sdcc backend). we use **gbdk-2020 c**. our c++ stays on the host
side: the emulator itself is the headless test harness that boots the rom,
scripts button input, and asserts on the framebuffer. the core is fully
deterministic (no rng, no clocks), so one input script produces identical
frames forever — game tests are exact.

## layout

- `games/flappy/` — `main.c`, `assets.c/h` (tiles as arrays), game source only
- `games/flappy/tests/flappy_test.cpp` — catch2, links gbcore, drives the rom
- cmake: `GBDK_HOME` (env or cache var) enables the rom target; absent → skip
  with a warning, exactly like the sdl2-less build. ci downloads gbdk-linux64.

## cart spec

- 32kb rom, mbc1+ram+battery (type 0x03), 8kb sram: best score persists via
  the frontend's existing `.sav` flow. title `FLAPPY`.

## testability rules (design for the harness, decided up front)

- bird is the only moving sprite; tests track it as min-y over sprite pixels
  (`framebuffer_tiles()` bit 8).
- score hud digits use dedicated tile ids `0xD0`-`0xD9` at fixed bg cells, so
  tests read the score straight out of `framebuffer_tiles()`.
- pipe columns use dedicated tile ids; tests detect scroll by watching them.
- rng: frame counter sampled at first flap. deterministic per input script.

## sub-milestones (one push each, 3-4 commits, ci green before the next)

1. **skeleton**: toolchain wiring, rom that boots to a title card, cmake rom
   target + test target, ci job. tests: header parses, rom loads, framebuffer
   non-blank after 60 frames.
2. **bird**: gravity, flap on a, floor/ceiling clamp, title → play on start.
   tests: bird falls without input; flap moves it up; it never leaves screen.
3. **world**: scrolling pipes with gapped columns, collision, game over,
   restart. tests: pipes advance left; flying into a pipe reaches game over;
   game over freezes the bird; start restarts.
4. **score + polish**: bcd score on pipe pass, best score in sram, flap/score/
   hit sfx, title art. tests: hud increments after passing a gap; sfx produce
   nonzero audio samples; best score survives save/load of external ram.

## physics (tuning is a manual gate, structure is not)

8.8 fixed point y and velocity. gravity ~0.20 px/frame², flap sets vy to
~-2.2 px/frame, terminal vy ~+3.5. tweak by feel later; tests assert
direction and bounds, never exact positions.

## traps

- sdcc is c99-ish: no vla, sparse const-correctness, 8-bit int math traps —
  cast before multiply.
- oam writes only via shadow oam + dma (gbdk handles it; do not poke oam
  mid-frame).
- don't read the score for tests out of wram — screen tiles are the contract.
- vblank-driven main loop (`vsync()`); logic must fit the frame budget.
- keep game constants in one header so tuning doesn't touch logic.

## done when

playable flappy bird: title, flap, pipes, death, restart, score, best score
persists, sfx audible. all game tests green in ci next to the emulator's own.
