# milestone 14 — wasm frontend

**branch:** `feat/14-wasm` · **plan/review:** fable · **implement:** sonnet
**depends on:** 08 (12 for audio) · **effort:** 1–2 weekends

## goal

a static web page anyone can open that plays a homebrew game in-browser. the payoff for milestone 00's discipline: `gbcore` compiles unchanged.

## spec

- emscripten toolchain build: `emcmake cmake -B build-wasm`; `gbcore` must need **zero changes** — if it does, the fix is removing the platform leak from core, never an `#ifdef`.
- path a (do this first): recompile the sdl frontend with emscripten's sdl2 port; swap the `while` loop for `emscripten_set_main_loop` (browsers forbid blocking loops).
- path b (optional, nicer): hand-written js shell — exported `run_frame`/framebuffer-pointer/`set_button` via `EMSCRIPTEN_KEEPALIVE`, canvas `putImageData`, audioworklet fed from `read_audio`.
- rom loading: `<input type="file">` → heap copy → `load_rom`. audio starts only after a user gesture (browser rule) — gate it on the first click/keydown.
- **ship a homebrew rom as the embedded demo. never host tetris or any commercial rom.**
- ci job builds the wasm target so it can't rot.

## tests

- ci: emscripten build green.
- `run_frame` determinism already covered by unit suite (same core).
- manual gate: page loads from a static file server, homebrew demo playable, file-input rom works, audio starts after gesture.

## done when

a github-pages-able folder: `index.html` + `.js` + `.wasm`, playable in chrome and firefox.

## traps

- blocking main loop — instant browser hang; `emscripten_set_main_loop` from the start.
- assuming synchronous file i/o exists.
- expecting audio before a user gesture.
- "fixing" core for wasm with ifdefs instead of removing the leak.
