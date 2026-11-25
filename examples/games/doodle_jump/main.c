/*
 * Doodle Jump for Flashiibo (.feb)  -- lean build
 *
 * Portrait game: the 128x64 screen is used as a 64 wide x 128 tall play field.
 * Hold the device vertically. UP and DOWN steer right/left.
 *
 * Game space: gx = 0..63 (left to right), gy = 0..127 (top to bottom).
 * Screen mapping: screen_x = gy, screen_y = 63 - gx  (see dplat / draw_player).
 * The score is drawn with the built-in font, so it reads rotated 90 degrees.
 *
 * Kept small on purpose: the whole program must fit in ~3.5 KB of VM memory.
 */
#include "../../include/feb.h"

#define W 64
#define H 128
#define NPLAT    6
#define PLAT_W   14
#define PW       7
#define PH       6
#define SCROLL_Y 50
#define STEP_T   6
#define DEAD     255

/* Swap these two if left/right feel reversed. */
#define KEY_RIGHT FEB_BTN_UP
#define KEY_LEFT  FEB_BTN_DOWN

uint8_t plat_x[NPLAT];
uint8_t plat_y[NPLAT];
uint8_t px, py;     /* player left / top */
uint8_t v;          /* 0..3 rising (speed 4..1), 4 hover, 5..8 falling (1..4) */
uint8_t tick;
uint8_t top_y;      /* y of the highest platform */
uint8_t gap;        /* distance to the next platform to spawn */
uint8_t score;

void dplat(uint8_t i)
{
    if (plat_y[i] < 126)
        feb_fill_rect(plat_y[i], W - plat_x[i] - PLAT_W, 2, PLAT_W);
}

void dplats(void)
{
    uint8_t i;
    for (i = 0; i < NPLAT; i++) dplat(i);
}

void draw_player(uint8_t mode)
{
    feb_set_draw_mode(mode);
    feb_fill_rrect(py, W - px - PW, PH, PW);
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
}

void show_score(void)
{
    feb_set_draw_mode(FEB_DRAW_MODE_OPAQUE);
    feb_draw_number(0, 58, score, FEB_FONT_4X6);
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
}

/* put a new platform above the highest one (only called when there is room) */
void spawn(void)
{
    uint8_t i;
    uint8_t x;
    for (i = 0; i < NPLAT; i++) {
        if (plat_y[i] == DEAD) {
            x = feb_rand(0x3F);
            if (x > 50) x = x - 50;
            plat_x[i] = x;
            top_y = top_y - gap;
            plat_y[i] = top_y;
            gap = 26 + feb_rand(0x0F);         /* 26..41 px, jump apex is 60 */
            if (score < 255) score = score + 1;
            i = NPLAT;
        }
    }
}

void reset_game(void)
{
    uint8_t i;
    feb_clear_screen();
    for (i = 0; i < NPLAT; i++) plat_y[i] = DEAD;
    plat_x[0] = 25;
    plat_y[0] = 120;
    top_y = 120;
    gap = 26 + feb_rand(0x0F);
    for (i = 0; i < NPLAT; i++) {
        if (top_y >= gap) spawn();
    }
    score = 0;
    px = 28;
    py = 120 - PH;
    v = 0;
    tick = 0;
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    dplats();
    draw_player(FEB_DRAW_MODE_SET);
    show_score();
}

/* when the player climbs above SCROLL_Y, move the world down instead */
void scroll(void)
{
    uint8_t d;
    uint8_t i;
    uint8_t old;
    if (py < SCROLL_Y) {
        d = SCROLL_Y - py;
        py = SCROLL_Y;
        old = score;
        feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);
        dplats();
        for (i = 0; i < NPLAT; i++) {
            if (plat_y[i] != DEAD) {
                plat_y[i] = plat_y[i] + d;
                if (plat_y[i] >= 126) plat_y[i] = DEAD;
            }
        }
        top_y = top_y + d;
        if (top_y >= gap) spawn();
        feb_set_draw_mode(FEB_DRAW_MODE_SET);
        dplats();
        if (score != old) show_score();
    }
}

void step(void)
{
    uint8_t k;
    uint8_t prev;
    uint8_t i;

    draw_player(FEB_DRAW_MODE_CLEAR);
    dplats();                                   /* repair anything the erase nicked */

    k = feb_get_keys();
    if (k & KEY_RIGHT) {
        px = px + 2;
        if (px > W - PW) px = 0;
    } else if (k & KEY_LEFT) {
        if (px < 2) px = W - PW;
        else px = px - 2;
    }

    prev = py + PH;
    if (v < 4) py = py - (4 - v);
    else py = py + (v - 4);
    tick = tick + 1;
    if (tick >= STEP_T) {
        tick = 0;
        if (v < 8) v = v + 1;
    }

    if (v >= 4) {                               /* falling: land on a platform */
        for (i = 0; i < NPLAT; i++) {
            if (prev <= plat_y[i] && py + PH >= plat_y[i]) {
                if (px + PW > plat_x[i] && px < plat_x[i] + PLAT_W) {
                    py = plat_y[i] - PH;
                    v = 0;
                    tick = 0;
                    i = NPLAT;
                }
            }
        }
    }

    scroll();
    draw_player(FEB_DRAW_MODE_SET);
}

int main(void)
{
    feb_set_high_res(true);
    while (1) {
        reset_game();
        while (py < 122) {
            step();
            feb_delay_frames(1);
        }
        feb_clear_screen();
        feb_draw_number(58, 27, score, FEB_FONT_6X10);
        feb_delay_frames(60);
        feb_wait_key();
    }
    return 0;
}
