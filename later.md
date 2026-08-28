# later

out-of-scope items noticed during milestones, per claude.md rule 8.

- flappy: the game-over window banner covers the bottom 56 px of the frozen
  scene because a dmg window is full-width; it is now a deliberate solid panel.
- flappy: rng now seeds off the hover frame counter, so scripted runs are
  stable across rom changes. (fixed after sub-milestone 4; scripts only need a
  re-search if the flow timing before the start press changes)
- crossy: a creep slide freezes input for its eight frames, the same way a hop
  slide does. a second slide accumulator would let a hop start mid-creep.
- crossy: the ramp tier is keyed by the lane being generated, which runs six
  lanes ahead of the furthest lane reached, so each tier arrives six lanes early.
- crossy: only stacked water lanes are forced to alternate direction. stacked
  road lanes can still share a direction and speed, which is harmless for
  dodging but makes some road chunks read as one wide lane.
- crossy: a track lane's quiet/warning/train cycle only ticks while the lane is
  on screen, so a track can scroll into view already mid-warning. keeping the
  per-frame work o(visible lanes) was worth that.
- crossy: two track lanes warning on the same frame ring one bell, not two,
  because terrain_tick_tracks folds every visible lane's bell into one flag.
- crossy: every track's warning light sits in the same map column (10). a
  per-lane light column would need another cached byte per ring slot.
- crossy: adding tracks changed the rng draw count from lane 15 on, so the
  pinned seeds of difficulty_ramps_car_speed had to be re-searched. any future
  generation change past lane 15 will move them again.
- crossy: the game over popup's SCORE and BEST lines are still bg text, so an
  odd length still sits half a tile cell left of centre. only the hover banner's
  best is pixel centred, because only it is drawn with the hud's digit sprites.
- crossy: the mover pool is exactly full at its worst visible window (five water
  lanes, or four plus a track). a sixth danger lane on screen would need either a
  bigger pool or a tighter generation cap.
- crossy: a log still pops out of view when its centre crosses screen x 0, since
  the draw limit parks a mover by its track x rather than by its far edge. the
  shorter water lap did not change that.
- crossy: hover draws no movers on the three lanes the banner covers, so a car
  or log on lanes 4-6 pops in over the unlock's three erase frames rather than
  sliding in. per-lane sprite clipping would fix it and cost a scanline test.
- crossy: the world's seed now comes from the free running frame counter at
  hover entry, so the boot world is one fixed world. a scripted test reaches any
  other world only by dying and choosing when to dismiss the popup, which is
  what the tests' enter_world helper does.
- crossy: the water ripple tile repeats every 8 px, so its ripples land on a
  regular grid across a lane. a second ripple tile alternated per cell would
  scatter them and cost one more tile id.
- crossy: colorize_crossy maps unmapped tiles through kGrayShades, where shade 3
  is white, so the inverted font band renders as a pale card even though a dmg
  shows it black. the game over popup has always looked that way; giving the
  0x60-0x9f range its own palette is a frontend change of its own.
- crossy: a ride snaps the chick onto the log's 8 px sprite grid, so it can pop
  up to 4 px sideways on landing and can only sit at three offsets on a log. it
  is what buys the oam x tie that keeps the rider drawn over its log.
- crossy: with a log 14 px tall in a 16 px lane there is no room for a rider
  above it, so the chick overlaps its log and wins on the tie instead. any
  future sprite that must sit above a mover has the same problem and no tie.
- crossy: the two log ends are the same rounded cap, so a 24 px log reads the
  same either way round. a cut end grain ring would need a third tile pair.
- crossy: 8x16 sprites cost twice the tile budget, so the sprite bank now runs
  0xb0-0xe3. only 0xba-0xbb, 0xdc-0xdf and 0xe4+ are left for new art.
