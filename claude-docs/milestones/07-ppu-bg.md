# milestone 07 — ppu: background + mode machine (second boss fight)

**branch:** `feat/07-ppu-bg` (stacked prs allowed) · **plan/review:** fable · **implement:** opus
**depends on:** 05, 06 · **effort:** 2–3 weeks

## goal

a scanline-based ppu: dot-accurate mode/interrupt timing, all-at-once scanline rendering of the background layer. **no pixel fifo** — the fifo is the #1 first-emulator killer and is explicitly a non-goal (see overview).

## hardware notes (binding)

- 456 dots/scanline, 154 lines (0–143 visible, 144–153 vblank), 70224 dots/frame.
- visible line: mode 2 oam scan (80 dots) → mode 3 drawing (fixed 172 for v1) → mode 0 hblank (remainder). lines 144–153: mode 1.
- registers: lcdc 0xFF40, stat 0xFF41 (mode bits 0–1, lyc=ly bit 2, enable bits 3–6), ly 0xFF44 (read-only), lyc 0xFF45, scy/scx 0xFF42/43, bgp 0xFF47.
- vblank interrupt (if bit 0) at the start of line 144. stat interrupt (if bit 1) when an enabled source becomes true — v1 fires per-condition; full rising-edge "stat blocking" deferred.
- background: pixel color for (x, ly) = tilemap cell from ((scx+x) & 255, (scy+ly) & 255); map at 0x9800 or 0x9C00 per lcdc bit 3; tile row = two bitplane bytes; **lcdc bit 4 = 0 means signed addressing: `0x9000 + int8_t(index) * 16`**; color = ((hi>>(7-px))&1)<<1 | ((lo>>(7-px))&1), through bgp.
- lcd off (lcdc bit 7 = 0): ly=0, mode 0, no ppu interrupts; rendering resumes clean on re-enable.

## spec

- `core/ppu.{hpp,cpp}`: `tick(uint32_t tcycles)` advances the dot counter, drives mode transitions, ly, lyc compare, interrupt requests; at each mode-3 entry renders that full scanline into the 160×144 index buffer.
- ppu owns vram + oam arrays; bus routes 8000–9FFF / FE00–FE9F to it. v1: no mode-based access blocking (documented deferral).
- debug hook: `dump_framebuffer_ppm(path)` in the frontend, and a core accessor for tile data (the tile viewer in milestone 08 uses it).

## tests

layer 2, hand-built vram:
- `mode_sequence_and_dot_budgets_per_line`
- `ly_increments_and_wraps_at_154`
- `vblank_interrupt_at_line_144`
- `lyc_coincidence_sets_stat_bit_and_interrupts_when_enabled`
- `bg_scanline_unsigned_addressing`
- `bg_scanline_signed_addressing` (the top-3 all-time bug — test both modes explicitly)
- `scx_scy_wraparound`
- `bitplane_merge_bit7_is_leftmost`
- `bgp_palette_applies`
- `lcd_off_resets_ly_and_mode`

## done when

headless run of tetris for ~600 frames, dump ppm: recognizable copyright/title screen (it's nearly all background tiles). unit tests green. this works *before* milestone 08 gives you a window — do it, it's the best smoke test.

## traps

- signed tile addressing (again: test both).
- ly stalling or vblank not firing — tetris sits in a halt-until-vblank loop, so a "frozen" emulator means check ly/if first, not the cpu.
- building the fifo because a forum said scanline is "wrong". scanline passes acid2. hold the line.
