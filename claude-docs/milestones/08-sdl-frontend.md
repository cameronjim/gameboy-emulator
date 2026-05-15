# milestone 08 — sdl2 frontend, first picture

**branch:** `feat/08-sdl` · **plan/review:** fable · **implement:** sonnet
**depends on:** 07 · **effort:** 1 weekend

## goal

the emulator becomes a thing you can see. tetris title screen in a window. also the tile viewer — twenty lines that will save days.

## spec

- replace the milestone-00 dummy loop with the real one: pump events → `run_frame()` → map indices through palette → `SDL_UpdateTexture` (streaming, argb8888, 160×144) → `SDL_RenderCopy` integer-scaled → present.
- pacing v1: create renderer with `SDL_RENDERER_PRESENTVSYNC`, accept 60 hz (true rate is 59.73 — a real frame timer is a later qol; audio takes over pacing in milestone 12).
- palettes: classic green dmg set default, grayscale via `--palette gray`.
- keys: esc quit, `p` dump framebuffer ppm, `t` toggle tile viewer.
- **tile viewer**: second small window rendering all 384 tiles from 0x8000–0x97FF as a 16×24 grid via the core debug accessor. bisects every graphics bug: tiles right here + screen wrong = tilemap/scroll bug; tiles wrong here = decode or vram-write (cpu/bus) bug.

## tests

frontend is thin by design; core stays covered by unit tests. add:
- `palette_mapping_covers_all_four_indices` (pure helper, unit-testable)
- manual gate: tetris title screen visible and stable for 30s; tile viewer shows the tetris font/tiles.

## done when

tetris copyright + title screens render in the window. screenshot goes in the pr. this is the milestone where the project becomes fun — enjoy it.

## traps

- running uncapped at thousands of fps and thinking timing is broken — that's just missing vsync.
- per-pixel `SDL_RenderDrawPoint`. streaming texture only.
- emulation logic creeping into the frontend. it maps indices and pumps events, nothing else.
