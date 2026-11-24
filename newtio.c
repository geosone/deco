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
 * A key with a modifier as xterm sends it, which newt does not know:
 * ESC [ 1 ; m P..S (F1-F4), ESC [ n ; m ~ (F5-F12); m - 1 the modifiers
 * (1 Shift, 2 Alt, 4 Ctrl). After the ESC newt gave back, the rest is
 * read here: Alt-F1/Alt-F2 and Shift-F1/Shift-F2 choose the drive of
 * the left and right panel (on GonWare Alt-F1... switch the screens);
 * other such keys are dropped. Not one of them: the bytes go back, the
 * ESC stays an ESC.
 */
static int modkey(void)
{
    unsigned char b[16];
    int n = 0, num = 0, mod = 0, c, f;

    if (SLang_input_pending(0) <= 0)
        return -1;
    b[n++] = c = SLang_getkey();
    if (c != '[') {
        SLang_ungetkey(c);
        return -1;
    }
    while (n < (int)sizeof(b) && SLang_input_pending(1) > 0) {
        b[n++] = c = SLang_getkey();
        if (c >= 0x40 && c <= 0x7e)
            break;
    }
    c = b[n - 1];
    if (sscanf((char *)b + 1, "%d;%d", &num, &mod) != 2 || !(c == '~' || (c >= 'P' && c <= 'S'))) {
        while (n > 0)
            SLang_ungetkey(b[--n]);
        return -1;
    }
    f = c == '~' ? 0 : c - 'P' + 1; /* F1-F4; F5-F12 by their number */
    if ((mod - 1) & 3) {            /* Shift or Alt */
        if (f == 1)
            return meta('1');
        if (f == 2)
            return meta('2');
    }
    return 0;
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
        if ((k = modkey()) >= 0)
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
