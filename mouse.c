/*
 * The mouse, through the mouse reports of xterm and its kind (the GonWare
 * console sends them too): ESC [ ? 1000 h turns them on, ESC [ ? 1006 h
 * makes them ESC [ < b ; x ; y M (a press) or m (a release).
 *
 * A report becomes a key where the place says what it means everywhere:
 * the wheel is Up and Down, a click on the F-key bar its F-key. Any other
 * press is MOUSEKEY with MouseRow, MouseCol (0-based), MouseButton (0 left,
 * 1 middle, 2 right) and MouseDouble (a second click of the left button on
 * the same cell); the panels, the menus and the lists use it, others drop
 * it as an unknown key.
 *
 * Copyright (C) 2026 Mario Fetka
 * Under the MIT license, as the rest of the commander.
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "deco.h"
#include "scr.h"

int MouseRow, MouseCol, MouseButton, MouseDouble;

static int mouseon;

/* the reports on or off; DECO_NOMOUSE in the environment: never */
void VMouse(int on)
{
    static const char onseq[] = "\033[?1000h\033[?1006h", offseq[] = "\033[?1006l\033[?1000l";

    if (on && getenv("DECO_NOMOUSE"))
        return;
    if (on == mouseon)
        return;
    mouseon = on;
    if (on)
        (void)!write(1, onseq, sizeof(onseq) - 1);
    else
        (void)!write(1, offseq, sizeof(offseq) - 1);
}

/* milliseconds of a clock that only goes forward */
static long msnow(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/*
 * The report after ESC [ <: "b;x;y" and M or m, its bytes from get()
 * (< 0: none). The key it is, 0 for one to drop (a release, a move, a
 * report cut short).
 */
int mouseread(int (*get)(void))
{
    static long last;
    static int lastrow = -1, lastcol = -1;
    int v[3] = { 0, 0, 0 }, i = 0, c, b, k, slot;
    long now;

    for (;;) {
        if ((c = get()) < 0)
            return 0;
        if (c >= '0' && c <= '9')
            v[i] = v[i] * 10 + c - '0';
        else if (c == ';' && i < 2)
            ++i;
        else if (c == 'M' || c == 'm')
            break;
        else
            return 0;
    }
    b = v[0];
    if (c == 'm' || (b & 32)) /* a release, a move */
        return 0;
    if (b & 64) /* the wheel: 64 up, 65 down */
        return (b & 1) ? meta('d') : meta('u');
    MouseRow    = v[2] - 1;
    MouseCol    = v[1] - 1;
    MouseButton = b & 3;
    if (MouseButton == 3)
        return 0;
    if (MouseRow == LINES - 1) {
        /* the F-key bar: a tenth of the width for each key, as drawkeys */
        slot = COLS / 10;
        k    = slot > 0 ? MouseCol / slot : 0;
        if (k > 9)
            k = 9;
        return meta(('A' + k));
    }
    now = msnow();
    MouseDouble =
        MouseButton == 0 && MouseRow == lastrow && MouseCol == lastcol && now - last < 400;
    if (MouseButton == 0) {
        /* a third click starts again */
        lastrow = MouseDouble ? -1 : MouseRow;
        lastcol = MouseCol;
        last    = now;
    }
    return MOUSEKEY;
}

/* the report from a string, as the newt backend has read it */
static const char *mstr;

static int strget(void)
{
    return *mstr ? (unsigned char)*mstr++ : -1;
}

int mousestr(const char *s)
{
    mstr = s;
    return mouseread(strget);
}
