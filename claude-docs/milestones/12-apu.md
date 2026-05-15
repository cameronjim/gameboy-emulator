# milestone 12 — apu → TETRIS WITH SOUND

**branch:** `feat/12-apu` (stacked prs: channels → mixer → frontend audio) · **plan/review:** fable · **implement:** opus
**depends on:** 06 · **effort:** 2–4 weeks

## goal

four channels, recognizable korobeiniki, no crackle, correct game speed. judged by ear against real recordings — **not** by blargg dmg_sound completion, which is explicitly a non-goal.

## hardware notes (binding)

- channels: ch1 square + frequency sweep, ch2 square, ch3 wave (32 4-bit samples at 0xFF30–3F), ch4 noise (lfsr).
- **frame sequencer**: 512 hz clock derived from div bit 4 falling edges (div-apu). steps: length counters 256 hz, ch1 sweep 128 hz, envelopes 64 hz.
- per channel: frequency timer, duty position (squares, 4 duty patterns), volume envelope, length counter, trigger behavior on nrx4 bit 7 (reload length if zero, reset envelope/timer, sweep shadow copy for ch1).
- mixer: nr50 master volume, nr51 panning, nr52 power (clears regs when off) + read-only channel status bits.
- implementation order: **ch2 → ch1 → ch4 → ch3** (tetris uses noise for line clears).

## spec

- `core/apu.{hpp,cpp}` (+ per-channel files if it grows): `tick(tcycles)`; sample the mixer every ~87 t-cycles (4194304 / 48000) into an internal ring; `Gameboy::read_audio(std::span<int16_t>)` drains stereo interleaved.
- frontend: sdl audio queue at 48 khz. **audio drives pacing**: run emulation to keep queued audio in a target band (~50–100 ms) instead of vsync — one mechanism solves drift, crackle, and speed. vsync stays on for presentation only.

## tests

- `frame_sequencer_rates_256_128_64`
- `square_duty_patterns_exact` (all four, one period sampled)
- `envelope_steps_at_64hz_and_clamps`
- `sweep_updates_and_overflow_disables_ch1`
- `length_counter_disables_channel`
- `trigger_reloads_length_and_envelope`
- `ch4_lfsr_sequence_15bit_and_7bit`
- `ch3_reads_wave_ram_nibbles_in_order`
- `nr52_power_off_clears_registers`
- `mixer_panning_nr51`
- golden test: render 2s of a known nr-register script to samples, compare hash — catches regressions without ears.

## done when

korobeiniki recognizable and clean for 60s; line-clear noise fires; game speed correct by the audio-band pacing; unit + golden tests green.

## traps

- pushing exactly one frame of samples against vsync = underruns = crackle. the target-band approach exists to prevent this; don't fight it.
- chasing blargg dmg_sound to 100%. ear + golden tests are the v1 bar.
- resampling cleverness. integer-ish decimation at ~87 cycles/sample is fine.
