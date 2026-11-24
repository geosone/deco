/*
 * The terminal on newt: what cap.c, tty.c and key.c do on termcap and
 * termios, for the newt build (DECO_NEWT). newt (on S-Lang) has the
 * terminal set up and reads the keys; they are mapped here to deco's key
 * codes (main.c's keymap).
 */
#include <newt.h>
#include <slang.h>

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
        return (cntrl('C'));
    case NEWT_KEY_RESIZE:
        return (cntrl('L'));
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
