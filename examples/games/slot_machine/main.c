/*
 * Slot Machine for Flashiibo (.feb)
 *
 * Hold the device normally (landscape). Press ANY button to pull the lever,
 * then press a button again to stop each reel, left to right.
 *
 *   3 x SEVEN 100   3 x BAR 15   3 x BELL 8   3 x LEMON 30   3 x CHERRY 5
 *   any 2 CHERRIES 2          (each spin costs 1 credit, you start with 20)
 *
 * Drawing notes (learned the hard way on the real device):
 *  - sprites and rectangles always XOR, so each reel image is erased by
 *    drawing it again; the reel frames are drawn once and blinked by redrawing
 *  - text is drawn in OPAQUE mode, and every message is padded to the same
 *    width so a new message fully covers the old one
 *  - kept free of "constant - variable" math, 2D arrays and multi-variable
 *    declarations, which the SDK compiler does not like
 */
#include "../../include/feb.h"

#define STEP 3            /* frames between reel changes while spinning */
#define START_CREDITS 20

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
uint8_t credits;

void draw_sym(uint8_t r, uint8_t s)
{
    uint8_t x;
    x = rx[r];
    if (s == 0) feb_draw_sprite16(x, 21, spr_seven);
    else if (s == 1) feb_draw_sprite16(x, 21, spr_bar);
    else if (s == 2) feb_draw_sprite16(x, 21, spr_bell);
    else if (s == 3) feb_draw_sprite16(x, 21, spr_cherry);
    else feb_draw_sprite16(x, 21, spr_lemon);
}

/* three reel windows; drawing them again erases them (used to blink) */
void windows(void)
{
    feb_draw_rect(22, 18, 22, 22);
    feb_draw_rect(53, 18, 22, 22);
    feb_draw_rect(84, 18, 22, 22);
}

void show_credits(void)
{
    uint8_t x;
    feb_set_draw_mode(FEB_DRAW_MODE_OPAQUE);
    x = feb_draw_string(2, 1, "CREDITS ", FEB_FONT_6X10);
    x = feb_draw_number(x, 1, credits, FEB_FONT_6X10);
    feb_draw_string(x, 1, "   ", FEB_FONT_6X10);
    feb_set_draw_mode(FEB_DRAW_MODE_XOR);
}

/* every message is exactly 12 characters */
void msg(uint8_t m)
{
    feb_set_draw_mode(FEB_DRAW_MODE_OPAQUE);
    if (m == 0) feb_draw_string(28, 48, "PRESS A KEY!", FEB_FONT_6X10);
    else if (m == 1) feb_draw_string(28, 48, "  SPINNING  ", FEB_FONT_6X10);
    else if (m == 2) feb_draw_string(28, 48, "  YOU WIN!  ", FEB_FONT_6X10);
    else if (m == 3) feb_draw_string(28, 48, "  JACKPOT!  ", FEB_FONT_6X10);
    else if (m == 4) feb_draw_string(28, 48, "  NO LUCK   ", FEB_FONT_6X10);
    else feb_draw_string(28, 48, "  GAME OVER ", FEB_FONT_6X10);
    feb_set_draw_mode(FEB_DRAW_MODE_XOR);
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

    credits = credits - 1;
    show_credits();
    msg(1);

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

uint8_t payout(void)
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t n;
    a = cur[0];
    b = cur[1];
    c = cur[2];
    if (a == b && b == c) {
        if (a == 0) return 100;
        if (a == 1) return 15;
        if (a == 2) return 8;
        if (a == 3) return 5;
        return 30;
    }
    n = 0;
    if (a == 3) n = n + 1;
    if (b == 3) n = n + 1;
    if (c == 3) n = n + 1;
    if (n == 2) return 2;
    return 0;
}

void blink(void)
{
    uint8_t i;
    for (i = 0; i < 6; i++) {
        windows();
        feb_delay_frames(8);
    }
}

int main(void)
{
    uint8_t p;
    uint8_t i;

    feb_set_high_res(true);
    feb_clear_screen();
    feb_set_draw_mode(FEB_DRAW_MODE_XOR);
    credits = START_CREDITS;
    cur[0] = 0;
    cur[1] = 3;
    cur[2] = 1;
    windows();
    for (i = 0; i < 3; i++) draw_sym(i, cur[i]);

    while (1) {
        show_credits();
        if (credits == 0) {
            msg(5);
            feb_delay_frames(60);
            feb_wait_key();
            credits = START_CREDITS;
            wait_release();
        } else {
            msg(0);
            feb_wait_key();
            spin();
            p = payout();
            if (p == 0) {
                msg(4);
            } else {
                if (p == 100) msg(3);
                else msg(2);
                for (i = 0; i < p; i++) {
                    if (credits < 250) credits = credits + 1;
                }
                show_credits();
                blink();
            }
            feb_delay_frames(20);
            wait_release();
        }
    }
    return 0;
}
