# testing — the most important doc in this folder

nothing merges without green tests. every milestone doc lists the tests it must add; this doc defines the layers, the harnesses, and the rules.

## framework and layout

- **catch2 v3** via cmake fetchcontent. `tests/unit/`, one file per component: `cpu_alu_test.cpp`, `cpu_decode_test.cpp`, `timer_test.cpp`, `ppu_bg_test.cpp`, `mapper_mbc1_test.cpp`, ...
- run with `ctest --test-dir build`. unit suite must stay under ~5s so it runs on every change.
- test names are lowercase snake_case sentences stating the behavior: `pop_af_masks_low_nibble`, `writing_div_resets_internal_counter`, `sprite_limit_is_ten_per_scanline`.

## the four layers

### 1. unit tests — pure logic, no wiring

target: alu + flag math, decode helpers, header parsing, mapper bank arithmetic, palette application, lfsr steps. these functions must be written as pure functions precisely so they are testable this way.

minimum coverage expected:
- every alu op family: add/adc/sub/sbc/and/or/xor/cp/inc/dec, 16-bit adds, `add sp,e8` (h/c from low byte, z=0), daa (drive with a table of known input→output pairs, both post-add and post-sub), rlca/rla/rrca/rrca z=0 rule, cb rotates/shifts/bit/res/set/swap.
- flag edge cases get their own asserts: half-carry with carry-in on adc/sbc; pop af masking.
- mapper math: bank-0→1 remap, bank masking to rom size, ram-enable gating.

### 2. logic/component tests — one component + fakes

target: a real component wired to a `FakeBus` / scripted memory, no other components.

- **cpu**: execute a short hand-assembled byte sequence from a fake bus; assert registers, flags, cycles consumed. cover: ei delay (interrupt not taken on the instruction after ei), halt wake with ime=0, halt bug byte-repeat, interrupt dispatch cost and vector, taken vs not-taken cycle counts for jr/jp/call/ret cc.
- **timer**: tick n cycles, assert div/tima; overflow reloads from tma and raises if bit 2; div write resets counter.
- **ppu**: tick through a scanline, assert mode sequence 2→3→0 and dot budgets; ly increments; vblank if bit raised at line 144; lyc coincidence flag. render one scanline into the index buffer from hand-built vram: signed and unsigned tile addressing, scx/scy wrap, bitplane merge order (bit 7 = leftmost).
- **joypad**: select lines active-low; unpressed reads 1.
- **bus**: echo ram mirror both directions, unmapped io reads 0xFF, oam dma copies 160 bytes.

### 3. integration — the test rom harness

`tools/fetch-test-roms.sh` downloads blargg (retrio/gb-test-roms), mooneye (Gekkio/mooneye-test-suite), dmg-acid2 (mattcurrie) into `tests/roms/vendor/` (gitignored). roms are homebrew, free and legal; commercial roms are never committed.

`tests/roms/harness.cpp` builds a headless runner with three result channels:
- **serial** (blargg): capture sb/sc output, run up to a cycle budget, assert the string contains `Passed`.
- **fibonacci** (mooneye): run until opcode `0x40` (`ld b,b`) marker, assert b,c,d,e,h,l = 3,5,8,13,21,34.
- **framebuffer** (acid2): run n frames, hash the index buffer, compare against a committed known-good hash; on mismatch dump a ppm artifact for eyeballing.

`tools/run-test-roms.sh` runs the manifest in `tests/roms/manifest.txt` (rom path → channel → expectation). each milestone appends lines to the manifest; the script is the merge gate from milestone 05 on.

### 4. regression — trace snippets

when a divergence bug is fixed, keep a tiny regression: the offending instruction sequence as bytes in a unit test asserting the corrected register outcome. never re-fix the same bug twice.

## ci

github actions on every pr: configure, build (debug with asan/ubsan + release), ctest, run-test-roms.sh for all manifest entries so far, clang-format check. red ci blocks merge — no exceptions, including "it passes locally".

## rules

- a bug fixed without a test that would have caught it is not fixed.
- never weaken an assertion to go green; if the expectation was wrong, cite pan docs in the pr.
- deterministic tests only: fixed cycle budgets, no sleeps, no wall clock.
- the harness must not know component internals — it drives `Gameboy` exactly like a frontend.
