# later

out-of-scope items noticed during milestones, per claude.md rule 8.

- flappy: `world_x` is uint16_t; a single run past ~18 minutes (65535 px at
  1 px/frame) wraps and breaks pipe collision x math. fix: widen or make all
  pipe x comparisons wrap-relative. (noticed in milestone 15 sub-milestone 3)
- flappy: the game-over window banner covers the bottom 56 px of the frozen
  scene because a dmg window is full-width; revisit if it looks bad in play.
- flappy: the pipe rng seeds off `DIV_REG` at `world_init`, so any change to
  rom size or boot timing reshuffles the gaps and `kSurvivingFlaps` has to be
  re-searched. a seed the harness can pin (a fixed seed under a build flag, or
  seeding off the frame counter at the first flap as the milestone doc says)
  would make the scripted-run tests stable. (noticed in sub-milestone 4)
