# milestone 09 — sprites, window layer, joypad → TETRIS PLAYABLE

**branch:** `feat/09-sprites-input` · **plan/review:** fable · **implement:** opus
**depends on:** 07, 08 · **effort:** 1–2 weeks

## goal

the checkpoint the whole ladder aims at: a full, playable game of tetris. pieces fall (sprites), stack (bg), move and rotate (joypad), and the sequence is random (div, already done).

## hardware notes (binding)

- **oam**: 40 entries at 0xFE00, 4 bytes: y (screen_y + 16), x (screen_x + 8), tile index, attributes (bit 7 bg-over-obj, 6 y-flip, 5 x-flip, 4 palette obp0/obp1). the +16/+8 offsets are the classic trap.
- per scanline: scan oam in order, take the **first 10** sprites whose y-range covers the line (hardware limit; games use it for flicker) — selection happens in mode 2 order, before any x consideration.
- pixel rules: sprite color 0 always transparent; obp0/obp1 palettes (0xFF48/49); bg-over-obj bit puts nonzero bg colors above the sprite. dmg x-priority (lower x wins overlaps) — implement if cheap, else defer to milestone 10.
- 8×16 mode (lcdc bit 2): tile index low bit ignored; top/bottom halves.
- **window**: lcdc bit 5 enable; wy 0xFF4A, wx 0xFF4B with a **−7 offset**; has its own internal line counter that only advances on lines where the window actually rendered. tetris barely uses it; dmg-acid2 does — get it structurally right now, pixel-perfect in 10.
- **joypad** (0xFF00): bits 4/5 select action/direction groups, active-low; pressed = 0 in bits 0–3; unselected group and unpressed bits read 1. tetris polls; the joypad interrupt can wait.

## spec

- ppu scanline renderer gains sprite pass (after bg+window, respecting priority rules) and window pass.
- `core/joypad.{hpp,cpp}`: 8-bit pressed state from `set_button`; composes 0xFF00 on read from select bits.
- frontend key map: arrows = dpad, z = a, x = b, enter = start, rshift = select.

## tests

- `oam_offsets_y16_x8`
- `sprite_limit_first_ten_in_oam_order`
- `sprite_color0_transparent`
- `bg_over_obj_priority_bit`
- `x_flip_and_y_flip`
- `tile_index_low_bit_ignored_in_8x16`
- `obp0_obp1_palettes_apply`
- `window_wx_minus_7_offset`
- `window_internal_line_counter_pauses`
- `joypad_active_low_and_group_select`
- `unselected_joypad_bits_read_one`

## done when

**you play a complete game of tetris.** pieces respond, rotation works, lines clear, game-overs happen, two consecutive games deal different pieces. record a short clip for the pr.

## traps

- forgetting color-0 transparency: boxy sprites on solid backgrounds.
- inverted joypad polarity: menus scroll by themselves (unpressed reading as pressed).
- y/x offsets applied twice or not at all — sprites hovering 16px off.
