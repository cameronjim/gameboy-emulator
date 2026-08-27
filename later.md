# later

out-of-scope items noticed during milestones, per claude.md rule 8.

- flappy: the game-over window banner covers the bottom 56 px of the frozen
  scene because a dmg window is full-width; it is now a deliberate solid panel.
- flappy: the pipe rng seeds off `DIV_REG` at `world_init`, so any change to
  rom size or boot timing reshuffles the gaps and `kSurvivingFlaps` has to be
  re-searched. a seed the harness can pin (a fixed seed under a build flag, or
  seeding off the frame counter at the first flap as the milestone doc says)
  would make the scripted-run tests stable. (noticed in sub-milestone 4)
