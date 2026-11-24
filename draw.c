#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "deco.h"
#include "dir.h"
#include "scr.h"

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
static void drawmode(char *m, int u, int g);
static void drawtime(struct file *f);
static void drawlong(struct file *f);
static char *filnam(const char *p, int tail, int n);
static void bytesinfiles(char *buf, size_t size, const char *fmt, long bytes, int files);

/*
 * The colours of each role (scr.h), VC-like: fg and bg of normal,
 * reverse, bold, bold reverse, dim and dim reverse. VGA colour numbers:
 * 0 black, 1 blue, 3 cyan, 4 red, 7 light gray, 8 gray, 11 bright cyan,
 * 14 yellow, 15 white.
 */
static const int vccolors[VCOL_NROLES][12] = {
    [VCOL_PANEL] = {
        11, 1, /* names */
        0, 3,  /* cursor */
        14, 1, /* tagged names */
        14, 3, /* cursor on a tagged name */
        3, 1,  /* frames, file types, details */
        0, 3,  /* the current panel's directory */
    },
    [VCOL_KEYS] = {
        7, 0, 0, 3, 15, 0, 15, 3,
        15, 0, /* the key numbers */
        0, 3,  /* the labels */
    },
    [VCOL_BAR] = {
        0, 3, 0, 3,
        15, 0, /* the chosen menu */
        15, 0,
        0, 3, /* the top line, the menu bar */
        0, 3,
    },
    [VCOL_MENU] = {
        0, 3,  /* items, the frame */
        15, 0, /* the chosen item */
        14, 3, /* hot keys */
        14, 0, /* the hot key of the chosen item */
        8, 3, /* items not possible now */
        0, 3, /* pop-up lists */
    },
    [VCOL_DIALOG] = {
        0, 3,  /* the input line */
        0, 7,
        0, 7,  /* the chosen button of an error */
        15, 4, /* errors */
        0, 3,  /* the chosen button */
        0, 7,  /* dialogs */
    },
    [VCOL_LIST] = {
        15, 0, /* the chosen line */
        0, 3, 15, 0, 15, 3, 0, 3,
        0, 3, /* the list */
    },
    [VCOL_VIEW] = {
        11, 1, /* text */
        0, 3,  /* found text */
        15, 1, /* bold */
        15, 3,
        3, 1, /* control characters */
        0, 3, /* the head line */
    },
};

void initcolors()
{
    int i;

    for (i = VCOL_DEFAULT + 1; i < VCOL_NROLES; ++i)
        VSetRoleColors(i, vccolors[i]);
}

void draw()
{
    if (visualwin) {
        drawbanners();
        draw1dir(&left);
        draw1dir(&right);
    } else {
        VClearBox(1, 0, LINES - 2, COLS);
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
    int oldc = VSetColor(VCOL_PANEL);

    draw1dir(c);
    if (statusflag) {
        c = (c == &left ? &right : &left);
        if (c->status)
            draw1dir(c);
    }
    VSetColor(oldc);
}

void drawbanners()
{
    register int i;
    register char *curtime;
    int oldc = VSetColor(VCOL_PANEL);

    if (visualwin) {
        if (widewin) {
            drawframe(BASECOL(cur), cur->view, cur->status);
        } else {
            drawframe(left.basecol, left.view, left.status);
            drawframe(right.basecol, right.view, right.status);
        }
    }
    VSetColor(VCOL_BAR);
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
        VMPutString(0, COLS / 2 - 1 - VStrLen(user), user);
    VPutChar('.');
    if (group)
        VPutString(group);
    if ((curtime = timestr(time(NULL))))
        VMPutString(0, COLS - 1 - VStrLen(curtime), curtime);
    VStandEnd();
    static const char *const keys[2][10] = {
        { NC_("keybar", "Help"), NC_("keybar", "User"), NC_("keybar", "View"),
          NC_("keybar", "Edit"), NC_("keybar", "Copy"), NC_("keybar", "RenMov"),
          NC_("keybar", "Mkdir"), NC_("keybar", "Delete"), NC_("keybar", "Menu"),
          NC_("keybar", "Quit") },
        { NC_("keybar", "Help"), NC_("keybar", "User"), "", "", "", "", "", "",
          NC_("keybar", "Menu"), NC_("keybar", "Quit") },
    };

    drawkeys(keys[cmdreg != 0]);
    VSetColor(oldc);
    VSetNormal();
}

/* the function keys F1...F10 at the bottom, spread over the width of
 * the screen; the panels, the viewer and the editor each have their own */
void drawkeys(const char *const labels[10])
{
    char buf[1024], *p = buf;
    const char *l, *e;
    int i, w, n;
    int slot = COLS / 10;
    int oldc = VSetColor(VCOL_KEYS);

    *p++ = '\1';
    for (i = 0; i < 10; ++i) {
        /* each takes a tenth: the number, the label, a space before the
         * next; the last one takes what is left */
        w = (i == 9 ? COLS - 9 * slot : slot) - (i == 9 ? 2 : 1) - (i ? 1 : 0);
        if (w < 1)
            w = 1;
        /* the label in w cells (UTF-8), cut or filled */
        l = trc("keybar", labels[i]);
        e = VStrSkip(l, w);
        n = w - VStrLen(l);
        p += sprintf(p, "%s%d\16%.*s%*s\17", i ? " " : "", i + 1, (int)(e - l), l, n > 0 ? n : 0, "");
    }
    *p++ = '\2';
    *p   = 0;
    VMPutString(LINES - 1, 0, buf);
    VSetColor(oldc);
}

static void draw1dir(struct dir *c) /* redraw window */
{
    register int len = c->num - c->topfile;
    int k, n, x, w;

    /* check visibility in full window mode */
    if (!visualwin || (widewin && c != cur))
        return;
    cdir = c;
    VSetColor(VCOL_PANEL);

    /* if it is status window - draw status of another one */
    if (c->status) {
        drawsdir(c);
        return;
    }

    /* reprint dir name */
    VSetDim();
    VLine(1, BASECOL(c) + 1, PAGEWID - 2, 0, LINE_DOUBLE);
    VLine(H + 2, BASECOL(c) + 1, PAGEWID - 2, 0, LINE_SINGLE);
    VLine(H + 4, BASECOL(c) + 1, PAGEWID - 2, 0, LINE_DOUBLE);
    n = ncolumns(c->view);
    for (k = 1; k < n; ++k) { /* the columns meet the frame and the status line */
        VJoint(1, BASECOL(c) + colsep(k, c->view), 0, LINE_SINGLE, LINE_DOUBLE, LINE_DOUBLE);
        VJoint(H + 2, BASECOL(c) + colsep(k, c->view), LINE_SINGLE, 0, LINE_SINGLE, LINE_SINGLE);
    }
    drawcwd(c->basecol, c->shortcwd);
    if (c->pattern[0])
        drawpat(BASECOL(c), c->pattern, PAGEWID);
    else {
        char buf[160];

        bytesinfiles(buf, sizeof(buf), _(" %s in %s "), c->nbytes, c->nfiles);
        mvcaddstr(H + 4, BASECOL(c) + PAGEWID / 2 + 1, buf);
        if (c->tfiles) {
            bytesinfiles(buf, sizeof(buf), _(" %s in %s "), c->tbytes, c->tfiles);
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
    int oldc = VSetColor(VCOL_PANEL);

    cdir = cur;
    if (cur->status) {
        VStandOut();
        drawcwd(cur->basecol, " Status ");
        VStandEnd();
        VSetColor(oldc);
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
    VSetColor(oldc);
}

void undrawcursor()
{
    int oldc = VSetColor(VCOL_PANEL);

    if (cbox) {
        VUngetBox(cbox);
        cbox = 0;
    }
    VSetDim();
    drawcwd(cur->basecol, cur->status ? " Status " : cur->shortcwd);
    VSetNormal();
    VSetColor(oldc);
}

/*
 * Draw frame of (one) window
 */
static void drawframe(int c, int view, int stawin)
{
    /* the whole panel in its colours, then the lines */
    VClearBox(1, widewin ? 0 : c, H + 4, PAGEWID);
    /* as VC: the frame double, the lines inside single */
    const int D = LINE_DOUBLE, S = LINE_SINGLE, r = c + PAGEWID - 1;

    VSetDim();
    if (stawin) {
        VFrame(1, c, H + 4, PAGEWID, D);
        VLine(4, c + 1, PAGEWID - 2, 0, S);
        VLine(11, c + 1, PAGEWID - 2, 0, S);
        VJoint(4, c, D, D, 0, S);
        VJoint(4, r, D, D, S, 0);
        VJoint(11, c, D, D, 0, S);
        VJoint(11, r, D, D, S, 0);
    } else {
        int k, n = ncolumns(view), x = widewin ? 0 : c;

        VFrame(1, x, H + 4, PAGEWID, D);
        VLine(H + 2, x + 1, PAGEWID - 2, 0, S);
        for (k = 1; k < n; ++k) {
            VLine(2, x + colsep(k, view), H, 1, S);
            VJoint(1, x + colsep(k, view), 0, S, D, D);
            VJoint(H + 2, x + colsep(k, view), S, 0, S, S);
        }
        VJoint(H + 2, x, D, D, 0, S);
        VJoint(H + 2, x + PAGEWID - 1, D, D, S, 0);
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
    len = VStrLen(wd);
    if (widewin && col)
        col = PAGEWID - tw;
    VMPutString(1, col + (tw - len) / 2, wd);
}

static void drawpat(int col, char *pat, int width)
{
    col += width - VStrLen(pat) - 1;
    VMove(H + 4, col);
    VPutString(pat);
}

/* "<n bytes> in <n files>" with fmt (" %s in %s "), plurals translated */
static void bytesinfiles(char *buf, size_t size, const char *fmt, long bytes, int files)
{
    char b[64], f[64];

    snprintf(b, sizeof(b), P_("%s byte", "%s bytes", bytes), ltoac(bytes));
    snprintf(f, sizeof(f), P_("%d file", "%d files", files), files);
    snprintf(buf, size, fmt, b, f);
    /* on the frame of a panel: no longer than it */
    if (VStrLen(buf) > PAGEWID - 2 && PAGEWID > 2)
        *VStrSkip(buf, PAGEWID - 2) = 0;
}

static void drawsdir(struct dir *c) /* draw status window */
{
    struct dir *d = c == &left ? &right : &left;
    char buf[160];

    /* print window name " Status " */
    VSetDim();
    VLine(1, BASECOL(c) + 1, PAGEWID - 2, 0, LINE_DOUBLE);
    drawcwd(c->basecol, _(" Status "));
    VLine(H + 4, BASECOL(c) + 1, PAGEWID - 2, 0, LINE_DOUBLE);
    VSetNormal();
    VClearBox(2, BASECOL(c) + 1, 2, PAGEWID - 2);
    VClearBox(5, BASECOL(c) + 1, 6, PAGEWID - 2);
    VClearBox(12, BASECOL(c) + 1, H - 8, PAGEWID - 2);
    mvcaddstr(2, BASECOL(c) + PAGEWID / 2 + 1, CMDR_NAME " " CMDR_VERSION);
    mvcaddstr(3, BASECOL(c) + PAGEWID / 2 + 1, _("after the Demos Commander, S.Vakulenko"));
    mvcaddstr(5, BASECOL(c) + PAGEWID / 2 + 1, _("Directory"));
    mvcaddstr(6, BASECOL(c) + PAGEWID / 2 + 1, d->shortcwd);
    snprintf(buf, sizeof(buf), P_("\3%d\2 file", "\3%d\2 files", d->nfiles), d->nfiles);
    mvcaddstr(7, BASECOL(c) + PAGEWID / 2 + 2, buf);
    snprintf(buf, sizeof(buf), P_("\3%s\2 byte", "\3%s\2 bytes", d->nbytes), ltoac(d->nbytes));
    mvcaddstr(8, BASECOL(c) + PAGEWID / 2 + 2, buf);
    if (d->pattern[0]) {
        bytesinfiles(buf, sizeof(buf), _("%s in %s matched"), d->mbytes, d->mfiles);
        mvcaddstr(9, BASECOL(c) + PAGEWID / 2 + 3, buf);
    }
    if (d->tfiles) {
        bytesinfiles(buf, sizeof(buf), _("%s in %s selected"), d->tbytes, d->tfiles);
        mvcaddstr(10, BASECOL(c) + PAGEWID / 2 + 3, buf);
    }
#ifdef notyet
    mvcaddstr(13, BASECOL(c) + PAGEWID / 2 + 1, "No .dirinfo file in this directory");
#endif
}

static void drawstatus(int col, struct file *f, int view)
{
    int nw = 14 + PAGEWID - (widewin ? 80 : 40);

    VClearBox(H + 3, col + 1, 1, PAGEWID - 2);
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
    static char buf[1024]; /* n cells of UTF-8 */
    int len = VStrLen(p), k;
    const char *e;
    char *s;

    if (n > 250)
        n = 250;
    if (len > n) {
        if (tail) {
            buf[0] = '~';
            snprintf(buf + 1, sizeof(buf) - 1, "%s", VStrSkip(p, len - (n - 1)));
        } else {
            e = VStrSkip(p, n - 1);
            memcpy(buf, p, e - p);
            strcpy(buf + (e - p), "~");
        }
        return (buf);
    }
    if (cdir->alignext) {
        e = extension(p); /* the name, spaces, the extension at the end */
        memcpy(buf, p, e - p);
        s = buf + (e - p);
        for (k = len; k < n; ++k)
            *s++ = ' ';
        strcpy(s, e);
    } else {
        strcpy(buf, p);
        s = buf + strlen(buf);
        for (k = len; k < n; ++k)
            *s++ = ' ';
        *s = 0;
    }
    return (buf);
}
