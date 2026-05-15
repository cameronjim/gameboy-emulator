# milestone 11 — mbc1, mbc3, battery saves

**branch:** `feat/11-mbc` · **plan/review:** fable · **implement:** opus (banking math), sonnet (save files)
**depends on:** 04 · **effort:** 1–2 weeks

## goal

open the library: super mario land, link's awakening, kirby (mbc1); pokémon red/blue (mbc3 + rtc). plus battery saves — trivial and high-value.

## hardware notes (binding)

- writes into rom space are mapper commands.
- **mbc1**: 0000–1FFF ram enable (low nibble 0x0A = on); 2000–3FFF rom bank low 5 bits — **writing 0 selects 1** (missing this breaks many games); 4000–5FFF upper 2 bits / ram bank; 6000–7FFF banking mode (mode 1 affects 0000–3FFF and ram banking on large carts). mask every bank number to the real bank count. mbc1m multicarts: out of scope.
- **mbc3**: 2000–3FFF rom bank full 7 bits (0→1 still applies); 4000–5FFF selects ram bank 0–3 or rtc register 0x08–0x0C; 6000–7FFF latch on 0x00→0x01 write. rtc v1: derive from a host-time value the *frontend* injects once at load (core stays clock-free); tick-accurate rtc deferred.
- **battery**: types with battery persist external ram. frontend writes `<rom>.sav` on exit + every 30s, loads at start. raw ram bytes, no header.

## spec

- `core/mapper_mbc1.{hpp,cpp}`, `core/mapper_mbc3.{hpp,cpp}` behind the milestone-01 interface; cartridge factory accepts types 0x01–0x03, 0x0F–0x13 and reports `has_battery`.
- ram reads while disabled return 0xFF; writes ignored.
- core exposes `external_ram()` span for the frontend save/load path.

## tests

- `mbc1_bank0_write_selects_bank1`
- `mbc1_bank_masked_to_rom_size`
- `mbc1_ram_enable_gates_reads_and_writes`
- `mbc1_mode1_remaps_lower_region`
- `mbc3_seven_bit_rom_bank`
- `mbc3_ram_vs_rtc_select`
- `mbc3_latch_on_00_01`
- manifest: combined `cpu_instrs.gb` now boots and passes; mooneye `emulator-only/mbc1/*` and `mbc3/*` (fibonacci channel — finish that harness path here).

## done when

combined cpu_instrs passes; mooneye mbc sets green; a battery game keeps its save across restart; at least one mbc1 commercial game reaches gameplay.

## traps

- unmasked bank numbers → out-of-bounds reads that look like random corruption (also a security.md violation).
- forgetting ram-enable gating.
- letting the core read the host clock for rtc. the frontend injects; the core stays deterministic.
