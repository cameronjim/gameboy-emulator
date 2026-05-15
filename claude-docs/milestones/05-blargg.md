# milestone 05 — blargg cpu_instrs passes (first boss fight)

**branch:** `feat/05-blargg` · **plan/review:** fable · **debugging:** fable + opus
**depends on:** 02, 03, 04 · **effort:** 1–2 weeks of pure debugging

## goal

all 11 individual blargg cpu_instrs sub-tests print `Passed`. after this, every future bug is *not* a cpu bug — the strongest checkpoint in the project.

## spec

- finish `tools/fetch-test-roms.sh`: clone/download retrio/gb-test-roms, gekkio mooneye prebuilts, mattcurrie dmg-acid2 into `tests/roms/vendor/` (gitignored).
- build `tests/roms/harness.cpp` per testing.md: headless, serial channel first (fibonacci + framebuffer channels can be stubs until needed).
- `tests/roms/manifest.txt` gains the 11 singles: `01-special.gb` … `11-op a,(hl).gb`, channel=serial, expect=`Passed`, budget ≈ 2e9 t-cycles each.
- debugging loop per failure: run under `--doctor`, feed to gameboy doctor, fix the exact divergent opcode, add a layer-1 regression test for the fixed behavior, rerun. expect 15–40 individual fixes; this is normal.

## known gotchas (read before panicking)

- the combined `cpu_instrs.gb` is >32kb and **requires mbc1** — it will not boot until milestone 11. run the singles. do not "fix" the cpu because the combined rom hangs.
- `02-interrupts.gb` needs the timer; it stays red until milestone 06. mark it expected-fail in the manifest with a comment.
- doctor mode must pin ly=0x90 (milestone 03) or every log diverges immediately.

## tests

- manifest: 10 singles green now, `02-interrupts` expected-fail.
- every divergence fixed adds a named unit regression (`sbc_carry_in_half_carry_regression`, ...). zero fixes merge without one.

## done when

`tools/run-test-roms.sh` shows 10/10 available singles `Passed` (plus 02 flagged), ci runs the harness, and each fix has its regression test.

## traps

- staring at code instead of trace-diffing. the diff names the instruction — use it.
- brute-forcing constants until green. every fix must be explainable against pan docs.
