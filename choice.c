#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "deco.h"
#include "dir.h"
#include "scr.h"

#define SWAP(a, b)      \
    {                   \
        register int t; \
        t = a;          \
        a = b;          \
        b = t;          \
    }

struct choice {
    char *name;
    int namlen;
    int r, c;
};

static struct choice tab[3];
static int cnum;
static void (*attron)();
static int (*attroff)();
static BOX *mbox;

static void initchoice(int row, int col, char *c1, char *c2, char *c3);
static void drawchoice(int ch);
static int menuchoice(void);

void error(char *s, ...)
{
    char buf[100];
    va_list ap;

    va_start(ap, s);
    vsnprintf(buf, sizeof(buf), s, ap);
    va_end(ap);
    getchoice(1, _(" Error "), buf, 0, _(" Ok "), 0, 0);
}

void message(char *name, char *s, ...)
{
    char buf[100];
    va_list ap;

    va_start(ap, s);
    vsnprintf(buf, sizeof(buf), s, ap);
    va_end(ap);
    getchoice(0, name, buf, 0, 0, 0, 0);
}

void endmesg()
{
    if (mbox)
        VUngetBox(mbox);
    mbox = 0;
}

static int getchoice1(int bold, char *head, char *msg, char *mesg2, char *c1, char *c2, char *c3);

/* in the dialog colours */
int getchoice(int bold, char *head, char *msg, char *mesg2, char *c1, char *c2, char *c3)
{
    int oldc = VSetColor(VCOL_DIALOG);
    int r    = getchoice1(bold, head, msg, mesg2, c1, c2, c3);

    VSetColor(oldc);
    return r;
}

static int getchoice1(int bold, char *head, char *msg, char *mesg2, char *c1, char *c2, char *c3)
{
    int len, ch;
    int isattr;
    register int r, c, w, h;
    BOX *box;
    char mesg[67];

    strncpy(mesg, msg, sizeof(mesg));
    mesg[sizeof(mesg) - 1] = 0;
    w                      = strlen(mesg);
    if (mesg2) {
        len = strlen(mesg2);
        if (len > w)
            w = len;
    }
    len = strlen(head);
    if (len > w)
        w = len;
    len = 0;
    if (c1)
        len += strlen(c1);
    if (c2)
        len += strlen(c2);
    if (c3)
        len += strlen(c3);
    if (len > w)
        w = len;
    h = 6;
    w += 10;
    if (bold)
        r = LINES / 2;
    else
        r = LINES / 4;
    c = (COLS - w) / 2;

    box    = VGetBox(r - 1, c - 2, h + 2, w + 4); /* save box */
    isattr = VStandOut();
    if (bold)
        VSetBold();
    else
        VSetDim();
    VFillBox(r - 1, c - 2, h + 2, w + 4, ' ');        /* clear */
    VDrawBox(r, c, h, w);                             /* draw margins */
    VMPutString(r, c + (w - strlen(head)) / 2, head); /* head */
    if (mesg2) {
        VMPutString(r + 1, c + (w - strlen(mesg)) / 2, mesg);
        VMPutString(r + 2, c + (w - strlen(mesg2)) / 2, mesg2);
    } else
        VMPutString(r + 2, c + (w - strlen(mesg)) / 2, mesg);

    if (c1) {
        if (isattr) {
            attron  = VStandEnd;
            attroff = VStandOut;
        } else {
            attron  = 0;
            attroff = 0;
        }
        initchoice(r + 4, c + w / 2, c1, c2, c3);

        ch = menuchoice();
        VStandEnd();
        VSetNormal();
        VUngetBox(box);
        VFreeBox(box);
        return (ch);
    } else {
        /* message */
        VStandEnd();
        VSetNormal();
        mbox = box;
        hidecursor();
        VSync();
#ifdef DOC
        KeyGet();
#endif
        return (0);
    }
}

static void initchoice(int row, int col, char *c1, char *c2, char *c3)
{
    register int i, w;

    cnum        = c2 ? (c3 ? 3 : 2) : 1;
    tab[0].name = c1;
    tab[1].name = c2;
    tab[2].name = c3;
    w           = cnum - 1;
    for (i = 0; i < cnum; ++i) {
        w += tab[i].namlen = strlen(tab[i].name);
        tab[i].r           = row;
    }
    tab[0].c = col - w / 2;
    tab[1].c = tab[0].c + tab[0].namlen + 1;
    tab[2].c = tab[1].c + tab[1].namlen + 1;
}

static int menuchoice()
{
    int ch = 0;

    for (;;) {
        drawchoice(ch);
        hidecursor();
        VSync();
        switch (KeyGet()) {
        default:
            VBeep();
            continue;
        case cntrl(']'): /* redraw screen */
            VRedraw();
            continue;
        case cntrl('['):
        case cntrl('C'):
        case meta('J'): /* f0 */
            return (-1);
        case cntrl('M'):
        case cntrl('J'):
            return (ch);
        case ' ':
        case cntrl('I'):
        case meta('r'): /* right */
            if (++ch >= cnum)
                ch = 0;
            continue;
        case meta('b'): /* backspace */
        case meta('l'): /* left */
            if (--ch < 0)
                ch = cnum - 1;
            continue;
        }
    }
}

static void drawchoice(int ch)
{
    register int i;

    for (i = 0; i < cnum; ++i) {
        if (i == ch) {
            if (attron)
                (*attron)();
            VMPutString(tab[i].r, tab[i].c, tab[i].name);
            if (!attron) {
                VMPutChar(tab[i].r, tab[i].c, '[');
                VMPutChar(tab[i].r, tab[i].c + strlen(tab[i].name) - 1, ']');
            }
            if (attroff)
                (*attroff)();
        } else
            VMPutString(tab[i].r, tab[i].c, tab[i].name);
    }
}

static char *editstring(int r, int c, int w, char *str, int cp)
{
    register int key, k;
    int firstkey = 1;

    if (cp) {
        for (cp = 0; str[cp]; ++cp)
            ;
        firstkey = 0;
    }
    for (;; firstkey = 0) {
        VClearBox(r, c, 1, w);
        VMPutString(r, c, str);
        VMove(r, c + cp);
        VSync();
        switch (key = KeyGet()) {
        default:
            if (key < ' ' || (key > '~' && key < 0300) || key > 0377) {
                VBeep();
                continue;
            }
            if (firstkey) {
                str[0] = key;
                str[1] = 0;
                cp     = 1;
                continue;
            }
            for (k = cp; str[k]; ++k)
                SWAP(key, str[k]);
            str[k] = key;
            str[w] = str[k + 1] = 0;
            /* fall through */
        case meta('r'): /* right */
            if (str[cp]) {
                ++cp;
                if (cp >= w)
                    cp = w - 1;
            }
            continue;
        case meta('l'): /* left */
            if (--cp < 0)
                cp = 0;
            continue;
        case cntrl(']'): /* redraw screen */
            VRedraw();
            continue;
        case cntrl('C'):
        case cntrl('['):
        case meta('J'): /* f0 */
            return (0);
        case cntrl('M'):
        case cntrl('J'):
            return (str);
        case cntrl('I'):
            if (str[cp])
                while (str[++cp])
                    ;
            else
                cp = 0;
            continue;
        case meta('h'): /* home */
            cp = 0;
            continue;
        case meta('e'): /* end */
            while (str[cp])
                ++cp;
            continue;
        case meta('b'): /* back space */
            if (cp) {
                for (k = cp--; str[k]; ++k)
                    str[k - 1] = str[k];
                str[k - 1] = 0;
            }
            continue;
        case cntrl('G'): /* delete */
            if (!str[cp])
                continue;
            for (k = cp + 1; str[k]; ++k)
                str[k - 1] = str[k];
            str[k - 1] = 0;
            continue;
        case cntrl('Y'): /* clear line */
            str[cp = 0] = 0;
            continue;
        }
    }
}

static char *getstring1(int w, char *str, char *head, char *mesg);

/* in the dialog colours */
char *getstring(int w, char *str, char *head, char *mesg)
{
    int oldc = VSetColor(VCOL_DIALOG);
    char *r  = getstring1(w, str, head, mesg);

    VSetColor(oldc);
    return r;
}

static char *getstring1(int w, char *str, char *head, char *mesg)
{
    register int r, c, h;
    int len;
    BOX *box;
    static char buf[81];

    len = strlen(mesg);
    if (len > w)
        w = len;
    len = strlen(head);
    if (len > w)
        w = len;
    h = 4;
    w += 4;
    r = LINES / 4;
    c = (COLS - 2 - w) / 2;

    box = VGetBox(r - 1, c - 2, h + 2, w + 4); /* save box */
    VStandOut();
    VSetDim();
    VFillBox(r - 1, c - 2, h + 2, w + 4, ' ');        /* clear */
    VDrawBox(r, c, h, w);                             /* draw margins */
    VMPutString(r, c + (w - strlen(head)) / 2, head); /* head */
    VMPutString(r + 1, c + 2, mesg);                  /* message */
    VStandEnd();
    VSetNormal();

    strncpy(buf, str ? str : "", 80);

    str = editstring(r + 2, c + 2, w - 4, buf, 0);
    VUngetBox(box);
    VFreeBox(box);
    return (str);
}

static char *getwstring1(int w, char *str, char *head, char *mesg);

/* in the dialog colours */
char *getwstring(int w, char *str, char *head, char *mesg)
{
    int oldc = VSetColor(VCOL_DIALOG);
    char *r  = getwstring1(w, str, head, mesg);

    VSetColor(oldc);
    return r;
}

static char *getwstring1(int w, char *str, char *head, char *mesg)
{
    register int r, c, h;
    int len;
    BOX *box;
    static char buf[81];

    len = strlen(mesg);
    if (len > w)
        w = len;
    len = strlen(head);
    if (len > w)
        w = len;
    h = 4;
    w += 4;
    r = LINES / 4;
    c = BASECOL(cur) + (PAGEWID - w) / 2;
    if (c < 3)
        c = 3;
    else if (c > COLS - 4 - w)
        c = COLS - 4 - w;

    box = VGetBox(r - 1, c - 2, h + 2, w + 4); /* save box */
    VStandOut();
    VSetDim();
    VFillBox(r - 1, c - 2, h + 2, w + 4, ' ');        /* clear */
    VDrawBox(r, c, h, w);                             /* draw margins */
    VMPutString(r, c + (w - strlen(head)) / 2, head); /* head */
    VMPutString(r + 1, c + 2, mesg);                  /* message */
    VStandEnd();
    VSetNormal();

    strncpy(buf, str ? str : "", 80);

    str = editstring(r + 2, c + 2, w - 4, buf, 1);
    VUngetBox(box);
    VFreeBox(box);
    return (str);
}

/*
 * A list to choose one of: the index, or -1 for Esc. Up, Down, Home,
 * End, Enter; the first letter of an item jumps to it.
 */
int getlist(char *head, char **items, int n, int sel)
{
    int w = strlen(head), r, c, i, key, oldc;
    BOX *box, *curbox;

    for (i = 0; i < n; ++i)
        if ((int)strlen(items[i]) > w)
            w = strlen(items[i]);
    w += 4;
    r = (LINES - n) / 2;
    c = (COLS - w) / 2;
    if (sel < 0 || sel >= n)
        sel = 0;

    oldc = VSetColor(VCOL_LIST);
    box  = VGetBox(r - 2, c - 4, n + 4, w + 8);
    VSetDim();
    VStandOut();
    VFillBox(r - 2, c - 4, n + 4, w + 8, ' ');
    VDrawBox(r - 1, c - 1, n + 2, w + 2);
    VMPutString(r - 1, c + (w - (int)strlen(head)) / 2, head);
    for (i = 0; i < n; ++i)
        VMPutString(r + i, c + 2, items[i]);
    VSetNormal();
    VStandEnd();
    for (;;) {
        curbox = VGetBox(r + sel, c, 1, w);
        VPrintBox(curbox);
        hidecursor();
        VSync();
        key = KeyGet();
        VUngetBox(curbox);
        VFreeBox(curbox);
        switch (key) {
        case cntrl(']'):
            VRedraw();
            continue;
        case cntrl('M'):
        case cntrl('J'):
            break;
        case cntrl('C'):
        case cntrl('['):
        case meta('J'): /* F10 */
            sel = -1;
            break;
        case meta('u'):
            if (--sel < 0)
                sel = n - 1;
            continue;
        case meta('d'):
            if (++sel >= n)
                sel = 0;
            continue;
        case meta('h'):
            sel = 0;
            continue;
        case meta('e'):
            sel = n - 1;
            continue;
        default:
            for (i = 1; i <= n; ++i)
                if (key > ' ' && key < 0177 &&
                    ((unsigned char)items[(sel + i) % n][0] | 040) == (key | 040)) {
                    sel = (sel + i) % n;
                    break;
                }
            if (i > n)
                VBeep();
            continue;
        }
        break;
    }
    VUngetBox(box);
    VFreeBox(box);
    VSetColor(oldc);
    return sel;
}
