# milestone 13 — save states + quality of life

**branch:** `feat/13-savestates` · **plan/review:** fable · **implement:** sonnet (serialization), opus (state audit)
**depends on:** 12 · **effort:** 3–5 evenings

## goal

full-machine snapshots — also a debugging superpower (snapshot before a bug, iterate in seconds). plus fast-forward and pause.

## spec

- versioned binary blob: magic `GBST`, format version u32, then per-component sections with length prefixes. include **all** state: cpu regs + ime + halt latch, full ram/vram/oam/hram, io registers, mapper state (banks, ram-enable, rtc), timer internal 16-bit counter, ppu dot counter + mode + window line counter, apu ring/positions/frame-sequencer step, pending oam-dma progress.
- `Gameboy::save_state(std::vector<uint8_t>&)` / `bool load_state(std::span<const uint8_t>)`.
- **load is parsing hostile input** — security.md rules: validate magic/version/lengths, re-mask every value to legal range, deserialize into a temp machine and swap only on success. a bad file leaves the running game untouched.
- frontend: f5 save, f8 load, single slot file `<rom>.state`; hold tab = fast-forward (run 4 frames per host frame, mute), p = pause.
- fuzz target `fuzz_savestate` per security.md.

## tests

- `roundtrip_restores_exact_state` (run n frames, save, run m, load, run m again — framebuffer hashes identical)
- `truncated_state_rejected_machine_untouched`
- `bad_magic_and_version_rejected`
- `out_of_range_values_rejected_or_masked`
- `nonarchitectural_state_survives` (timer counter, ppu dot, window line counter each explicitly asserted — the classic desync bugs)

## done when

save/load mid-game is seamless in tetris and one mbc1 game; roundtrip test green; fuzzer runs clean.

## traps

- forgetting non-architectural state (timer counter, dot counter, dma progress) — loads *look* fine, then desync subtly. the dedicated test exists for this.
- serializing pointers or sizes without validation on the way back in.
