# later

out-of-scope items noticed during milestones, per claude.md rule 8.

- flappy: `world_x` is uint16_t; a single run past ~18 minutes (65535 px at
  1 px/frame) wraps and breaks pipe collision x math. fix: widen or make all
  pipe x comparisons wrap-relative. (noticed in milestone 15 sub-milestone 3)
- flappy: the game-over window banner covers the bottom 40 px of the frozen
  scene because a dmg window is full-width; revisit if it looks bad in play.
