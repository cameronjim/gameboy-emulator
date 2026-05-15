# milestone 06 — timer (div/tima/tma/tac)

**branch:** `feat/06-timer` · **plan/review:** fable · **implement:** opus
**depends on:** 04, 05 · **effort:** 3–5 evenings

## goal

the system timer, correct enough for blargg `02-interrupts` + `instr_timing` and for tetris piece randomness (tetris seeds from div — without div every game deals the same pieces).

## spec

- mental model (binding): a free-running internal 16-bit counter incremented every t-cycle.
  - div (0xFF04) = counter's upper 8 bits (ticks at 16384 hz). **any write to div resets the entire internal counter to 0.**
  - tima (0xFF05) increments on a falling edge (1→0) of the tac-selected counter bit. tac (0xFF07): bit 2 enable; bits 0–1 select bit 9 / 3 / 5 / 7 (4096 / 262144 / 65536 / 16384 hz).
  - tima overflow: reload from tma (0xFF06), request timer interrupt (if bit 2). v1: reload on the overflow tick is acceptable; the one-m-cycle 0x00 window is a documented deferral.
- implement the falling-edge model (it's barely more code and gets div-write-causes-tima-tick free). do **not** chase mooneye's full timer suite to 100% — several of those need sub-instruction timing. out of scope, noted in overview non-goals.
- `Timer::tick(uint32_t tcycles)` called from the gameboy loop every instruction — never once per frame.

## tests

- `div_increments_at_16384hz`
- `div_write_resets_internal_counter`
- `div_write_can_tick_tima_via_falling_edge`
- `tima_rates_match_tac_select` (all four)
- `tima_overflow_reloads_tma_and_requests_interrupt`
- `disabled_tac_stops_tima_not_div`
- manifest: `02-interrupts.gb` flips to expect `Passed`; add `instr_timing.gb` expect `Passed`.

## done when

both roms pass; unit tests green; interrupt latency sane (per-instruction ticking verified by a test that a pending timer interrupt is serviced within one instruction of ime set).

## traps

- ticking the timer per frame instead of per instruction.
- wiring tac select to the wrong bits (the frequency order is not monotonic).
