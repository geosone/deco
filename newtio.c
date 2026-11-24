/*
 * The terminal on newt: what cap.c, tty.c and key.c do on termcap and
 * termios, for the newt build (DECO_NEWT). newt (on S-Lang) has the
 * terminal set up and reads the keys; they are mapped here to deco's key
 * codes (main.c's keymap).
 */
#include <newt.h>
#include <slang.h>
#include <stdio.h>
#include <stdlib.h>

#include "scr.h"

/* newt's key reader with its escape sequences, exported but declared only
 * in newt_pr.h */
int newtGetKey(void);

static int keyback = -1;

int CapInit(char *bp)
{
    (void)bp;
    return (1);
}

void CapGet(struct CapTab *t)
{
    (void)t;
}

char *CapGoto(char *CM, int destcol, int destline)
{
    (void)CM;
    (void)destcol;
    (void)destline;
    return ("");
}

void TtySet(void)
{
}

void TtyReset(void)
{
}

void TtyFlushInput(void)
{
    SLang_flush_input();
}

void KeyInit(struct KeyMap *map, void (*flushproc)(void))
{
    (void)map;
    (void)flushproc;
}

/*
 * F1/F2 with Alt or Shift, which newt does not know, after the ESC newt
 * gave back:
 *   ESC [ 1 ; m P, ESC [ 1 1 ; m ~   xterm, VTE, Konsole (m - 1: 1 Shift, 2 Alt)
 *   ESC O m P                         older xterm and Konsole
 *   ESC ESC O P, ESC ESC [ 1 1 ~, ESC ESC [ [ A
 *                                     Alt as an ESC before the key (rxvt,
 *                                     "meta sends escape", the Linux console)
 * Alt-F1/Alt-F2, Shift-F1/Shift-F2 choose the drive of the left and right
 * panel (on GonWare Alt-F1... switch the screens). Other keys of this form
 * are dropped; anything else goes back and the ESC stays an ESC.
 */
/* also: the mouse reports ESC [ < b ; x ; y M (mouse.c) */
static int modkey(void)
{
    unsigned char b[32];
    int n = 0, alt = 0, num = 0, mod = 1, c, f = 0, i;

    if (SLang_input_pending(0) <= 0)
        return -1;
    b[n++] = c = SLang_getkey();
    if (c == 033 && SLang_input_pending(1) > 0) { /* Alt as a leading ESC */
        alt    = 1;
        b[n++] = c = SLang_getkey();
    }
    if (c != '[' && c != 'O')
        goto back;
    while (n < (int)sizeof(b) - 1 && SLang_input_pending(1) > 0) {
        b[n++] = c = SLang_getkey();
        if (c >= 0x40 && c <= 0x7e && !(c == '[' && b[n - 2] == '['))
            break;
    }
    if (!alt && b[0] == '[' && b[1] == '<') { /* a mouse report */
        b[n] = 0;
        return mousestr((char *)b + 2);
    }
    i = alt + 1; /* the parameters after [ or O */
    if (b[i] == '[' && b[alt] == '[') {
        if (c >= 'A' && c <= 'E') /* Linux console: ESC [ [ A */
            f = c - 'A' + 1;
    } else {
        if (b[alt] == 'O')
            sscanf((char *)b + i, "%d", &mod); /* ESC O m P */
        else
            sscanf((char *)b + i, "%d;%d", &num, &mod);
        if (c >= 'P' && c <= 'S')
            f = c - 'P' + 1;
        else if (c == '~' && num >= 11 && num <= 15)
            f = num - 10;
        else
            goto back;
    }
    if (mod < 1)
        mod = 1;
    if (alt)
        mod = ((mod - 1) | 2) + 1; /* with Alt */
    if ((mod - 1) & 3) {
        if (f == 1)
            return meta('1');
        if (f == 2)
            return meta('2');
    }
    return 0;
back:
    while (n > 0)
        SLang_ungetkey(b[--n]);
    return -1;
}

static int newt_key(void)
{
    int k;

    if (keyback >= 0) {
        k       = keyback;
        keyback = -1;
        return (k);
    }
    VFlush();
    k = newtGetKey();
    switch (k) {
    case NEWT_KEY_UP:
        return (meta('u'));
    case NEWT_KEY_DOWN:
        return (meta('d'));
    case NEWT_KEY_LEFT:
        return (meta('l'));
    case NEWT_KEY_RIGHT:
        return (meta('r'));
    case NEWT_KEY_PGUP:
        return (meta('p'));
    case NEWT_KEY_PGDN:
        return (meta('n'));
    case NEWT_KEY_HOME:
        return (meta('h'));
    case NEWT_KEY_END:
        return (meta('e'));
    case NEWT_KEY_INSERT:
        return (cntrl('T'));
    case NEWT_KEY_DELETE:
        return (cntrl('G'));
    case NEWT_KEY_BKSPC:
        return (meta('b'));
    case NEWT_KEY_ESCAPE:
        if ((k = modkey()) == 0)
            return (newt_key()); /* dropped: a mouse release, ... */
        if (k > 0)
            return (k);
        return (cntrl('C'));
    case NEWT_KEY_RESIZE:
        return (cntrl('L'));
    case NEWT_KEY_ERROR: /* the terminal gone (a hangup): go too */
        VClose();
        exit(0);
    default:
        if (k >= NEWT_KEY_F1 && k <= NEWT_KEY_F10)
            return (meta(('A' + (k - (NEWT_KEY_F1)))));
        if (k >= NEWT_KEY_EXTRA_BASE)
            return (0);
        return (k & 0377);
    }
}

int KeyGet(void)
{
    return (newt_key());
}

int KeyGetChar(void)
{
    return (newt_key());
}

void KeyUnget(int key)
{
    keyback = key;
}
