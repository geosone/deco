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
#include <stdio.h>
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

/*
 * A key as xterm sends it with its modifiers: ESC [ num ; mod final, or
 * ESC O mod final (mod - 1: 1 Shift, 2 Alt, 4 Ctrl; 1 for none). The
 * commander's code for it, 0 for one it has no use for.
 */
int xtermkey(int final, int num, int mod)
{
    static const signed char fnum[25] = { 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0,  1, 2,
                                          3, 4, 5, 0, 6, 7, 8, 9, 10, 0, 11, 12 };
    int f = 0, k = 0, m = mod > 1 ? mod - 1 : 0;

    switch (final) {
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
        f = final - 'P' + 1;
        break;
    case 'A':
        k = meta('u');
        break;
    case 'B':
        k = meta('d');
        break;
    case 'C':
        k = meta('r');
        break;
    case 'D':
        k = meta('l');
        break;
    case 'H':
        k = meta('h');
        break;
    case 'F':
        k = meta('e');
        break;
    case '~':
        if (num >= 11 && num <= 24)
            f = fnum[num];
        else if (num == 5)
            k = meta('p');
        else if (num == 6)
            k = meta('n');
        else if (num == 1 || num == 7)
            k = meta('h');
        else if (num == 4 || num == 8)
            k = meta('e');
        else if (num == 2)
            k = cntrl('T');
        else if (num == 3)
            k = cntrl('G');
        break;
    }
    if (f) {
        if (m & 4)
            return KEY_CF(f);
        if (m & 2)
            return KEY_AF(f);
        if (m & 1)
            return KEY_SF(f);
        return f <= 10 ? meta(('A' + f - 1)) : 0;
    }
    if (k && (m & 4)) /* Ctrl and a cursor key */
        switch (k) {
        case meta('l'):
            return KEY_CLEFT;
        case meta('r'):
            return KEY_CRIGHT;
        case meta('h'):
            return KEY_CHOME;
        case meta('e'):
            return KEY_CEND;
        case meta('p'):
            return KEY_CPGUP;
        case meta('n'):
            return KEY_CPGDN;
        case meta('u'):
            return KEY_CUP;
        case meta('d'):
            return KEY_CDOWN;
        case cntrl('G'):
            return KEY_CDEL;
        }
    return k; /* with Shift or Alt: the key itself */
}

/* whether s (n bytes after the ESC) is a whole key sequence yet */
int xtermdone(const unsigned char *s, int n)
{
    int i = n && s[0] == 033; /* Alt as an ESC before the key */

    if (n <= i)
        return 0;
    if (s[i] == 'O')
        return n > i + 1 &&
               ((s[n - 1] >= 'A' && s[n - 1] <= 'Z') || (s[n - 1] >= 'a' && s[n - 1] <= 'z'));
    if (s[i] != '[')
        return 1;                     /* Alt and a key: no sequence */
    if (n > i + 1 && s[i + 1] == '[') /* ESC [ [ A */
        return n > i + 2;
    return n > i + 1 && s[n - 1] >= 0x40 && s[n - 1] <= 0x7e;
}

/*
 * A whole sequence after the ESC (n bytes): the key, 0 one of no use, -1
 * none of the kind (xterm's keys with modifiers, Alt as an ESC before a
 * key, the Linux console's F1-F5, the mouse).
 */
int xtermseq(const unsigned char *s, int n)
{
    char buf[40];
    int alt = 0, num = 0, mod = 1, f, final;

    if (n > 0 && s[0] == 033)
        alt = 1, ++s, --n;
    if (n == 1 && (s[0] == 0177 || s[0] == '\b'))
        return KEY_ABS; /* Alt-Backspace */
    if (n < 2 || n >= (int)sizeof(buf))
        return -1;
    memcpy(buf, s, n);
    buf[n] = 0;
    final  = (unsigned char)buf[n - 1];
    if (buf[0] == '[' && buf[1] == '<')
        return alt ? -1 : mousestr(buf + 2);
    if (buf[0] == '[' && buf[1] == '[') { /* ESC [ [ A ... E */
        if (final < 'A' || final > 'E')
            return 0;
        f = final - 'A' + 1;
        return alt ? KEY_AF(f) : meta(('A' + f - 1));
    }
    if (buf[0] == 'O')
        sscanf(buf + 1, "%d", &mod);
    else if (buf[0] == '[')
        sscanf(buf + 1, "%d;%d", &num, &mod);
    else
        return -1;
    if (mod < 1)
        mod = 1;
    if (alt)
        mod = ((mod - 1) | 2) + 1;
    return xtermkey(final, num, mod);
}
