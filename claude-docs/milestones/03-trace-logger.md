# milestone 03 — trace logger (gameboy doctor format)

**branch:** `feat/03-trace` · **plan/review:** fable · **implement:** sonnet
**depends on:** 02 (can land mid-02) · **effort:** 1–2 evenings

## goal

one log line per instruction, byte-compatible with gameboy doctor's reference logs, so milestone 05 debugging is trace-diffing instead of guessing. highest roi tooling in the project.

## spec

- `core/trace.{hpp,cpp}`: when enabled, before each instruction emit exactly:

```
A:01 F:B0 B:00 C:13 D:00 E:D8 H:01 L:4D SP:FFFE PC:0100 PCMEM:00,C3,13,02
```

uppercase hex, two digits, fields space-separated, pcmem = 4 bytes at pc read through the bus.
- doctor mode flag: reads of ly (0xFF44) return 0x90 (reference logs assume no ppu).
- frontend: `--doctor <path>` enables both; buffered writes (this is hot).
- `--trace-from N` starts logging at instruction n — logs get huge.

## tests

- `trace_line_matches_doctor_format_exactly` (golden string compare, initial state)
- `doctor_mode_pins_ly_to_0x90`
- `trace_from_skips_n_instructions`

## done when

`./gbemu-sdl --doctor out.log 01-special.gb` produces a log the doctor tool parses (even if it diverges early). manually verify doctor accepts it before merging.

## traps

- format drift: a missing space or lowercase hex makes doctor reject everything. golden-string test is mandatory.
- logging after execution instead of before.
