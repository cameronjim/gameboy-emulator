#ifndef CROSSY_SFX_H
#define CROSSY_SFX_H

void sfx_init(void);
void sfx_hop(void);
void sfx_splash(void);
void sfx_hit(void);
void sfx_score(void);
// train warning ding; stateless, so the caller calls this twice for the two-note ring
void sfx_bell(void);

#endif
