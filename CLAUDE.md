# CLAUDE.md

rules for working in this repo. specifics live in `claude-docs/` — read the relevant doc before writing code. one milestone doc = one PR.

## what this project is

a game boy (dmg) emulator in c++20. platform-agnostic core library (`core/`), sdl2 frontend (`frontend/sdl/`), wasm frontend later. success target: tetris, playable, with sound. full roadmap in `OVERVIEW.md`.

## model routing

- **fable**: thinking tasks — planning a milestone, designing an interface, reviewing a PR, debugging a divergence, anything requiring hardware-behavior reasoning.
- **opus**: hard agentic/integration tasks — implementing the cpu or ppu, wiring subsystems together, multi-file refactors.
- **sonnet**: routine agentic tasks — mechanical codegen (opcode tables), boilerplate, test scaffolding, small fixes, formatting.

when in doubt about difficulty, start with sonnet; escalate to opus if it struggles; never let either invent hardware timing — that is fable's call, grounded in pan docs.

## hard rules

1. **comments**: very minimal. all lowercase. single line `//` ONLY — never `/* */`, never a comment spanning two lines. a few words to one very short sentence. comments state *why* or a hardware fact, never what the code obviously does.
   - good: `// f low nibble always reads zero`
   - good: `// pandocs: writing div resets the whole counter`
   - bad: `// This function adds two numbers and sets the flags accordingly.`
2. **srp / separation of concerns**: one hardware component per class, one class per file pair. cpu knows nothing about sdl. ppu knows nothing about files. frontend owns all os i/o. everything crosses through the bus.
3. **core purity**: `core/` includes no sdl, no platform headers, no file i/o, no clocks, no rng. it is given bytes and returns bytes.
4. **tests are the arbiter**: no PR merges without its milestone's tests green. ai-written code is untrusted until a test rom or unit test passes over it. never tweak timing constants just to make a test pass — a timing change requires a pan docs citation in the PR description.
5. **no hardware guessing**: if behavior isn't in pan docs or gekkio's technical reference, stop and flag it. do not synthesize plausible timing.
6. **fixed-width ints everywhere**: `uint8_t`, `uint16_t`, `uint64_t`. no bare `int` for hardware values.
7. **warnings are errors**: `-Wall -Wextra -Werror`. asan+ubsan in debug builds.
8. **scope discipline**: implement exactly what the current milestone doc says. no "while i'm in here". out-of-scope items go in a `later.md` note, not in code.

## claude-docs index

| doc | read when |
|---|---|
| `claude-docs/conventions.md` | before writing any code — style, naming, comments, srp detail |
| `claude-docs/architecture.md` | before creating/moving files — layout, patterns, tick model, dependency rule |
| `claude-docs/testing.md` | before every PR — test layers, harnesses, what each milestone must add |
| `claude-docs/security.md` | when touching parsing, memory indexing, save states, wasm |
| `claude-docs/workflow.md` | pr process, branch names, how milestone docs are consumed |
| `claude-docs/milestones/NN-*.md` | the spec for the pr you are implementing |

## build and test (once milestone 00 lands)

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./tools/run-test-roms.sh          # integration gate, milestone 05+
```
