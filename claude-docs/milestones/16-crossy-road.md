# milestone 16 — crossy road, our second game rom

**branch:** main (direct) · **plan/review:** fable · **implement:** sonnet (skeleton), opus (the rest)
**depends on:** 15 (reuse flappy's proven patterns) · **effort:** 4-6 sessions

## the game

endless frogger: a chick hops on a 16 px grid through procedurally generated
lanes — grass (trees block), roads (cars, one direction per lane), rivers
(ride logs or drown) and train tracks (warned, fast, lethal). score is the
furthest lane reached. idling summons the eagle. difficulty ramps with
distance. gbdk-2020 c, mbc1+ram+battery, title `CROSSY`, best score in sram.

## hardware budgets (pinned; violating these breaks on real dmg)

- lanes are 16 px (2 tile rows); 10 columns of 16 px; camera scrolls
  vertically via scy over a 32-row bg ring (16 buffered lanes), streaming one
  lane (2 rows x 20 tiles) per crossing, written only in vblank.
- movers are 8x8 sprite pairs/triples: car = 2 sprites (16 px), log = 3
  (24 px), train = handled as a sweeping 4-sprite block. per-lane caps:
  3 cars or 2 logs, so one scanline never exceeds 3x2 (cars) or 2x3 (logs)
  + player + hud = 10 sprites. oam worst case ~34 of 40.
- player is one 8x8 sprite centered in its 16 px cell.

## test/tile-id contract (the emulator is the harness, as in milestone 15)

- bg terrain: grass 0xA0, tree 0xA1, road 0xA2, road stripe 0xA3, water 0xA4,
  rail 0xA5, rail warning 0xA6. popup inverted font at 0x60-0x9F.
- sprites: player 0xE0-0xE3 (idle/hop frames), car 0xC0-0xC1, log 0xC4,
  train 0xC8-0xCB, hud digits 0xD0-0xD9.
- rng seeds from the hover frame counter (flappy lesson: scripted tests stay
  valid across rom changes). all text lines even length, print_centered.
- tests read framebuffer_tiles(); directional assertions only.

## generation rules (researched from the original)

- lanes come in seeded chunks; never an impossible row: grass lanes always
  leave a gap in the trees, river lanes guarantee a reachable log, road gaps
  never close completely. first 3 lanes are plain grass.
- roads cluster 1-3 lanes, each lane one direction and one speed; rivers
  cluster 1-2; tracks appear singly, always right-to-left, warning light
  blinks ~1s before the train sweeps through.
- ramp by distance: car speed up, spacing down, bigger road/river clusters.

## sub-milestones (one push each, 3-4 commits, review + ci green between)

1. **skeleton** (sonnet): games/crossy/ + cmake rom target + ci job covers
   both games; title card (CROSSY / SPACE TO START / BEST 0). tests: cart
   header, boot render, determinism.
2. **grid, camera, score** (opus): terrain streaming, grass+trees gen, 4-way
   hop with animation, tree blocking, camera follows forward hops, hud score
   = furthest lane. tests: hops move/block, camera streams, score counts.
3. **roads** (opus): car lanes, per-lane direction/speed/spacing with
   guaranteed gaps, collision death, popup + sram best + space-to-retry flow
   ported from flappy. tests: motion, gaps, death, popup solidity, sram.
4. **rivers** (opus): logs carry the player (x follows log speed), water
   without a log drowns, carried off either edge dies, reachability rule.
   tests: riding, drowning, edge death.
5. **pressure and ramp** (opus): eagle idle timer (~600 frames), slow
   auto-camera with fall-off-bottom death, distance ramp table. tests: idle
   death, ramp measured via scripts.
6. **trains and polish** (opus): tracks + warning + train sweep, sfx (hop,
   splash, hit, train bell), readme manual section, dist ship. tests: warning
   precedes train, track death, sfx samples.

## traps

- sdcc 8-bit promotion: cast before multiply; keep per-frame work o(lanes).
- oam is written via gbdk's shadow; never poke mid-frame.
- one lane streamed per hop maximum — a hop can cross at most one lane.
- vertical ring wrap: lane row = (lane * 2) & 31; scy wraps for free.
- don't let generation read the rng when streaming off-screen rows twice —
  generate per lane index, cache in the ring, like flappy's gap_top array.
- scripted tests: hop scripts, not flap scripts; searcher needs 4 buttons.

## done when

playable crossy road: hop, dodge ramping traffic, ride logs, dread trains,
die to the eagle when afk; score + best persist; all game tests green in ci
next to flappy's and the emulator's own.
