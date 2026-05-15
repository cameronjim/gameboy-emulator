# milestone 02 — the sm83 cpu

**branch:** `feat/02-cpu` (stacked prs allowed within this doc) · **plan/review:** fable · **implement:** opus (alu/interrupts), sonnet (generated decode table)
**depends on:** 01 · **effort:** 3–4 weeks. the biggest milestone.

## goal

a complete sm83 interpreter: all 512 opcodes, flags exact, per-instruction cycle counts exact (both counts for conditionals), interrupts, halt including the halt bug. "done" is ultimately milestone 05; this pr lands the machinery plus its unit suite.

## hardware notes (binding)

- the sm83 is z80-*like*, not a z80. no ix/iy, no shadow registers, different flags in exactly the places that hurt. use the gbdev opcode table + pan docs only; never z80 docs.
- registers `a f b c d e h l`, pairs `af bc de hl`, plus `sp pc`. f low nibble always zero — mask on every write including `pop af`.
- flags z(7) n(6) h(5) c(4). adc/sbc half-carry includes carry-in. `add sp,e8` and `ld hl,sp+e8`: h/c from low-byte 8-bit add, z=0, n=0.
- `daa` from the n/h/c flag algorithm, not x86 nibble inspection.
- `rlca/rla/rrca/rra` always set z=0; their cb twins set z from result.
- conditional `jr/jp/call/ret` have two cycle counts; both must be right.
- interrupts: ime; ie 0xFFFF, if 0xFF0F. vectors vblank 0x40, stat 0x48, timer 0x50, serial 0x58, joypad 0x60, priority in that order. dispatch: 5 m-cycles, clears ime and the if bit, pushes pc. `ei` takes effect one instruction late; `ei` then `di` fires nothing.
- `halt`: ime=1 sleeps then services. ime=0, none pending: sleeps, resumes without servicing. ime=0, one already pending: **halt bug** — pc fails to increment, next byte executes twice.
- `stop`: 2-byte nop stub.
- no boot rom: init `a=0x01 f=0xB0 b=0x00 c=0x13 d=0x00 e=0xD8 h=0x01 l=0x4D sp=0xFFFE pc=0x0100` + pan docs io defaults.

## spec

- `core/cpu.{hpp,cpp}`: `uint32_t step()` — services one pending interrupt or executes one instruction, returns t-cycles. reads/writes only through a bus reference.
- decode: giant switch generated into `core/opcodes.inc` by `tools/gen_opcodes.py` from the gbdev opcode json (commit the json). generated cases call hand-written helpers: `alu_add`, `alu_adc`, ..., `do_daa`, `push16`, `pop16`, rotate/shift helpers. helpers are pure where possible — that's what layer-1 unit tests bite on.
- unknown opcode (0xD3 etc.): debug log + clean stop status per conventions.md.
- t-cycles everywhere, `uint64_t` total owned by `Gameboy`.

## tests

layer 1 (pure helpers): every alu family with flag-edge tables; `daa_table_post_add` / `daa_table_post_sub`; `adc_half_carry_includes_carry_in`; `sbc_half_carry_includes_borrow_in`; `pop_af_masks_low_nibble`; `add_sp_e8_flags_from_low_byte`; `rlca_zeroes_z`; cb bit/res/set/swap.
layer 2 (cpu + fakebus byte scripts): `ei_delays_one_instruction`; `ei_di_fires_nothing`; `interrupt_dispatch_costs_20_tcycles_and_jumps_vector`; `interrupt_priority_order`; `halt_wakes_without_service_when_ime_clear`; `halt_bug_repeats_next_byte`; `jr_taken_vs_not_taken_cycles` (and jp/call/ret cc); `pc_wraps_at_ffff`.

## done when

full opcode coverage (a coverage test walks all 512 entries against the json for length + cycle counts), unit suite green, several hundred instructions of a blargg rom execute without hitting the unknown-opcode trap. blargg passing comes in 05.

## traps

- transcribing opcodes by hand. generate from json; the tests catch generator slips.
- one cycle count for conditionals — breaks timing tests weeks later in ways that look unrelated.
- signed operands: `jr`, `add sp,e8` — explicit `int8_t` cast at the read site.
- inventing timing. anything not in the json/pan docs: stop and flag, per CLAUDE.md rule 5.
