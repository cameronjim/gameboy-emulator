# conventions — style, naming, comments, srp

read before writing any code. these are not suggestions.

## language and toolchain

- c++20. no exceptions thrown across the core api; no rtti reliance.
- cmake ≥ 3.24. clang or gcc; both must build clean.
- `-Wall -Wextra -Werror` everywhere. debug builds add `-fsanitize=address,undefined`.
- clang-format enforced in ci (config committed at repo root; llvm base, 4-space indent, 110 col).

## naming

| thing | style | example |
|---|---|---|
| files | snake_case | `cpu.cpp`, `mbc1.hpp` |
| types | PascalCase | `Cpu`, `Cartridge`, `PixelFifo` |
| functions/methods | snake_case | `read8`, `step_instruction` |
| variables | snake_case | `rom_bank`, `dot_counter` |
| constants | kPascalCase | `kVBlankVector`, `kOamStart` |
| enum class members | PascalCase | `Mode::OamScan` |
| test names | snake_case sentences | `daa_after_add_adjusts_high_nibble` |

hardware register constants take their pan docs names: `kRegLcdc = 0xFF40`, `kRegStat = 0xFF41`, `kRegDiv = 0xFF04`. never a bare magic address in logic code.

## comments — the house style

- very minimal. most functions need zero comments.
- all lowercase, always.
- single line `//` ONLY. never `/* */`. never let one thought span two comment lines — compress it or delete it.
- a few words to one very short sentence.
- a comment earns its place only by stating *why*, a hardware fact, or a pan docs citation. never what the code visibly does.

```cpp
f &= 0xF0;                      // f low nibble always reads zero
if (!ime && pending) pc--;      // halt bug: next byte executes twice
counter = 0;                    // pandocs: any div write clears whole counter
```

forbidden:

```cpp
/* This block handles the DAA instruction.
   It adjusts the accumulator after BCD arithmetic. */
// Increment the program counter by one
```

## types and values

- fixed-width ints for all hardware values: `uint8_t`, `uint16_t`. no bare `int`, no `char` arithmetic.
- signed offsets go through explicit `static_cast<int8_t>` at the read site.
- t-cycles are the one true time unit, carried as `uint64_t`. never mix m- and t-cycles in an interface; convert at the boundary with a named helper.
- `enum class` for all state machines (ppu mode, mbc type). no raw enums, no bool soup.
- no owning raw pointers. components live by value inside `Gameboy`; cross-references are non-owning refs wired in the constructor.

## srp and separation of concerns

- one hardware component = one class = one `.hpp/.cpp` pair. `Cpu`, `Bus`, `Timer`, `Ppu`, `Apu`, `Joypad`, `Serial`, `Cartridge` (+ one class per mbc).
- a class owns exactly its own registers and state. nothing reaches into another component's arrays — all cross-component traffic goes through `Bus::read8/write8` or an explicit interrupt-request line.
- the cpu executes instructions. it does not know what a scanline is.
- the ppu produces a 160×144 index buffer. it does not know what a pixel format, window, or texture is.
- the frontend maps indices to rgb, pumps events, paces time, plays samples. it contains zero emulation logic.
- functions do one thing. an opcode handler computes one instruction's effect. soft ceiling ~40 lines; the opcode switch and other generated tables are exempt.
- if a function needs "and" to describe it, split it.

## error handling

- the core never aborts on bad guest behavior; bad rom values get masked or ignored exactly as hardware masks them.
- unimplemented opcode in debug: log pc + opcode + register dump, then stop cleanly via a status return — not `exit()` inside the core.
- host-side errors (file missing, bad args) are the frontend's problem and are checked there.

## misc quality bars

- no globals, no singletons, no static mutable state in the core.
- deterministic core: same rom + same inputs ⇒ same instruction stream, always. no wall clock, no rng in `core/`.
- headers are self-sufficient (iwyu); include order: own header, core headers, std.
- prefer `std::array` over c arrays; `std::span` for buffer views across the api.
