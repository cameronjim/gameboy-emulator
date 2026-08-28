# later

out-of-scope items noticed during milestones, per claude.md rule 8.

- flappy: the game-over window banner covers the bottom 56 px of the frozen
  scene because a dmg window is full-width; it is now a deliberate solid panel.
- flappy: rng now seeds off the hover frame counter, so scripted runs are
  stable across rom changes. (fixed after sub-milestone 4; scripts only need a
  re-search if the flow timing before the start press changes)
