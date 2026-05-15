# milestone 10 — dmg-acid2 pixel-perfect

**branch:** `feat/10-acid2` · **plan/review:** fable · **debugging:** fable + opus
**depends on:** 09 · **effort:** 1 week

## goal

dmg-acid2 renders a smiley face where every rendering defect distorts a documented facial feature. pass it pixel-for-pixel and the ppu's lcdc edge cases, sprite priority, and window line counter are hardened for the whole library.

## spec

- finish the harness's framebuffer channel: run the rom a fixed frame count, hash the index buffer, compare to the committed known-good hash; on mismatch write a ppm artifact next to the log.
- manifest: `dmg-acid2.gb`, channel=framebuffer.
- debugging loop: diff your ppm against the reference png feature by feature; the repo's readme maps each deformity to its cause (e.g. missing left eye = 8×16 low-bit masking; mouth artifacts = window line counter; eyebrow issues = bg-over-obj). fix, add the unit test that would have caught it, rerun.
- expected fix areas: window internal line counter exactness, dmg x-priority if deferred from 09, lcdc mid-frame toggles, bg-over-obj corners.

## tests

- manifest acid2 green with committed hash.
- each fix lands a named unit regression, same rule as milestone 05.

## done when

hash matches; ppm is pixel-identical to the reference png by manual diff once; ci runs it.

## traps

- "fixing" by nudging rendering until the hash matches without understanding which documented quirk was wrong — the readme names the cause; use it.
- updating the committed hash to match broken output. the reference png is the truth, not your last run.
