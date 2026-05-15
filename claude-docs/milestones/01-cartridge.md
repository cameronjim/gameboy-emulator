# milestone 01 — cartridge loading + header parsing

**branch:** `feat/01-cartridge` · **plan/review:** fable · **implement:** sonnet
**depends on:** 00 · **effort:** 2–3 evenings

## goal

load a `.gb` file, parse and validate its header, serve reads for rom-only carts. quick win; also the first untrusted-input surface, so security.md applies in full.

## scope

**in:** header parse, validation, rom-only mapper, mapper interface, logging the parsed header.
**out:** mbc1/mbc3 (milestone 11), cart ram, any write behavior beyond ignoring.

## spec

- `core/mapper.hpp`: interface `uint8_t read_rom(uint16_t)`, `void write_rom(uint16_t, uint8_t)`, `uint8_t read_ram(uint16_t)`, `void write_ram(uint16_t, uint8_t)`.
- `core/mapper_rom.{hpp,cpp}`: rom-only. reads index into the rom masked to its size; writes ignored; ram reads 0xFF.
- `core/cartridge.{hpp,cpp}`: `static std::optional<Cartridge> parse(std::span<const uint8_t>)`. header fields:
  - title: 0x0134–0x0143, trimmed at first null.
  - type 0x0147: accept 0x00 (rom-only) now; recognize-and-reject 0x01–0x03, 0x0F–0x13 with a clear "unsupported mapper" reason; unknown values rejected.
  - rom size 0x0148: declared size must equal file size; mismatch rejects.
  - ram size 0x0149, header checksum 0x014D verified (sum over 0x0134–0x014C per pan docs).
- reject = `std::nullopt` + reason retrievable; never throw, never crash (fuzz target lands here).
- frontend prints: `title=TETRIS type=rom_only rom=32KB ram=none checksum=ok`.
- fuzz target `fuzz_header` per security.md.

## tests

unit, table-driven with synthetic headers:
- `valid_rom_only_header_parses`
- `title_trims_trailing_nulls`
- `declared_size_mismatch_rejects`
- `bad_checksum_rejects`
- `mbc_types_rejected_as_unsupported`
- `truncated_file_rejects` (files of size 0, 0x100, 0x14F)
- `rom_read_masks_to_size`

## done when

`./gbemu-sdl tetris.gb` prints the header line for a real 32kb rom; all unit tests green; fuzz_header runs 1 min without a crash.

## traps

- implementing mbc1 now "since you're in there". don't — you'll write it better after the bus exists.
- allocating from the declared size instead of the actual file size.
