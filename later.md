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
- crossy: an odd length hover line sits half a tile cell left of centre, so
  "BEST 12" is 4 px off while "BEST 0" is exact. print_centered works on the 20
  column grid; sub-cell centring would need the text drawn as sprites, as
  flappy's is not either.
- crossy: the mover pool is exactly full at its worst visible window (five water
  lanes, or four plus a track). a sixth danger lane on screen would need either a
  bigger pool or a tighter generation cap.
- crossy: a log still pops out of view when its centre crosses screen x 0, since
  the draw limit parks a mover by its track x rather than by its far edge. the
  shorter water lap did not change that.
