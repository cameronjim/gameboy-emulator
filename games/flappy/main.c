#include "flappy.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdio.h>

void main(void) {
    font_init();
    font_set(font_load(font_ibm));
    BGP_REG = kTitleBgp;
    cls();

    gotoxy(kTitleTextX, kTitleTextY);
    printf("FLAPPY");
    gotoxy(kPromptTextX, kPromptTextY);
    printf("PRESS START");

    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
    }
}
