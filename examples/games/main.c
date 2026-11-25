/*
 * Doodle Jump for Flashiibo (.feb)
 *
 * Portrait game: the 128x64 screen is used as a 64 wide x 128 tall play field.
 * Hold the device vertically. UP and DOWN steer right/left.
 *
 * Game space: gx = 0..63 (left to right), gy = 0..127 (top to bottom).
 * Only gpix() and grect() know about the rotation. If the picture comes out
 * upside down, use the "flipped" lines noted inside them.
 *
 * Everything is 8-bit and the screen is never fully cleared during play
 * (single-buffered display): things are erased and redrawn where they move.
 */
#include "../../include/feb.h"

#define W 64
#define H 128

#define NPLAT    6
#define PLAT_W   14
#define PW       7        /* player width  */
#define PH       6        /* player height (legs add 1) */
#define SCROLL_Y 50       /* player is held at or below this row */
#define STEP_T   6        /* frames per speed step (jump apex = 60 px) */
#define DEAD     255
#define MAGIC    0xD7

/* Swap these two if left/right feel reversed. */
#define KEY_RIGHT FEB_BTN_UP
#define KEY_LEFT  FEB_BTN_DOWN

/* 3x5 font, 5 bytes per glyph, low 3 bits of each row (bit 2 = left).
   0-9 digits, 10 G, 11 A, 12 M, 13 E, 14 O, 15 V, 16 R, 17 H, 18 I */
static const uint8_t font[95] = {
    7,5,5,5,7,  2,6,2,2,7,  7,1,7,4,7,  7,1,7,1,7,  5,5,7,1,1,
    7,4,7,1,7,  7,4,7,5,7,  7,1,1,1,1,  7,5,7,5,7,  7,5,7,1,7,
    7,4,5,5,7,  2,5,7,5,5,  5,7,7,5,5,  7,4,6,4,7,  7,5,5,5,7,
    5,5,5,5,2,  6,5,6,5,5,  5,5,7,5,5,  7,2,2,2,7
};

uint8_t plat_x[NPLAT];
uint8_t plat_y[NPLAT];
uint8_t px;
uint8_t py;
uint8_t pdir;       /* 0 = rising, 1 = falling */
uint8_t pv;         /* speed in px per frame, 1..4 */
uint8_t ptick;
uint8_t facing;     /* 0 = left, 1 = right */
uint8_t frame;
uint8_t next_gap;
uint8_t dirty;
uint8_t dg[8];      /* dg[0..3] score digits, dg[4..7] best score digits */
uint8_t sv[5];

/* ---------- rotation layer ---------- */
void gpix(uint8_t x, uint8_t y)
{
    if (x >= W || y >= H) return;
    feb_draw_pixel(y, W - 1 - x);              /* flipped: (H-1-y, x) */
}

void grect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    if (x >= W || y >= H) return;
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    feb_fill_rect(y, W - x - w, h, w);         /* flipped: (H-y-h, x, h, w) */
}

/* ---------- tiny font ---------- */
void glyph(uint8_t x, uint8_t y, uint8_t idx)
{
    uint8_t r;
    uint8_t base;
    uint8_t row;
    base = (idx << 2) + idx;                   /* idx * 5 */
    for (r = 0; r < 5; r++) {
        row = font[base + r];
        if (row & 4) gpix(x, y + r);
        if (row & 2) gpix(x + 1, y + r);
        if (row & 1) gpix(x + 2, y + r);
    }
}

/* draws 4 digits starting at dg[off], hiding leading zeros */
void show_num(uint8_t x, uint8_t y, uint8_t off)
{
    uint8_t i;
    uint8_t started;
    started = 0;
    for (i = 0; i < 4; i++) {
        if (dg[off + i] != 0 || started || i == 3) {
            glyph(x, y, dg[off + i]);
            x = x + 4;
            started = 1;
        }
    }
}

void score_inc(void)
{
    uint8_t i;
    i = 3;
    while (1) {
        dg[i] = dg[i] + 1;
        if (dg[i] < 10) return;
        dg[i] = 0;
        if (i == 0) {                          /* cap at 9999 */
            dg[0] = 9; dg[1] = 9; dg[2] = 9; dg[3] = 9;
            return;
        }
        i = i - 1;
    }
}

uint8_t beats_best(void)
{
    uint8_t i;
    for (i = 0; i < 4; i++) {
        if (dg[i] > dg[4 + i]) return 1;
        if (dg[i] < dg[4 + i]) return 0;
    }
    return 0;
}

void load_best(void)
{
    uint8_t i;
    feb_load_flags(sv, 5);
    for (i = 0; i < 4; i++) {
        if (sv[0] == MAGIC && sv[1 + i] < 10) dg[4 + i] = sv[1 + i];
        else dg[4 + i] = 0;
    }
}

void save_best(void)
{
    uint8_t i;
    sv[0] = MAGIC;
    for (i = 0; i < 4; i++) sv[1 + i] = dg[4 + i];
    feb_save_flags(sv, 5);
}

/* ---------- platforms ---------- */
void dplat(uint8_t i)
{
    grect(plat_x[i], plat_y[i], PLAT_W, 2);    /* DEAD rows are clipped away */
}

/* Fill dead slots above the highest live platform, as far as there is room. */
void fill_platforms(void)
{
    uint8_t i;
    uint8_t top;
    uint8_t x;
    top = H;
    for (i = 0; i < NPLAT; i++) {
        if (plat_y[i] < top) top = plat_y[i];
    }
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    for (i = 0; i < NPLAT; i++) {
        if (plat_y[i] == DEAD && top >= next_gap) {
            x = feb_rand(0x3F);
            if (x > W - PLAT_W) x = x - (W - PLAT_W);
            plat_x[i] = x;
            top = top - next_gap;
            plat_y[i] = top;
            dplat(i);
            next_gap = 26 + feb_rand(0x0F);    /* 26..41 px, apex is 60 */
        }
    }
}

/* ---------- player ---------- */
void erase_player(void)
{
    uint8_t ex;
    uint8_t ew;
    feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);
    if (px >= 2) ex = px - 2; else ex = 0;
    ew = px + PW + 2;
    if (ew > W) ew = W;
    grect(ex, py, ew - ex, PH + 1);
}

/* erasing the player can nick a platform it overlaps: redraw those */
void repair_platforms(void)
{
    uint8_t i;
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    for (i = 0; i < NPLAT; i++) {
        if (plat_y[i] < H && plat_y[i] + 2 > py && plat_y[i] <= py + PH) {
            dplat(i);
        }
    }
}

void draw_player(void)
{
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    grect(px, py, PW, PH);
    grect(px + 1, py + PH, 1, 1);
    grect(px + PW - 2, py + PH, 1, 1);
    if (facing) {
        if (px + PW + 2 <= W) grect(px + PW, py + 2, 2, 1);
    } else {
        if (px >= 2) grect(px - 2, py + 2, 2, 1);
    }
    feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);    /* eye */
    if (facing) gpix(px + 4, py + 1);
    else gpix(px + 2, py + 1);
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
}

/* ---------- game flow ---------- */
void reset_game(void)
{
    uint8_t i;
    feb_clear_screen();
    for (i = 0; i < NPLAT; i++) {
        plat_x[i] = 0;
        plat_y[i] = DEAD;
    }
    for (i = 0; i < 4; i++) dg[i] = 0;
    plat_x[0] = 25;
    plat_y[0] = 120;
    next_gap = 26 + feb_rand(0x0F);
    px = 28;
    py = 120 - PH - 1;
    pdir = 0;
    pv = 4;
    ptick = 0;
    facing = 1;
    frame = 0;
    dirty = 0;
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    dplat(0);
    fill_platforms();
    draw_player();
    show_num(1, 1, 0);
}

void land(uint8_t prev_feet)
{
    uint8_t i;
    uint8_t feet;
    feet = py + PH + 1;
    for (i = 0; i < NPLAT; i++) {
        if (prev_feet <= plat_y[i] && feet >= plat_y[i]) {
            if (px + PW > plat_x[i] && px < plat_x[i] + PLAT_W) {
                py = plat_y[i] - PH - 1;
                pdir = 0;
                pv = 4;
                ptick = 0;
                return;
            }
        }
    }
}

void scroll(void)
{
    uint8_t d;
    uint8_t i;
    if (py >= SCROLL_Y) return;
    d = SCROLL_Y - py;
    py = SCROLL_Y;

    feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);
    for (i = 0; i < NPLAT; i++) dplat(i);
    for (i = 0; i < NPLAT; i++) {
        if (plat_y[i] < H) {
            plat_y[i] = plat_y[i] + d;
            if (plat_y[i] >= H) plat_y[i] = DEAD;
        }
    }
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    for (i = 0; i < NPLAT; i++) dplat(i);

    for (i = 0; i < d; i++) score_inc();
    dirty = 1;
    fill_platforms();
}

void step(void)
{
    uint8_t k;
    uint8_t dx;
    uint8_t prev_feet;

    frame = frame + 1;
    erase_player();
    repair_platforms();

    /* horizontal: 1,2,1,2 px per frame; wraps by jumping to the other edge */
    k = feb_get_keys();
    dx = 1 + (frame & 1);
    if (k & KEY_RIGHT) {
        facing = 1;
        px = px + dx;
        if (px > W - PW) px = 0;
    } else if (k & KEY_LEFT) {
        facing = 0;
        if (px < dx) px = W - PW;
        else px = px - dx;
    }

    /* vertical: speed steps 4,3,2,1 going up, then 1,2,3,4 coming down */
    prev_feet = py + PH + 1;
    ptick = ptick + 1;
    if (pdir == 0) {
        py = py - pv;
        if (ptick >= STEP_T) {
            ptick = 0;
            pv = pv - 1;
            if (pv == 0) { pdir = 1; pv = 1; }
        }
    } else {
        py = py + pv;
        if (ptick >= STEP_T) {
            ptick = 0;
            if (pv < 4) pv = pv + 1;
        }
        land(prev_feet);
    }

    scroll();
    draw_player();

    if (dirty && (frame & 7) == 0) {
        feb_set_draw_mode(FEB_DRAW_MODE_CLEAR);
        grect(1, 1, 17, 6);
        feb_set_draw_mode(FEB_DRAW_MODE_SET);
        show_num(1, 1, 0);
        dirty = 0;
    }
}

void game_over(void)
{
    if (beats_best()) {
        dg[4] = dg[0]; dg[5] = dg[1]; dg[6] = dg[2]; dg[7] = dg[3];
        save_best();
    }
    feb_clear_screen();
    feb_set_draw_mode(FEB_DRAW_MODE_SET);
    glyph(26, 20, 10); glyph(30, 20, 11); glyph(34, 20, 12); glyph(38, 20, 13);
    glyph(26, 28, 14); glyph(30, 28, 15); glyph(34, 28, 13); glyph(38, 28, 16);
    show_num(26, 50, 0);
    glyph(26, 70, 17); glyph(30, 70, 18);
    show_num(26, 78, 4);
    feb_delay_frames(60);
    feb_wait_key();
}

int main(void)
{
    feb_set_high_res(true);
    load_best();
    while (1) {
        reset_game();
        while (py < H) {
            step();
            feb_delay_frames(1);
        }
        game_over();
    }
    return 0;
}
