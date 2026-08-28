#include "crossy.h"

#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t text_len(const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

static void print_centered(uint8_t y, const char* text) {
    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    printf("%s", text);
}

// the hover screen; the only screen this skeleton draws
static void draw_title(void) {
    BGP_REG = kTitleBgp;
    SCX_REG = 0;
    SCY_REG = 0;
    cls();
    print_centered(kTitleTextY, "CROSSY");
    print_centered(kPromptTextY, "SPACE TO START");
    print_centered(kBestTextY, "BEST 0");
}

void main(void) {
    font_init();
    font_set(font_load(font_ibm));
    draw_title();
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        vsync();
    }
}
