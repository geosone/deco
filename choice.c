#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define EDITSZ 324 /* 80 cells of UTF-8 */

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
    w                      = VStrLen(mesg);
    if (mesg2) {
        len = VStrLen(mesg2);
        if (len > w)
            w = len;
    }
    len = VStrLen(head);
    if (len > w)
        w = len;
    len = 0;
    if (c1)
        len += VStrLen(c1);
    if (c2)
        len += VStrLen(c2);
    if (c3)
        len += VStrLen(c3);
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
    VFillBox(r - 1, c - 2, h + 2, w + 4, ' ');         /* clear */
    VDrawBox(r, c, h, w);                              /* draw margins */
    VMPutString(r, c + (w - VStrLen(head)) / 2, head); /* head */
    if (mesg2) {
        VMPutString(r + 1, c + (w - VStrLen(mesg)) / 2, mesg);
        VMPutString(r + 2, c + (w - VStrLen(mesg2)) / 2, mesg2);
    } else
        VMPutString(r + 2, c + (w - VStrLen(mesg)) / 2, mesg);

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
        w += tab[i].namlen = VStrLen(tab[i].name);
        tab[i].r           = row;
    }
    tab[0].c = col - w / 2;
    tab[1].c = tab[0].c + tab[0].namlen + 1;
    tab[2].c = tab[1].c + tab[1].namlen + 1;
}

static int menuchoice()
{
    int ch = 0, i;

    for (;;) {
        drawchoice(ch);
        hidecursor();
        VSync();
        switch (KeyGet()) {
        default:
            VBeep();
            continue;
        case MOUSEKEY: /* a click on a button chooses it */
            for (i = 0; i < cnum; ++i)
                if (MouseRow == tab[i].r && MouseCol >= tab[i].c &&
                    MouseCol < tab[i].c + tab[i].namlen)
                    return (i);
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
                VMPutChar(tab[i].r, tab[i].c + VStrLen(tab[i].name) - 1, ']');
            }
            if (attroff)
                (*attroff)();
        } else
            VMPutString(tab[i].r, tab[i].c, tab[i].name);
    }
}

/* the bytes of the character at s (UTF-8), at least 1 */
static int chlen(const char *s)
{
    int n = utf8len(s, s + strlen(s));

    return n ? n : 1;
}

/* the start of the character before p (UTF-8) */
static int chprev(const char *s, int p)
{
    if (p > 0)
        --p;
    while (p > 0 && UTF8CONT(s[p]))
        --p;
    return p;
}

/* a line of w cells to edit, str (EDITSZ bytes) in UTF-8; cp: the cursor
 * at the end, else the first key replaces the text */
static char *editstring(int r, int c, int w, char *str, int cp)
{
    register int key, k, n;
    int firstkey = 1;
    char save;

    if (cp) {
        cp       = strlen(str);
        firstkey = 0;
    }
    for (;; firstkey = 0) {
        VClearBox(r, c, 1, w);
        VMove(r, c);
        VPutStringN(str, w);
        save    = str[cp];
        str[cp] = 0;
        VMove(r, c + VStrLen(str));
        str[cp] = save;
        VSync();
        switch (key = KeyGet()) {
        default:
            /* a byte of UTF-8 goes in like a character; whole
             * characters are kept to w cells */
            if (key < ' ' || key == 0177 || key > 0377) {
                VBeep();
                continue;
            }
            if (firstkey)
                str[cp = 0] = 0;
            n = strlen(str);
            if (n >= EDITSZ - 1 || (!UTF8CONT(key) && VStrLen(str) >= w)) {
                VBeep();
                continue;
            }
            memmove(str + cp + 1, str + cp, n - cp + 1);
            str[cp++] = key;
            continue;
        case meta('r'): /* right */
            if (str[cp])
                cp += chlen(str + cp);
            continue;
        case meta('l'): /* left */
            cp = chprev(str, cp);
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
            cp = str[cp] ? (int)strlen(str) : 0;
            continue;
        case meta('h'): /* home */
            cp = 0;
            continue;
        case meta('e'): /* end */
            cp = strlen(str);
            continue;
        case meta('b'): /* back space */
            if (cp) {
                k = chprev(str, cp);
                memmove(str + k, str + cp, strlen(str + cp) + 1);
                cp = k;
            }
            continue;
        case cntrl('G'): /* delete */
            if (str[cp])
                memmove(str + cp, str + cp + chlen(str + cp),
                        strlen(str + cp + chlen(str + cp)) + 1);
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
    static char buf[EDITSZ];

    len = VStrLen(mesg);
    if (len > w)
        w = len;
    len = VStrLen(head);
    if (len > w)
        w = len;
    h = 4;
    w += 4;
    r = LINES / 4;
    c = (COLS - 2 - w) / 2;

    box = VGetBox(r - 1, c - 2, h + 2, w + 4); /* save box */
    VStandOut();
    VSetDim();
    VFillBox(r - 1, c - 2, h + 2, w + 4, ' ');         /* clear */
    VDrawBox(r, c, h, w);                              /* draw margins */
    VMPutString(r, c + (w - VStrLen(head)) / 2, head); /* head */
    VMPutString(r + 1, c + 2, mesg);                   /* message */
    VStandEnd();
    VSetNormal();

    snprintf(buf, sizeof(buf), "%s", str ? str : "");

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
    static char buf[EDITSZ];

    len = VStrLen(mesg);
    if (len > w)
        w = len;
    len = VStrLen(head);
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
    VFillBox(r - 1, c - 2, h + 2, w + 4, ' ');         /* clear */
    VDrawBox(r, c, h, w);                              /* draw margins */
    VMPutString(r, c + (w - VStrLen(head)) / 2, head); /* head */
    VMPutString(r + 1, c + 2, mesg);                   /* message */
    VStandEnd();
    VSetNormal();

    snprintf(buf, sizeof(buf), "%s", str ? str : "");

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
    return getlistat(head, items, n, sel, COLS / 2);
}

/* the list centred on column mid */
int getlistat(char *head, char **items, int n, int sel, int mid)
{
    int w = VStrLen(head), r, c, i, key, oldc;
    BOX *box, *curbox;

    for (i = 0; i < n; ++i)
        if ((int)VStrLen(items[i]) > w)
            w = VStrLen(items[i]);
    w += 4;
    r = (LINES - n) / 2;
    c = mid - w / 2;
    if (c + w + 4 > COLS)
        c = COLS - w - 4;
    if (c < 4)
        c = 4;
    if (sel < 0 || sel >= n)
        sel = 0;

    oldc = VSetColor(VCOL_LIST);
    box  = VGetBox(r - 2, c - 4, n + 4, w + 8);
    VSetDim();
    VStandOut();
    VFillBox(r - 2, c - 4, n + 4, w + 8, ' ');
    VDrawBox(r - 1, c - 1, n + 2, w + 2);
    VMPutString(r - 1, c + (w - (int)VStrLen(head)) / 2, head);
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
        case MOUSEKEY:
            /* an item chosen; outside the box: none */
            if (MouseRow >= r && MouseRow < r + n && MouseCol >= c - 1 && MouseCol <= c + w) {
                sel = MouseRow - r;
                break;
            }
            if (MouseRow < r - 2 || MouseRow >= r + n + 2 || MouseCol < c - 4 ||
                MouseCol >= c + w + 4) {
                sel = -1;
                break;
            }
            continue;
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
