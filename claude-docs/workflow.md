# workflow — one doc, one branch, one pr

## the loop

1. pick the next `claude-docs/milestones/NN-*.md` (they are ordered; do not skip).
2. **plan** the pr with fable: read the doc, restate scope, list files and tests. no code yet.
3. branch `feat/NN-shortname` off main (`feat/05-blargg`).
4. **implement** with opus (hard: cpu, ppu, apu, mbc, debugging) or sonnet (mechanical: codegen, boilerplate, tests scaffolding, frontend glue). routing rules in CLAUDE.md.
5. run: unit suite, then the rom manifest. fix until green.
6. open the pr. **review** with fable against the checklist below.
7. human reviews, tests by hand, merges to main. next doc.

one milestone per pr. if a milestone turns out too big mid-flight (02 and 07 might), split into stacked prs *within* the same doc — never blend two docs into one pr.

## pr template

```
## milestone
claude-docs/milestones/NN-name.md

## scope check
- in-scope items implemented: ...
- explicitly not done (out of scope): ...

## tests
- unit added: ...
- manifest lines added: ...
- all green locally + ci

## hardware citations
any timing/behavior decision not obvious from the doc: pan docs section quoted.
```

## review checklist (fable)

- scope matches the doc exactly — nothing extra, nothing missing.
- conventions.md respected: comment style (lowercase, single-line, minimal), naming, fixed-width ints, srp.
- core purity: zero platform includes in `core/`.
- every new behavior has a test that fails without the change.
- no timing constant changed without a pan docs citation.
- security.md rules for any parsing/indexing touched.

## commit style

small commits, lowercase imperative subject ≤ 60 chars: `add daa flag table`, `mask mbc1 bank to rom size`. no scopes, no emoji.

## when stuck

a divergence you can't explain after one trace-diff session: stop, write up the divergence (rom, pc, expected vs got) in the pr, and escalate to fable with pan docs open. do not brute-force constants until tests pass — that is how three games break to fix one test.
