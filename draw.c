#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "deco.h"
#include "dir.h"
#include "scr.h"

#define DECOVERSION "Version 3.10"

static BOX *cbox;        /* box under cursor */
static struct dir *cdir; /* drawed directory */

static void draw1dir(struct dir *c);
static void drawframe(int c, int view, int stawin);
static void drawcwd(int col, char *wd);
static void drawpat(int col, char *pat, int width);
static void drawsdir(struct dir *c);
static void drawstatus(int col, struct file *f, int view);
static void drawpage(int col, struct file *c, int n, struct dir *d, int w);
static void drawfull(struct file *f, int tail, int nw);
static void drawkeys(void);
static void drawmode(char *m, int u, int g);
static void drawtime(struct file *f);
static void drawlong(struct file *f);
static char *filnam(const char *p, int tail, int n);

void draw()
{
    if (visualwin) {
        drawbanners();
        draw1dir(&left);
        draw1dir(&right);
    } else {
        VClearBox(1, 0, LINES - 2, 80);
        drawbanners();
    }
}

/*
 * Panel geometry. A brief column takes 19 cells (18 and the line), a long
 * or wide one 38; a panel has as many as fit, so wider screens show more
 * names. Full and info views have one column (in the wide window; in a
 * half panel they are drawn as long).
 */
static int colwidth(int view)
{
    switch (view) {
    case VIEW_BRIEF:
        return 19;
    case VIEW_INFO:
    case VIEW_FULL:
        if (widewin)
            return PAGEWID - 1;
        /* Fall through... */
    default:
        return 38;
    }
}

int ncolumns(int view)
{
    int n = (PAGEWID - 1) / colwidth(view);

    return n < 1 ? 1 : n;
}

int colsep(int k, int view)
{
    return k * (PAGEWID - 1) / ncolumns(view);
}

/* the width of a file name in a column w cells wide (14 in the classic
 * 80-column layout) */
static int namewidth(int view, int w)
{
    int n;

    switch (view) {
    case VIEW_BRIEF:
    case VIEW_WIDE:
        n = w - 4;
        break;
    case VIEW_INFO:
    case VIEW_FULL:
        if (widewin) {
            n = 14 + w - 75;
            break;
        }
        /* Fall through... */
    default:
        n = w - 23;
        break;
    }
    return n < 8 ? 8 : n > 200 ? 200 : n;
}

void drawdir(struct dir *c, int statusflag) /* redraw window */
{
    draw1dir(c);
    if (statusflag) {
        c = (c == &left ? &right : &left);
        if (c->status)
            draw1dir(c);
    }
}

void drawbanners()
{
    register int i;
    register char *curtime;

    if (visualwin) {
        if (widewin) {
            drawframe(BASECOL(cur), cur->view, cur->status);
        } else {
            drawframe(left.basecol, left.view, left.status);
            drawframe(right.basecol, right.view, right.status);
        }
    }
    VSetDim();
    VStandOut();
    VMove(0, 0);
    for (i = COLS; --i >= 0; VPutChar(' '))
        ;
    VMove(0, 1);
    if (machine) {
        VPutString(machine);
        VPutChar(':');
    }
    if (tty)
        VPutString(tty);
    if (user)
        VMPutString(0, COLS / 2 - 1 - strlen(user), user);
    VPutChar('.');
    if (group)
        VPutString(group);
    if ((curtime = timestr(time(NULL))))
        VMPutString(0, COLS - 1 - strlen(curtime), curtime);
    VStandEnd();
    drawkeys();
    VSetNormal();
}

/* the function keys at the bottom, spread over the width of the screen */
static void drawkeys()
{
    static const char *const keys[2][10] = {
        { "Help", "User", "View", "Edit", "Copy", "RenMov", "Mkdir", "Delete", "Menu", "Quit" },
        { "Help", "User", "", "", "", "", "", "", "Menu", "Quit" },
    };
    char buf[1024], *p = buf;
    int i, slot        = COLS / 10, w;

    for (i = 0; i < 10; ++i) {
        /* each takes a tenth: the number, the label, a space before the
         * next; the last one takes what is left */
        w = (i == 9 ? COLS - 9 * slot : slot) - (i == 9 ? 2 : 1) - (i ? 1 : 0);
        if (w < 1)
            w = 1;
        p += sprintf(p, "%s%d\16%-*.*s\17", i ? " " : "", i + 1, w, w, keys[cmdreg != 0][i]);
    }
    VMPutString(LINES - 1, 0, buf);
}

static void draw1dir(struct dir *c) /* redraw window */
{
    register int len = c->num - c->topfile;
    int k, n, x, w;

    /* check visibility in full window mode */
    if (!visualwin || (widewin && c != cur))
        return;
    cdir = c;

    /* if it is status window - draw status of another one */
    if (c->status) {
        drawsdir(c);
        return;
    }

    /* reprint dir name */
    VSetDim();
    VHorLine(1, BASECOL(c) + 1, PAGEWID - 2);
    VHorLine(H + 2, BASECOL(c) + 1, PAGEWID - 2);
    VHorLine(H + 4, BASECOL(c) + 1, PAGEWID - 2);
    n = ncolumns(c->view);
    for (k = 1; k < n; ++k) {
        if (widewin)
            VCorner(1, colsep(k, c->view), 2);
        VCorner(H + 2, BASECOL(c) + colsep(k, c->view), 8);
    }
    drawcwd(c->basecol, c->shortcwd);
    if (c->pattern[0])
        drawpat(BASECOL(c), c->pattern, PAGEWID);
    else {
        char buf[80];

        sprintf(buf, " %s byte%s in %d file%s ", ltoac(c->nbytes), c->nbytes == 1 ? "" : "s",
                c->nfiles, c->nfiles == 1 ? "" : "s");
        mvcaddstr(H + 4, BASECOL(c) + PAGEWID / 2 + 1, buf);
        if (c->tfiles) {
            sprintf(buf, " %s byte%s in %d file%s ", ltoac(c->tbytes), c->tbytes == 1 ? "" : "s",
                    c->tfiles, c->tfiles == 1 ? "" : "s");
            VSetBold();
            mvcaddstr(H + 2, BASECOL(c) + PAGEWID / 2 + 1, buf);
        }
    }
    VSetNormal();

    for (k = 0; k < n; ++k, len -= H) {
        x = BASECOL(c) + colsep(k, c->view) + 1;
        w = colsep(k + 1, c->view) - colsep(k, c->view) - 1;
        VClearBox(2, x, H, w);
        if (len > 0)
            drawpage(x, &c->cat[c->topfile + k * H], len, c, w);
    }
    drawstatus(BASECOL(c), &c->cat[c->curfile], c->view);
}

void drawcursor()
{
    register int col, line, w, k;

    cdir = cur;
    if (cur->status) {
        VStandOut();
        drawcwd(cur->basecol, " Status ");
        VStandEnd();
        return;
    }
    line = cur->curfile - cur->topfile;
    k    = line / H;
    line %= H;
    col = BASECOL(cur) + colsep(k, cur->view) + 1;
    w   = colsep(k + 1, cur->view) - colsep(k, cur->view) - 1;
    line += 2;
    if (cbox)
        VFreeBox(cbox);
    cbox = VGetBox(line, col, 1, w);
    if (VStandOut())
        if (cur->cat[cur->curfile].tag) {
            VSetBold();
            VPrintBox(cbox);
            VSetNormal();
        } else
            VPrintBox(cbox);
    else {
        VMPutChar(line, col, '[');
        VMPutChar(line, col + w - 1, ']');
    }
    drawcwd(cur->basecol, cur->shortcwd);
    VStandEnd();
    drawstatus(BASECOL(cur), &cur->cat[cur->curfile], cur->view);
}

void undrawcursor()
{
    if (cbox) {
        VUngetBox(cbox);
        cbox = 0;
    }
    VSetDim();
    drawcwd(cur->basecol, cur->status ? " Status " : cur->shortcwd);
    VSetNormal();
}

/*
 * Draw frame of (one) window
 */
static void drawframe(int c, int view, int stawin)
{
    VSetDim();
    if (stawin) {
        VHorLine(4, c + 1, PAGEWID - 2);
        VHorLine(11, c + 1, PAGEWID - 2);
        VVertLine(c, 2, H + 2);
        VVertLine(c + PAGEWID - 1, 2, H + 2);
        VCorner(1, c, 1);
        VCorner(1, c + PAGEWID - 1, 3);
        VCorner(4, c, 4);
        VCorner(4, c + PAGEWID - 1, 6);
        VCorner(11, c, 4);
        VCorner(11, c + PAGEWID - 1, 6);
        VCorner(H + 4, c, 7);
        VCorner(H + 4, c + PAGEWID - 1, 9);
    } else {
        int k, n = ncolumns(view);

        if (widewin)
            c = 0;
        VHorLine(H + 2, c + 1, PAGEWID - 2);
        VVertLine(c, 2, H + 2);
        VVertLine(c + PAGEWID - 1, 2, H + 2);
        for (k = 1; k < n; ++k) {
            VVertLine(c + colsep(k, view), 2, H);
            VCorner(H + 2, c + colsep(k, view), 8);
        }
        VCorner(1, c, 1);
        VCorner(1, c + PAGEWID - 1, 3);
        VCorner(H + 2, c, 4);
        VCorner(H + 2, c + PAGEWID - 1, 6);
        VCorner(H + 4, c, 7);
        VCorner(H + 4, c + PAGEWID - 1, 9);
    }
    VSetNormal();
}

static void drawcwd(int col, char *wd)
{
    register int len, tw;

    if (!visualwin)
        return;
    /* over the panel; in the wide window over its half */
    tw  = widewin ? (PAGEWID + 1) / 2 : PAGEWID;
    len = strlen(wd);
    if (widewin && col)
        col = PAGEWID - tw;
    VMPutString(1, col + (tw - len) / 2, wd);
}

static void drawpat(int col, char *pat, int width)
{
    col += width - strlen(pat) - 1;
    VMove(H + 4, col);
    VPutString(pat);
}

static void drawsdir(struct dir *c) /* draw status window */
{
    struct dir *d = c == &left ? &right : &left;
    char buf[80];

    /* print window name " Status " */
    VSetDim();
    VHorLine(1, BASECOL(c) + 1, PAGEWID - 2);
    drawcwd(c->basecol, " Status ");
    VHorLine(H + 4, BASECOL(c) + 1, PAGEWID - 2);
    VSetNormal();
    VClearBox(2, BASECOL(c) + 1, 2, PAGEWID - 2);
    VClearBox(5, BASECOL(c) + 1, 6, PAGEWID - 2);
    VClearBox(12, BASECOL(c) + 1, H - 8, PAGEWID - 2);
    sprintf(buf, "The Demos Commander, %s", DECOVERSION);
    mvcaddstr(2, BASECOL(c) + PAGEWID / 2 + 1, buf);
    mvcaddstr(3, BASECOL(c) + PAGEWID / 2 + 1, "Copyright (C) 1989-2000 S.Vakulenko");
    mvcaddstr(5, BASECOL(c) + PAGEWID / 2 + 1, "Directory");
    mvcaddstr(6, BASECOL(c) + PAGEWID / 2 + 1, d->shortcwd);
    sprintf(buf, "\3%d\2 file%s", d->nfiles, d->nfiles == 1 ? "" : "s");
    mvcaddstr(7, BASECOL(c) + PAGEWID / 2 + 2, buf);
    sprintf(buf, "\3%s\2 byte%s", ltoac(d->nbytes), d->nbytes == 1 ? "" : "s");
    mvcaddstr(8, BASECOL(c) + PAGEWID / 2 + 2, buf);
    if (d->pattern[0]) {
        sprintf(buf, "\3%s\2 byte%s in \3%d\2 matched file%s", ltoac(d->mbytes),
                d->mbytes == 1 ? "" : "s", d->mfiles, d->mfiles == 1 ? "" : "s");
        mvcaddstr(9, BASECOL(c) + PAGEWID / 2 + 3, buf);
    }
    if (d->tfiles) {
        sprintf(buf, "\3%s\2 byte%s in \3%d\2 selected file%s", ltoac(d->tbytes),
                d->tbytes == 1 ? "" : "s", d->tfiles, d->tfiles == 1 ? "" : "s");
        mvcaddstr(10, BASECOL(c) + PAGEWID / 2 + 3, buf);
    }
#ifdef notyet
    mvcaddstr(13, BASECOL(c) + PAGEWID / 2 + 1, "No .dirinfo file in this directory");
#endif
}

static void drawstatus(int col, struct file *f, int view)
{
    int nw = 14 + PAGEWID - (widewin ? 77 : 39);

    /* draw current file status box */
    VMove(H + 3, col + 2);
    VSetDim();
    VPutChar(filetype(f));
    VSetNormal();
    VPutChar(' ');
    if (widewin)
        switch (view) {
        case VIEW_FULL:
            VPutString(filnam(f->name, 1, nw));
            drawtime(f);
            break;
        case VIEW_INFO:
        case VIEW_LONG:
        case VIEW_WIDE:
        case VIEW_BRIEF:
            drawfull(f, 1, nw);
            drawlong(f);
            break;
        }
    else
        drawfull(f, 1, nw);
}

/*
 * Redraw file window
 */
static void drawpage(int col, struct file *c, int n, struct dir *d, int w)
{
    register int i;
    char *info;
    int nw = namewidth(d->view, w);

    if (n > H)
        n = H;
    ++col;
    for (i = 0; i < n; ++i, ++c) {
        VSetDim();
        VMPutChar(i + 2, col, filetype(c));
        if (c->tag) {
            VPutChar('>');
            VSetBold();
        } else if (c->link) {
            VPutChar('@');
            VSetNormal();
        } else {
            VPutChar(' ');
            VSetNormal();
        }
        switch (d->view) {
        case VIEW_INFO:
            if (!widewin)
                goto draw_long;
            VPutString(filnam(c->name, 0, nw)); /* 14 */
            VPutChar(' ');                      /* 1 */
            VPutChar(' ');                      /* 1 */
            if (!c->infoflag)
                setinfo(c, d);
            if (!c->dateflag)
                VSetDim();
            VPutString(filedate(c->infodate)); /* 8 */
            if (!c->dateflag)
                VSetPrev();
            if (c->rinfo && (lang == RUS || lang == UKR))
                info = c->rinfo;
            else
                info = c->info;
            if (info) {
                VPutChar(' ');    /* 1 */
                VPutChar(' ');    /* 1 */
                VPutString(info); /* 48 */
            }
            break;
        case VIEW_FULL:
            drawfull(c, 0, nw);
            if (widewin)
                drawlong(c);
            break;
        case VIEW_LONG:
        draw_long:
            drawfull(c, 0, nw);
            break;
        case VIEW_WIDE:
            VPutString(filnam(c->name, 0, nw));
            break;
        case VIEW_BRIEF:
            VPutString(filnam(c->name, 0, nw));
            break;
        }
        if (c->tag)
            VSetNormal();
    }
}

static void drawfull(struct file *f, int tail, int nw)
{
    VPutString(filnam(f->name, tail, nw)); /* 14 */
    VPutString(filesize(f));               /* 8 */
    VPutChar(' ');                         /* 1 */
    VPutChar(' ');                         /* 1 */
    drawmode(filemode((int)f->mode), (int)f->uid, (int)f->gid);
}

static void drawmode(char *m, int file_uid, int file_gid)
{
    int u, g;

    u = (file_uid == uid);
    g = mygroup(file_gid);
    if (!u)
        VSetDim();
    VPutChar(*m++);
    VPutChar(*m++);
    VPutChar(*m++);
    if (u)
        VSetDim();
    else if (g)
        VSetPrev();
    VPutChar(*m++);
    VPutChar(*m++);
    VPutChar(*m++);
    if (!u) {
        if (g) {
            VSetDim();
        } else {
            VSetPrev();
        }
    }
    VPutChar(*m++);
    VPutChar(*m++);
    VPutChar(*m++);
    if (u || g)
        VSetPrev();
}

static void drawtime(struct file *f)
{
    VPrint("%4d/%-5d", f->dev, f->ino);
    if (f->nlink > 1)
        VPrint(" %-3d ", f->nlink);
    else
        VPutString("     ");
    VPutString(timestr(f->atime));
    VPutChar(' ');
    VPutChar(' ');
    VPutString(timestr(f->ctime));
}

static void drawlong(struct file *f)
{
    VPrint(" %8.8s.%-8.8s", username((int)f->uid), groupname((int)f->gid));
    VPutString(timestr(f->mtime));
}

/* p in a field of n cells: cut with '~', the extension aligned to the
 * end if the panel wants that */
static char *filnam(const char *p, int tail, int n)
{
    static char buf[256];
    register char *s;
    const char *e, *q;

    if ((int)strlen(p) > n) {
        if (tail) {
            buf[0] = '~';
            strcpy(buf + 1, p + strlen(p) - (n - 1));
        } else {
            strncpy(buf, p, n - 1);
            buf[n - 1] = '~';
            buf[n]     = 0;
        }
        return (buf);
    }
    for (s = buf; s < buf + n; *s++ = ' ')
        ; /* clear buf */
    if (cdir->alignext) {
        e = extension(p); /* make ptr to extension */
        for (s = buf; p < e; *s++ = *p++)
            ; /* copy file name */
        for (q = e; *q; ++q)
            ; /* search extension end */
        for (s = buf + n - 1, --q; q >= e; *s-- = *q--)
            ; /* copy extension */
    } else {
        e = p + n; /* make ptr to end */
        for (s = buf; *p && p < e; *s++ = *p++)
            ; /* copy file name */
    }
    buf[n] = 0;
    return (buf);
}
