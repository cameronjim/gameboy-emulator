# milestone 04 — memory bus / mmu + serial stub

**branch:** `feat/04-bus` · **plan/review:** fable · **implement:** opus
**depends on:** 01, interleaves with 02 · **effort:** ~1 week net

## goal

all memory traffic routed through one mediator with the dmg memory map, echo ram, correct unmapped behavior, oam dma, and the serial stub that lets test roms print before a screen exists.

## spec

`core/bus.{hpp,cpp}`, `read8/write8` routing:

| range | region | behavior |
|---|---|---|
| 0000–7FFF | rom | `mapper.read_rom` / `mapper.write_rom` |
| 8000–9FFF | vram | array (ppu owns it later) |
| A000–BFFF | cart ram | mapper |
| C000–DFFF | wram | array |
| E000–FDFF | echo | `addr - 0x2000` into wram |
| FE00–FE9F | oam | array |
| FEA0–FEFF | unusable | read 0xFF, write ignored |
| FF00–FF7F | io | dispatch to components; unimplemented read 0xFF |
| FF80–FFFE | hram | array |
| FFFF | ie | register |

- **unmapped/unimplemented io reads return 0xFF, never 0x00.** tetris's joypad polling breaks on 0x00 — menus navigate themselves.
- `if` (0xFF0F) upper 3 bits read as 1. apply per-register unused-bit masks as registers appear.
- serial stub: write to sb (0xFF01) stores; write 0x81 to sc (0xFF02) emits the byte to a `std::function<void(uint8_t)>` sink the frontend/harness installs. stdout printing lives outside the core.
- oam dma: write to 0xFF46 copies 160 bytes from xx00–xx9F instantly (v1 decision).
- `read16/write16` little-endian helpers.

## tests

- `echo_ram_mirrors_wram_both_directions`
- `unusable_region_reads_ff_ignores_writes`
- `unmapped_io_reads_ff`
- `if_upper_bits_read_ones`
- `serial_sink_receives_bytes_on_sc_81`
- `oam_dma_copies_160_bytes`
- `read16_is_little_endian`

## done when

cpu runs against the real bus; a rom writing a string via sb/sc reaches the sink; all unit tests green.

## traps

- components poking each other's arrays directly instead of going through the bus — quirks must live in one place.
- returning 0x00 for unmapped io (see above; this is the #1 "tetris acts possessed" cause).
