# game boy emulator — project overview

a game boy (dmg) emulator written from scratch in c++20. platform-agnostic core, sdl2 desktop frontend first, wasm/browser frontend later. the concrete success target is **tetris: playable, correct piece randomness, with sound** — and the milestone order is built so tetris arrives at the halfway point, not the end.

this file is the map. fine detail lives in `claude-docs/`. the working method: feed claude code **one milestone doc at a time**, review the pr, run the tests, merge to main, move to the next.

## why this order works

three facts shape everything:

1. **tetris is a 32kb rom-only cartridge.** no memory bank controller, no cart ram, no battery. it needs a correct cpu, a bus, vblank, background rendering, sprites, joypad, and the div register. it does not need mbc1, the window layer, or the apu. so mbc and audio come *after* the tetris checkpoint.
2. **debugging dominates writing.** an emulator bug's symptom (garbled screen) sits hundreds of thousands of instructions away from its cause (one wrong flag). the trace logger (milestone 03) is built mid-cpu, not after, because trace-diffing against known-good logs is the single highest-leverage technique in this domain.
3. **test roms are the arbiter.** free, legal homebrew programs that run on the emulator and report pass/fail against verified real-hardware behavior. every milestone gates on them or on unit tests. nothing merges on vibes.

## milestone ladder

each milestone is one branch, one pr, one spec doc. estimates assume 8–12 h/week.

| # | milestone | spec | effort | cumulative |
|---|---|---|---|---|
| 00 | skeleton, cmake, core/frontend split | `claude-docs/milestones/00-skeleton.md` | 1 weekend | wk 1 |
| 01 | cartridge loading + header parsing | `claude-docs/milestones/01-cartridge.md` | 2–3 evenings | wk 1–2 |
| 02 | sm83 cpu | `claude-docs/milestones/02-cpu.md` | 3–4 weeks | wk 5–6 |
| 03 | trace logger (gameboy doctor format) | `claude-docs/milestones/03-trace-logger.md` | 1–2 evenings | — |
| 04 | memory bus / mmu + serial stub | `claude-docs/milestones/04-bus.md` | ~1 wk, interleaved | — |
| 05 | blargg cpu_instrs all pass | `claude-docs/milestones/05-blargg.md` | 1–2 weeks | wk 6–8 |
| 06 | timer (div/tima/tma/tac) | `claude-docs/milestones/06-timer.md` | 3–5 evenings | wk 7–9 |
| 07 | ppu: background + mode machine | `claude-docs/milestones/07-ppu-bg.md` | 2–3 weeks | wk 9–12 |
| 08 | sdl2 frontend, first picture | `claude-docs/milestones/08-sdl-frontend.md` | 1 weekend | wk 10–12 |
| 09 | sprites, window, joypad → **tetris playable** | `claude-docs/milestones/09-sprites-input.md` | 1–2 weeks | **wk 11–14** |
| 10 | dmg-acid2 pixel-perfect | `claude-docs/milestones/10-acid2.md` | 1 week | wk 12–15 |
| 11 | mbc1, mbc3, battery saves | `claude-docs/milestones/11-mbc.md` | 1–2 weeks | wk 14–17 |
| 12 | apu → **tetris with sound** | `claude-docs/milestones/12-apu.md` | 2–4 weeks | **wk 16–21** |
| 13 | save states + qol | `claude-docs/milestones/13-savestates.md` | 3–5 evenings | wk 17–22 |
| 14 | wasm frontend | `claude-docs/milestones/14-wasm.md` | 1–2 weekends | wk 18–23 |

the two boss fights are 05 (pure debugging against blargg) and 07 (the ppu). the historical failure mode is underestimating exactly those two and quitting there. budget them honestly and the rest follows.

## non-goals for v1

deliberately out of scope — each is a v2 project on top of a working emulator, not a v1 requirement: cycle-accurate ppu pixel fifo (scanline rendering passes acid2 and runs the library), game boy color, link cable beyond the serial stub, a perfect apu (judge by ear, not blargg dmg_sound completion), sub-instruction memory timing, jit or any performance work (a plain interpreter runs this machine at hundreds of fps).

accuracy is a dial, not a binary. v1 sets it at "passes blargg + acid2, plays the library." every notch past that costs ten times the previous notch.

## repository shape (after milestone 00)

```
core/           the emulator. no platform code, no i/o, no sdl.
frontend/sdl/   desktop shell: window, texture, audio, input, main loop.
frontend/wasm/  browser shell (milestone 14).
tests/unit/     catch2 unit + logic tests.
tests/roms/     test rom harness + expected outputs (roms fetched, not committed).
tools/          run-test-roms.sh, trace-diff helpers.
claude-docs/    the specs. read before coding.
```

## key references

- **pan docs** (gbdev.io/pandocs) — the hardware spec. when anything disagrees with pan docs, pan docs wins.
- **gbdev opcode table** (gbdev.io/gb-opcodes/optables) — all 512 opcodes, machine-readable json; generate the decoder from it.
- **game boy: complete technical reference** (gekkio) — hardware-verified depth; the tiebreaker for timing questions.
- **gameboy doctor** (github.com/robert/gameboy-doctor) — trace-diff tool with reference logs per blargg sub-test.
- **the ultimate game boy talk** (michael steil, 33c3) — watch before milestone 02; rewatch the ppu section before 07.
- **gbedg** (hacktix) — first-timer ppu/timer walkthrough.

## the working loop

1. open the next `claude-docs/milestones/NN-*.md`.
2. plan with **fable**; implement with **opus** (hard) or **sonnet** (mechanical) — routing rules in `CLAUDE.md`.
3. branch `feat/NN-name`, implement exactly the doc's scope.
4. run unit tests + the milestone's rom gate. green or it doesn't merge.
5. pr review (fable), merge to main, next doc.
