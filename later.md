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
