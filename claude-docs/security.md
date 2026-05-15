# security — untrusted input, memory safety, fuzzing

an emulator is a program that executes untrusted input. every rom byte is attacker-controlled. the threat model is: a malicious or corrupt `.gb` file, and later a malicious save-state file, must not be able to crash, corrupt, or escape the emulator.

## rom input hardening

- treat all header fields as hostile. rom size byte, ram size byte, cartridge type: validate against the actual file size before allocating or indexing. a header claiming 8mb rom on a 12kb file is rejected at `load_rom`, not discovered by a segfault later.
- every rom-derived index is masked before use: bank numbers masked to real bank count, tile indices masked to vram bounds, oam-dma source pages clamped. masking mirrors what hardware does, so correctness and safety are the same code.
- no allocation sized from an unvalidated header field.

## memory safety

- all guest memory accesses go through `Bus::read8/write8`, which cannot read or write outside emulator-owned buffers by construction (fixed `std::array` regions + masked indices). no pointer arithmetic from guest values.
- `std::span` for every buffer crossing an interface; `.at()` or explicit masks in cold paths, masks in hot paths.
- asan + ubsan on in debug and ci. a sanitizer report is a release blocker.
- no ub as an optimization: no unions for register pairs (use shifts/masks), no type-punning, signed conversions explicit.

## fuzzing

- libfuzzer targets, run in ci for a short budget and locally for longer:
  - `fuzz_header`: bytes → `Cartridge::parse`. must never crash.
  - `fuzz_exec`: bytes → load as rom-only, run 100k cycles headless. must never crash or hang (cycle budget enforced).
  - milestone 13 adds `fuzz_savestate`: bytes → `load_state`. must never crash and must reject cleanly.
- crashes found become regression tests.

## save states (milestone 13)

deserialization is parsing, with the same rules: magic + version checked first; every length field validated against remaining bytes; every loaded value re-masked to its legal range (ppu mode ∈ 0–3, bank ∈ real banks, pc anywhere but sp/counters masked). a truncated or tampered state file returns failure and leaves the running machine untouched — load into a temp, swap on success.

## frontend and wasm

- frontend validates host-side inputs (paths, file sizes) before handing bytes to the core.
- wasm build: the core's no-file-io, no-clock, no-network property means the sandbox surface is just "bytes in, pixels out". keep it that way — no emscripten fs, no fetch from the core.
- never ship or commit commercial roms; demo builds embed homebrew only. (legal hygiene, but it lives here so it gates prs.)

## secrets and supply chain

- no secrets exist in this project; ci uses no tokens beyond the default. dependencies are catch2 and sdl2 only, pinned by version in cmake. any new dependency needs a pr justification.
