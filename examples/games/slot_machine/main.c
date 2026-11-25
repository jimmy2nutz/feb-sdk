/*
 * Slot Machine for Flashiibo (.feb)
 *
 * Hold the device normally (landscape). Press ANY button to pull the lever,
 * then press a button again to stop each reel, left to right.
 *
 * Win = three of a kind, or any two cherries: the reel frames blink.
 * Three sevens is the jackpot and blinks for longer.
 *
 * Drawing notes (learned the hard way on the real device):
 *  - sprites and rectangles always XOR, so each reel image is erased by
 *    drawing it again; the reel frames are drawn once and blinked by
 *    redrawing them
 *  - kept free of "constant - variable" math, 2D arrays and multi-variable
 *    declarations, which the SDK compiler does not like
 */
#include "../../include/feb.h"

#define STEP 3            /* frames between reel changes while spinning */

static const uint8_t spr_seven[32] = {
    0x00, 0x00, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC,
    0x00, 0x1C, 0x00, 0x1C, 0x00, 0x38, 0x00, 0x38,
    0x00, 0x70, 0x00, 0x70, 0x00, 0xE0, 0x00, 0xE0,
    0x01, 0xC0, 0x01, 0xC0, 0x03, 0x80, 0x00, 0x00
};

static const uint8_t spr_bar[32] = {
    0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE, 0x3F, 0xFC,
    0x7F, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE,
    0x3F, 0xFC, 0x7F, 0xFE, 0x00, 0x00, 0x00, 0x00,
    0x7F, 0xFE, 0x3F, 0xFC, 0x7F, 0xFE, 0x00, 0x00
};

static const uint8_t spr_bell[32] = {
    0x01, 0x80, 0x01, 0x80, 0x00, 0x80, 0x07, 0xF0,
    0x0F, 0xF8, 0x0F, 0xF8, 0x0F, 0xF8, 0x1F, 0xFC,
    0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x7F, 0xFE,
    0x7F, 0xFE, 0x01, 0x80, 0x01, 0xC0, 0x00, 0x80
};

static const uint8_t spr_cherry[32] = {
    0x00, 0x00, 0x00, 0x7C, 0x00, 0xFC, 0x01, 0x44,
    0x03, 0x20, 0x02, 0x20, 0x06, 0x20, 0x04, 0x10,
    0x08, 0x10, 0x3E, 0x7C, 0x2E, 0x5C, 0x7F, 0xFE,
    0x3E, 0x7C, 0x3E, 0x7C, 0x08, 0x10, 0x00, 0x00
};

static const uint8_t spr_lemon[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xC0,
    0x1F, 0xF8, 0x27, 0xFC, 0x7F, 0xFF, 0x7F, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC,
    0x1F, 0xF8, 0x03, 0xC0, 0x00, 0x00, 0x00, 0x00
};

/* 0 seven, 1 bar, 2 bell, 3 cherry, 4 lemon  (weights: 1,2,2,2,1 out of 8) */
static const uint8_t smap[8] = { 0, 1, 1, 2, 2, 3, 3, 4 };
static const uint8_t rx[3] = { 25, 56, 87 };

uint8_t cur[3];

void draw_sym(uint8_t r, uint8_t s)
{
    uint8_t x;
    x = rx[r];
    if (s == 0) feb_draw_sprite16(x, 24, spr_seven);
    else if (s == 1) feb_draw_sprite16(x, 24, spr_bar);
    else if (s == 2) feb_draw_sprite16(x, 24, spr_bell);
    else if (s == 3) feb_draw_sprite16(x, 24, spr_cherry);
    else feb_draw_sprite16(x, 24, spr_lemon);
}

/* three reel windows; drawing them again erases them (used to blink) */
void windows(void)
{
    feb_draw_rect(22, 21, 22, 22);
    feb_draw_rect(53, 21, 22, 22);
    feb_draw_rect(84, 21, 22, 22);
}

uint8_t pick(void)
{
    uint8_t r;
    r = feb_rand(7);
    return smap[r];
}

void wait_release(void)
{
    while (feb_get_keys() != 0) feb_delay_frames(1);
}

void spin(void)
{
    uint8_t stopped;
    uint8_t k;
    uint8_t prev;
    uint8_t t;
    uint8_t r;
    uint8_t s;

    stopped = 0;
    t = 0;
    prev = feb_get_keys();                      /* the press that started the spin */
    while (stopped < 3) {
        k = feb_get_keys();
        if (k != 0 && prev == 0) stopped = stopped + 1;
        prev = k;
        t = t + 1;
        if (t >= STEP) {
            t = 0;
            for (r = stopped; r < 3; r++) {
                draw_sym(r, cur[r]);            /* XOR: erase the old symbol */
                s = pick();
                cur[r] = s;
                draw_sym(r, s);
            }
        }
        feb_delay_frames(1);
    }
}

/* 0 = nothing, 1 = win, 2 = jackpot (three sevens) */
uint8_t result(void)
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t n;
    a = cur[0];
    b = cur[1];
    c = cur[2];
    if (a == b && b == c) {
        if (a == 0) return 2;
        return 1;
    }
    n = 0;
    if (a == 3) n = n + 1;
    if (b == 3) n = n + 1;
    if (c == 3) n = n + 1;
    if (n == 2) return 1;
    return 0;
}

/* n must be even so the frames end up drawn */
void blink(uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n; i++) {
        windows();
        feb_delay_frames(8);
    }
}

int main(void)
{
    uint8_t w;
    uint8_t i;

    feb_set_high_res(true);
    feb_clear_screen();
    feb_set_draw_mode(FEB_DRAW_MODE_XOR);
    cur[0] = 0;
    cur[1] = 3;
    cur[2] = 1;
    windows();
    for (i = 0; i < 3; i++) draw_sym(i, cur[i]);

    while (1) {
        feb_wait_key();
        spin();
        w = result();
        if (w == 2) blink(16);
        else if (w == 1) blink(6);
        feb_delay_frames(20);
        wait_release();
    }
    return 0;
}
