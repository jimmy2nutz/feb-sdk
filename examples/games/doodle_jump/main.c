/*
 * Doodle Jump for Flashiibo (.feb)  -- lean build
 *
 * Portrait game: the 128x64 screen is used as a 64 wide x 128 tall play field.
 * Hold the device vertically. UP and DOWN steer right/left.
 *
 * Screen mapping: game height runs along screen X, game width along screen Y,
 * so positions are stored directly as screen coordinates:
 *   py      = screen X of the player's top   (bigger = lower in the game)
 *   pu      = screen Y of the player's side  (bigger = further LEFT in the game)
 *   plat_y  = screen X of a platform, plat_u = screen Y of a platform
 * The score uses the built-in font, so it reads rotated 90 degrees.
 *
 * Everything that must be erased is drawn with feb_fill_rrect: on the device
 * feb_fill_rect ignores FEB_DRAW_MODE_CLEAR, which left trails behind platforms.
 * Also avoids "constant - variable" expressions and variable draw modes.
 */
#include "../../include/feb.h"

#define NPLAT    6
#define PLAT_W   14
#define PLAT_H   3
#define BOTTOM   124      /* platforms below this row are dropped */
#define PW       7
#define PH       6
#define SCROLL_Y 50
#define STEP_T   6
#define DEAD     255

/* Key bits from feb_get_keys(): UP=1 DOWN=2 LEFT=4 RIGHT=8.
   Plain numbers on purpose (no alias of another #define).
   RIGHT_KEYS = UP or RIGHT(OK), LEFT_KEYS = DOWN or LEFT(BACK).
   Swap the two values if steering feels reversed. */
#define RIGHT_KEYS 9
#define LEFT_KEYS  6

uint8_t plat_u[NPLAT];
uint8_t plat_y[NPLAT];
uint8_t pu;
uint8_t py;
uint8_t rising;     /* 1 while going up, 0 while falling */
uint8_t spd;        /* px per frame, 1..4 */
uint8_t tick;
uint8_t top_y;      /* y of the highest platform */
uint8_t gap;        /* distance to the next platform to spawn */
uint8_t score;

void dplat(uint8_t i)
{
    if (plat_y[i] < BOTTOM)
        feb_fill_rrect(plat_y[i], plat_u[i], PLAT_H, PLAT_W);
}

void dplats(void)
{
    uint8_t i;
    for (i = 0; i < NPLAT; i++) dplat(i);
}

void erase_player(void)
{
    feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);
    feb_fill_rrect(py, pu, PH, PW);
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
}

void draw_player(void)
{
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    feb_fill_rrect(py, pu, PH, PW);
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
    uint8_t u;
    for (i = 0; i < NPLAT; i++) {
        if (plat_y[i] == DEAD) {
            u = feb_rand(0x3F);
            if (u > 50) u = u - 50;
            plat_u[i] = u;
            top_y = top_y - gap;
            plat_y[i] = top_y;
            gap = feb_rand(0x0F) + 26;         /* 26..41 px, jump apex is 60 */
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
    plat_u[0] = 20;
    plat_y[0] = 120;
    top_y = 120;
    gap = feb_rand(0x0F) + 26;
    for (i = 0; i < NPLAT; i++) {
        if (top_y >= gap) spawn();
    }
    score = 0;
    pu = 25;
    py = 114;
    rising = 1;
    spd = 4;
    tick = 0;
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    dplats();
    draw_player();
    show_score();
}

/* when the player climbs above SCROLL_Y, move the world down instead */
void scroll(void)
{
    uint8_t d;
    uint8_t i;
    uint8_t old;
    if (py < SCROLL_Y) {
        d = 0;
        while (py < SCROLL_Y) {
            py = py + 1;
            d = d + 1;
        }
        old = score;
        feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);
        dplats();
        for (i = 0; i < NPLAT; i++) {
            if (plat_y[i] != DEAD) {
                plat_y[i] = plat_y[i] + d;
                if (plat_y[i] >= BOTTOM) plat_y[i] = DEAD;
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

    erase_player();
    dplats();                                   /* repair anything the erase nicked */

    /* right = smaller screen Y, left = bigger; wraps to the other edge */
    k = feb_get_keys();
    if (k & RIGHT_KEYS) {
        if (pu < 2) pu = 57;
        else pu = pu - 2;
    } else if (k & LEFT_KEYS) {
        pu = pu + 2;
        if (pu > 57) pu = 0;
    }

    /* speed steps 4,3,2,1 going up, then 1,2,3,4 coming down */
    prev = py + PH;
    tick = tick + 1;
    if (rising) {
        py = py - spd;
        if (tick >= STEP_T) {
            tick = 0;
            spd = spd - 1;
            if (spd == 0) {
                rising = 0;
                spd = 1;
            }
        }
    } else {
        py = py + spd;
        if (tick >= STEP_T) {
            tick = 0;
            if (spd < 4) spd = spd + 1;
        }
        for (i = 0; i < NPLAT; i++) {           /* falling: land on a platform */
            if (prev <= plat_y[i] && py + PH >= plat_y[i]) {
                if (pu + PW > plat_u[i] && pu < plat_u[i] + PLAT_W) {
                    py = plat_y[i] - PH;
                    rising = 1;
                    spd = 4;
                    tick = 0;
                    i = NPLAT;
                }
            }
        }
    }

    scroll();
    draw_player();
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
