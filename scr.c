/*
 *      Visual screen packadge.
 *      -----------------------
 *
 *      int VInit ()
 *              - initialise packadge. Returns 1 if ok else 0.
 *
 *      VOpen ()
 *              - enters video mode.
 *
 *      VClose ()
 *              - closes video mode.
 *
 *      VSetPalette (n, nb, nr, nrb, b, bb, br, brb, d, db, dr, drb)
 *              - set screen palette. Sets normal, normal background,
 *              normal reverse, normal reverse background, bold,
 *              bold background, bold reverse, bold reverse background,
 *              dim, dim background, dim reverse, dim reverse background.
 *              Default are 10, 0, 0, 10, 15, 0, 15, 12, 7, 0, 0, 6.
 *
 *      VRedraw ()
 *              - redraws screen.
 *
 *      VSync ()
 *              - refresh screen.
 *
 *      VFlush ()
 *              - flush terminal output buffer. Called just before
 *              getting input from terminal.
 *
 *      VBeep ()
 *              - bell.
 *
 *      VSyncLine (wy)
 *              - refresh line number wy.
 *
 *      VDelLine (n)
 *              - delete line.
 *
 *      VInsLine (n)
 *              - insert line.
 *
 *      VDelChar ()
 *              - delete char at current position, shift line left.
 *
 *      VInsChar ()
 *              - insert space at current position.
 *
 *      VMove (y, x)
 *              - set current position.
 *
 *      VClearLine ()
 *              - erase to end of line.
 *
 *      VClear ()
 *              - clear screen.
 *
 *      VPutChar (c)
 *              - put character to screen. Special characters are:
 *                      '\1'    - set dim
 *                      '\2'    - set normal
 *                      '\3'    - set bold
 *                      '\16'   - set reverse
 *                      '\17'   - unset reverse
 *                      '\t'    - next tab stop
 *                      '\r'    - return
 *                      '\n'    - new line
 *                      '\b'    - back space
 *
 *      VPutString (str)
 *              - print string to screen.
 *
 *      int VStandOut ()
 *              - set reverse attribute. Returns 1 if terminal
 *              has reverse, else 0.
 *
 *      VStandEnd ()
 *              - unset reverse attribute.
 *
 *      VSetNormal ()
 *              - set normal attribute.
 *
 *      VSetPrev ()
 *              - set attribute before last change.
 *
 *      VSetDim ()
 *              - set dim attribute.
 *
 *      VSetBold ()
 *              - set bold attribute.
 *
 *      CURSOR VGetCursor ()
 *              - get current cursor position.
 *
 *      VSetCursor (c)
 *              - set stored cursor position.
 *
 *      BOX *VGetBox (y, x, ny, nx)
 *              - get rectangular area of screen, called "box".
 *              (y, x) specifies upper left corner, ny, nx -
 *              vertical and horisontal sizes.
 *
 *      VUngetBox (box)
 *              - restore saved box.
 *
 *      VPrintBox (box)
 *              - print saved box with current attribute.
 *
 *      VFreeBox (box)
 *              - free box structure.
 *
 *      VExpandString (s, d)
 *              - expand string "s", which contains
 *              attribute switching escapes '\1, '\2', '\3', '\16', '\17'.
 *              Store expanded string in "d".
 */
/* #define DEBUG */

#include "scr.h"

#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "deco.h"
#include "env.h"

/* above the code point of a cell (CHARMASK) */
#define STANDOUT  (1 << 21)
#define BOLD      (1 << 22)
#define DIM       (1 << 23)
#define GRAPH     (1 << 24)
#define ROLESHIFT 25
#define ROLEMASK  (7 << ROLESHIFT) /* VCOL_*: 3 bits, the sign bit stays free */
#define NOCHANGE  -1
#define OUTBUFSZ  256

#define qputch(c) (outptr >= outbuf + OUTBUFSZ ? VFlush(), *outptr++ = (c) : (*outptr++ = (c)))

WINDOW VScreen;
int BlackWhite   = 0;
int ColorMode    = 1;
int GraphMode    = 1;
int TtyUpperCase = 0;

static WINDOW curscr;
static int scrool, rscrool;
static int beepflag;
static int prevattr;

#ifndef DECO_NEWT
static short ctab[16], btab[16];
static char *colorbuf, *colorp;
static char outbuf[OUTBUFSZ], *outptr = outbuf;
#endif

static char *BS, *BR, *DS, *DR, *NS, *NR;
static char *GS, *GE, *G1, *G2, *GT, *AC, *AS, *AE;
static char *CS, *SF, *SR, *EA;

static char *KS, *KE;

static char *CL, *CM, *SE, *SO, *TE, *TI, *VE, *VS, *AL, *DL, *IS, *IF, *FS, *MD, *MH, *ME, *MR,
    *CF, *CB, *AF, *AB, *Sf, *Sb, *MF, *MB;
static int NF, NB;
static char MS, C2;

static char Cy;
static char *Cs, *Ce, *Ct;

char VCyrInputTable['~' - ' ' + 1];
int VCyrInput; /* the terminal has the table (Cy, Ct): ^N, ^O switch it */
#ifndef DECO_NEWT
static char VCyrOutputTable[64];
static int cyroutput;
#endif

int LINES; /* number of lines on screen */
int COLS;  /* number of columns */

struct CapTab outtab[] = {
    // clang-format off
    { "ms", CAPFLG, 0, &MS, 0, 0, },
    { "C2", CAPFLG, 0, &C2, 0, 0, },
    { "CY", CAPFLG, 0, &Cy, 0, 0, },
    { "li", CAPNUM, 0, 0, &LINES, 0, },
    { "co", CAPNUM, 0, 0, &COLS, 0, },
    { "Nf", CAPNUM, 0, 0, &NF, 0, },
    { "Nb", CAPNUM, 0, 0, &NB, 0, },
    { "cl", CAPSTR, 0, 0, 0, &CL, },
    { "cm", CAPSTR, 0, 0, 0, &CM, },
    { "se", CAPSTR, 0, 0, 0, &SE, },
    { "so", CAPSTR, 0, 0, 0, &SO, },
    { "Cf", CAPSTR, 0, 0, 0, &CF, },
    { "Cb", CAPSTR, 0, 0, 0, &CB, },
    { "AF", CAPSTR, 0, 0, 0, &AF, },
    { "AB", CAPSTR, 0, 0, 0, &AB, },
    { "Sf", CAPSTR, 0, 0, 0, &Sf, },
    { "Sb", CAPSTR, 0, 0, 0, &Sb, },
    { "Mf", CAPSTR, 0, 0, 0, &MF, },
    { "Mb", CAPSTR, 0, 0, 0, &MB, },
    { "md", CAPSTR, 0, 0, 0, &MD, },
    { "mh", CAPSTR, 0, 0, 0, &MH, },
    { "mr", CAPSTR, 0, 0, 0, &MR, },
    { "me", CAPSTR, 0, 0, 0, &ME, },
    { "te", CAPSTR, 0, 0, 0, &TE, },
    { "ti", CAPSTR, 0, 0, 0, &TI, },
    { "vs", CAPSTR, 0, 0, 0, &VS, },
    { "ve", CAPSTR, 0, 0, 0, &VE, },
    { "ks", CAPSTR, 0, 0, 0, &KS, },
    { "ke", CAPSTR, 0, 0, 0, &KE, },
    { "al", CAPSTR, 0, 0, 0, &AL, },
    { "dl", CAPSTR, 0, 0, 0, &DL, },
    { "is", CAPSTR, 0, 0, 0, &IS, },
    { "if", CAPSTR, 0, 0, 0, &IF, },
    { "fs", CAPSTR, 0, 0, 0, &FS, },
    { "eA", CAPSTR, 0, 0, 0, &EA, },
    { "gs", CAPSTR, 0, 0, 0, &GS, },
    { "ge", CAPSTR, 0, 0, 0, &GE, },
    { "as", CAPSTR, 0, 0, 0, &AS, },
    { "ae", CAPSTR, 0, 0, 0, &AE, },
    { "g1", CAPSTR, 0, 0, 0, &G1, },
    { "g2", CAPSTR, 0, 0, 0, &G2, },
    { "gt", CAPSTR, 0, 0, 0, &GT, },
    { "ac", CAPSTR, 0, 0, 0, &AC, },
    { "cs", CAPSTR, 0, 0, 0, &CS, },
    { "sf", CAPSTR, 0, 0, 0, &SF, },
    { "sr", CAPSTR, 0, 0, 0, &SR, },
    { "Cs", CAPSTR, 0, 0, 0, &Cs, },
    { "Ce", CAPSTR, 0, 0, 0, &Ce, },
    { "Ct", CAPSTR, 0, 0, 0, &Ct, },
    { { 0, 0, }, 0, 0, 0, 0, 0, },
    // clang-format on
};

static char *skipdelay(char *);

static char linedraw[11] = {
    // clang-format off
    '-',    /* 0    horisontal line */
    '|',    /* 1    vertical line */
    '+',    /* 2    lower left corner */
    '-',    /* 3    lower center */
    '+',    /* 4    lower right corner */
    '|',    /* 5    left center */
    '+',    /* 6    center cross */
    '|',    /* 7    right center */
    '+',    /* 8    upper left corner */
    '-',    /* 9    upper center */
    '+',    /* 10   upper right corner */
    // clang-format on
};

#ifndef DECO_NEWT
static void putch(int c);
static void cyr(int on);
static void tputs(char *cp);
static void setattr(int c);
#ifndef DECO_NEWT
static char *rolecolor(int c);
#endif
static void initgraph(void);
static void resetattr(int c);
static void initcolor(void);
static void initbold(void);
#endif
static void pokechr(int y, int x, int c);
static void delwin(WINDOW *win);
static int newwin(WINDOW *win);
static int makenew(WINDOW *win);
#ifndef DECO_NEWT
static void makech(int y);
static void makerch(int y);
static void domvcur(int y, int x);
static void doscrool(void);
static void sclln(int y1, int y2, int n);
#endif
static void screrase();

#ifndef DECO_NEWT
int VInit()
{
    register char *p;
    int i;

    CapGet(outtab);

    /* a terminal in UTF-8 (the locale says so): Unicode box drawing */
    if (!((p = EnvGet("LC_ALL")) && *p) && !((p = EnvGet("LC_CTYPE")) && *p))
        p = EnvGet("LANG");
    VUnicode =
        p && (strstr(p, "UTF-8") || strstr(p, "utf8") || strstr(p, "UTF8") || strstr(p, "utf-8"));

    p = EnvGet("LINES");
    if (p && *p)
        LINES = strtol(p, 0, 0);
    p = EnvGet("COLUMNS");
    if (p && *p)
        COLS = strtol(p, 0, 0);
#ifdef TIOCGWINSZ
    {
        static struct winsize ws;
        if (ioctl(0, TIOCGWINSZ, &ws) >= 0) {
            if (ws.ws_row > 0)
                LINES = ws.ws_row;
            if (ws.ws_col > 0)
                COLS = ws.ws_col;
        }
    }
#endif
    if (LINES <= 6 || COLS <= 30)
        return (0);
    if (!CM)
        return (0);

    if (AF || Sf) {
        CF = AF;
        if (!CF)
            CF = Sf;
        CB = AB;
        if (!CB)
            CB = Sb;
        C2 = 0;
        NF = 16;
        NB = 8;
        MF = "042615378CAE9DBF";
        MB = "04261537";
    } else if (!NF) {
        p = EnvGet("TERM");
        if (p && *p) {
            if (strncmp(p, "cons", 4) == 0 || strncmp(p, "linux", 5) == 0 ||
                strncmp(p, "ibmpc3", 6) == 0 || strncmp(p, "pc3", 3) == 0) {
                /* BSD/OS, Linux */
                C2 = 1;
                NF = 16;
                NB = 8;
                MF = "042615378CAE9DBF";
                MB = "04261537";
                CF = "\33[%p1%{8}%/%d;3%p1%{8}%m%d;4%p2%dm";
            }
        }
    }
    if (!SO && !SE) {
        SO = "\33[7m";
        SE = "\33[m";
    }

    scrool = AL && DL;
    if (!(rscrool = SF && SR))
        SF = SR = 0;
    if (NF && ColorMode)
        initcolor();
    else if (ME)
        initbold();
    if ((G1 || G2 || GT || AC) && GraphMode)
        initgraph();

    for (i = ' '; i <= '~'; ++i)
        VCyrInputTable[i - ' '] = i;
    for (i = 0300; i <= 0377; ++i)
        VCyrOutputTable[i - 0300] = i;

    if (!Cs || !Ce)
        Cs = Ce = 0;
    if (Cy && Ct) {
        int fd = open(Ct, 0);
        if (fd >= 0) {
            read(fd, VCyrOutputTable, sizeof(VCyrOutputTable));
            VCyrInput = read(fd, VCyrInputTable, sizeof(VCyrInputTable)) > 0;
            close(fd);
        }
    }

    if (curscr.y)
        delwin(&curscr);
    if (VScreen.y)
        delwin(&VScreen);
    if (!newwin(&curscr) || !newwin(&VScreen)) {
        VClose();
        return (0);
    }
    curscr.clear = 1;
    resetattr(0);
    if (Cy)
        cyr(0);
    return (1);
}
#endif

#ifndef DECO_NEWT
/* in the terminal's screen of the commander (TI), not the programs' one */
static int inalt;

/* the programs' screen: what they wrote is there (and stays) */
static void leavealt(void)
{
    if (TE && inalt) {
        /* the terminal's own colours there, not a role's */
        if (ME)
            tputs(ME);
        else
            tputs("\33[0m");
        curscr.flgs = -1; /* to be set again */
        tputs(TE);
        inalt = 0;
    }
}

void VOpen()
{
    TtySet();
    if (IS)
        tputs(IS);
    if (TI) {
        tputs(TI);
        inalt = 1;
    }
    if (VS)
        tputs(VS);
    if (KS)
        tputs(KS);
    if (EA)
        tputs(EA);
    VFlush();
    VMouse(1);
}
#endif

#ifndef DECO_NEWT
void VReopen()
{
    TtySet();
    if (TI && !inalt) { /* the commander's screen again, all drawn anew */
        tputs(TI);
        inalt         = 1;
        VScreen.clear = 1;
    }
    if (VS)
        tputs(VS);
    if (KS)
        tputs(KS);
    if (ME)
        tputs(ME);
    VFlush();
    VMouse(1);
}
#endif

#ifndef DECO_NEWT
void VClose()
{
    if (curscr.y)
        setattr(0);
    if (Cy)
        cyr(0);
    if (FS)
        tputs(FS);
    if (VE)
        tputs(VE);
    if (TE && inalt)
        tputs(TE);
    inalt = 0;
    if (KE)
        tputs(KE);
    VFlush();
    VMouse(0);
    TtyReset();
}
#endif

#ifndef DECO_NEWT
/* keys without the commander's screen: the terminal raw, nothing drawn */
void VUserScreen()
{
    leavealt();
    VFlush();
    VMouse(0);
    TtySet();
}
#endif

#ifndef DECO_NEWT
void VRestore()
{
    if (Cy)
        cyr(0);
    if (VE)
        tputs(VE);
    if (KE)
        tputs(KE);
    leavealt();
    VFlush();
    VMouse(0);
    TtyReset();
}
#endif

static void delwin(WINDOW *win)
{
    register int i;

    for (i = 0; i < LINES && win->y[i]; i++)
        free(win->y[i]);
    free(win->y);
    free(win->firstch);
    free(win->lastch);
    free(win->lnum);
    win->y = 0;
}

static int newwin(WINDOW *win)
{
    register CELL *sp;
    register int i;

    if (!makenew(win))
        return (0);
    for (i = 0; i < LINES; i++) {
        win->y[i] = (CELL *)malloc((int)(COLS * sizeof(CELL)));
        if (!win->y[i]) {
            register int j;

            for (j = 0; j < i; j++)
                free(win->y[i]);
            free(win->y);
            free(win->firstch);
            free(win->lastch);
            free(win->lnum);
            win->y = 0;
            return (0);
        }
        for (sp = win->y[i]; sp < win->y[i] + COLS;)
            *sp++ = ' ';
    }
    return (1);
}

static int makenew(WINDOW *win)
{
    register int i;

    if (!(win->y = (CELL **)malloc((int)(LINES * sizeof(CELL *)))))
        return (0);
    if (!(win->firstch = (short *)malloc((int)(LINES * sizeof(short)))))
        goto b;
    if (!(win->lastch = (short *)malloc((int)(LINES * sizeof(short)))))
        goto c;
    if (!(win->lnum = (short *)malloc((int)(LINES * sizeof(short))))) {
        free(win->lastch);
    c:
        free(win->firstch);
    b:
        free(win->y);
        return (0);
    }
    win->cury = win->curx = 0;
    win->clear            = 1;
    win->flgs             = 0;
    for (i = 0; i < LINES; i++) {
        win->firstch[i] = win->lastch[i] = NOCHANGE;
        win->lnum[i]                     = i;
    }
    return (1);
}

#ifndef DECO_NEWT
void VRedraw()
{
    register short wy;
    register int y, x;

    tputs(CL);
    y           = curscr.cury;
    x           = curscr.curx;
    curscr.cury = 0;
    curscr.curx = 0;
    for (wy = 0; wy < LINES; wy++)
        makerch(wy);
    domvcur(y, x);
    setattr(VScreen.flgs);
    VFlush();
}
#endif

#ifndef DECO_NEWT
static void makerch(int y)
{
    register CELL *new;
    register short x;

    new = &curscr.y[y][0];
    for (x = 0; x < COLS; ++new, ++x) {
        if (*new == ' ')
            continue;
        if (x >= COLS - 1 && y >= LINES - 1)
            return;
        domvcur(y, x);
        setattr(*new);
        putch(*new);
        curscr.curx = x + 1;
    }
}
#endif

#ifndef DECO_NEWT
void VSyncLine(int wy)
{
    if (VScreen.firstch[wy] != NOCHANGE) {
        makech(wy);
        VScreen.firstch[wy] = NOCHANGE;
    }
}
#endif

#ifndef DECO_NEWT
void VSync()
{
    register short wy;

    if (VScreen.clear || curscr.clear) {
        setattr(0);
        tputs(CL);
        VScreen.clear = 0;
        curscr.clear  = 0;
        curscr.cury   = 0;
        curscr.curx   = 0;
        for (wy = 0; wy < LINES; wy++) {
            register CELL *sp, *end;

            end = &curscr.y[wy][COLS];
            for (sp = curscr.y[wy]; sp < end; sp++)
                *sp = ' ';
        }
        for (wy = 0; wy < LINES; wy++) {
            VScreen.firstch[wy] = 0;
            VScreen.lastch[wy]  = COLS - 1;
            VScreen.lnum[wy]    = wy;
        }
    } else if (rscrool || scrool)
        doscrool();
    for (wy = 0; wy < LINES; wy++) {
        VSyncLine(wy);
        VScreen.lnum[wy] = wy;
    }
    domvcur(VScreen.cury, VScreen.curx);
    curscr.cury = VScreen.cury;
    curscr.curx = VScreen.curx;
    if (beepflag) {
        qputch('\007');
        beepflag = 0;
    }
    setattr(VScreen.flgs);
    VFlush();
}
#endif

#ifndef DECO_NEWT
static void makech(int y)
{
    register CELL *new, *old;
    register short x, lastch;

    x      = VScreen.firstch[y];
    lastch = VScreen.lastch[y];
    old    = &curscr.y[y][x];
    new    = &VScreen.y[y][x];
    for (; x <= lastch; ++new, ++old, ++x) {
        if (*new == *old)
            continue;
        if (x >= COLS - 1 && y >= LINES - 1)
            return;
        domvcur(y, x);
        setattr(*new);
        putch(*old = *new);
        curscr.curx = x + 1;
    }
}
#endif

#ifndef DECO_NEWT
static void doscrool()
{
    register int line, n, topline, botline;
    int mustreset = 0;

    for (line = 0; line < LINES; ++line) {
        /* find next range to scrool */

        /* skip fresh lines */
        while (line < LINES && VScreen.lnum[line] < 0)
            ++line;

        /* last line reached - no range to scrool */
        if (line >= LINES)
            break;

        /* top line found */
        topline = line;

        /* skip range of old lines */
        while (line < LINES - 1 && VScreen.lnum[line] + 1 == VScreen.lnum[line + 1])
            ++line;

        /* bottom line found */
        botline = line;

        /* compute number of scrools, >0 - forward */
        n = topline - VScreen.lnum[topline];

        if (n == 0)
            continue;
        else if (n > 0)
            topline = VScreen.lnum[topline];
        else if (n < 0)
            botline = VScreen.lnum[botline];

        /* do scrool */

        if (rscrool && !scrool && !CS) {
            /* cannot scrool small regions if no scrool region */
            if (2 * (botline - topline) < LINES - 2) {
                for (line = topline; line <= botline; ++line) {
                    VScreen.firstch[line] = 0;
                    VScreen.lastch[line]  = COLS - 1;
                }
                return;
            }
            if (topline > 0) {
                for (line = 0; line < topline; ++line) {
                    VScreen.firstch[line] = 0;
                    VScreen.lastch[line]  = COLS - 1;
                }
                topline = 0;
            }
            if (botline < LINES - 1) {
                for (line = botline + 1; line < LINES; ++line) {
                    VScreen.firstch[line] = 0;
                    VScreen.lastch[line]  = COLS - 1;
                }
                botline = LINES - 1;
            }
        }

        /* update curscr */
        sclln(topline, botline, n);

        /* set scrool region */
        if (CS) {
            tputs(CapGoto(CS, botline, topline));
            mustreset = 1;
        }

        /* do scrool n lines forward or backward */
        if (n > 0) {
            if (CS || !scrool) {
                tputs(CapGoto(CM, 0, CS ? topline : 0));
                while (--n >= 0)
                    tputs(SR);
            } else {
                while (--n >= 0) {
                    tputs(CapGoto(CM, 0, botline));
                    tputs(DL);
                    tputs(CapGoto(CM, 0, topline));
                    tputs(AL);
                }
            }
        } else {
            if (CS || !scrool) {
                tputs(CapGoto(CM, 0, CS ? botline : LINES - 1));
                while (++n <= 0)
                    tputs(SF);
            } else {
                while (++n <= 0) {
                    tputs(CapGoto(CM, 0, topline));
                    tputs(DL);
                    tputs(CapGoto(CM, 0, botline));
                    tputs(AL);
                }
            }
        }
    }
    if (mustreset)
        /* unset scrool region */
        tputs(CapGoto(CS, LINES - 1, 0));
}
#endif

#ifndef DECO_NEWT
static void sclln(int y1, int y2, int n)
{
    register CELL *end, *temp;
    register int y;

    if (n > 0) {
        for (y = y2 - n + 1; y <= y2; ++y) {
            temp = curscr.y[y];
            for (end = &temp[COLS]; temp < end; *--end = ' ')
                ;
        }
        while (--n >= 0) {
            temp = curscr.y[y2];
            for (y = y2; y > y1; --y)
                curscr.y[y] = curscr.y[y - 1];
            curscr.y[y1] = temp;
        }
    } else {
        for (y = y1; y < y1 - n; ++y) {
            temp = curscr.y[y];
            for (end = &temp[COLS]; temp < end; *--end = ' ')
                ;
        }
        while (++n <= 0) {
            temp = curscr.y[y1];
            for (y = y1; y < y2; ++y)
                curscr.y[y] = curscr.y[y + 1];
            curscr.y[y2] = temp;
        }
    }
}
#endif

void VDelLine(int n)
{
    register CELL *temp;
    register int y;
    register CELL *end;

    if (n < 0 || n >= LINES)
        return;
    temp = VScreen.y[n];
    for (y = n; y < LINES - 1; y++) {
        VScreen.y[y]    = VScreen.y[y + 1];
        VScreen.lnum[y] = VScreen.lnum[y + 1];
        if (scrool || rscrool) {
            VScreen.firstch[y] = VScreen.firstch[y + 1];
            VScreen.lastch[y]  = VScreen.lastch[y + 1];
        } else {
            VScreen.firstch[y] = 0;
            VScreen.lastch[y]  = COLS - 1;
        }
    }
    VScreen.y[LINES - 1]       = temp;
    VScreen.lnum[LINES - 1]    = -1;
    VScreen.firstch[LINES - 1] = 0;
    VScreen.lastch[LINES - 1]  = COLS - 1;
    for (end = &temp[COLS]; temp < end; *temp++ = ' ')
        ;
}

void VInsLine(int n)
{
    register CELL *temp;
    register int y;
    register CELL *end;

    if (n < 0 || n >= LINES)
        return;
    temp = VScreen.y[LINES - 1];
    for (y = LINES - 1; y > n; --y) {
        VScreen.y[y]    = VScreen.y[y - 1];
        VScreen.lnum[y] = VScreen.lnum[y - 1];
        if (scrool || rscrool) {
            VScreen.firstch[y] = VScreen.firstch[y - 1];
            VScreen.lastch[y]  = VScreen.lastch[y - 1];
        } else {
            VScreen.firstch[y] = 0;
            VScreen.lastch[y]  = COLS - 1;
        }
    }
    VScreen.lnum[n]    = -1;
    VScreen.y[n]       = temp;
    VScreen.firstch[n] = 0;
    VScreen.lastch[n]  = COLS - 1;
    for (end = &temp[COLS]; temp < end; *temp++ = ' ')
        ;
}

#ifndef DECO_NEWT
static void domvcur(int y, int x)
{
    if (curscr.curx == x && curscr.cury == y)
        return;
    if (curscr.cury == y && x - curscr.curx > 0 && x - curscr.curx < 7) {
        register CELL i;

        while (curscr.curx < x) {
            i = curscr.y[curscr.cury][curscr.curx];
            if ((i & ~CHARMASK) == curscr.flgs ||
                (!rolecolor(i) && (i & ~CHARMASK & ~ROLEMASK) == curscr.flgs))
                putch((int)i);
            else
                break;
            ++curscr.curx;
        }
        if (curscr.curx == x)
            return;
    }
    if (!MS && curscr.flgs)
        setattr(0);
    tputs(CapGoto(CM, x, y));
    curscr.curx = x;
    curscr.cury = y;
}
#endif

void VMove(int y, int x)
{
    if (x >= 0 && x < COLS)
        VScreen.curx = x;
    if (y >= 0 && y < LINES)
        VScreen.cury = y;
}

void VClearLine()
{
    register CELL *sp, *end;
    register int y, x;
    register CELL *maxx;
    register int minx;

    CELL blank = ' ' | (VScreen.flgs & ROLEMASK); /* in the current colours */

    y    = VScreen.cury;
    x    = VScreen.curx;
    end  = &VScreen.y[y][COLS];
    minx = NOCHANGE;
    maxx = &VScreen.y[y][x];
    for (sp = maxx; sp < end; sp++)
        if (*sp != blank) {
            maxx = sp;
            if (minx == NOCHANGE)
                minx = sp - VScreen.y[y];
            *sp = blank;
        }
    if (minx != NOCHANGE) {
        if (VScreen.firstch[y] > minx || VScreen.firstch[y] == NOCHANGE)
            VScreen.firstch[y] = minx;
        if (VScreen.lastch[y] < maxx - VScreen.y[y])
            VScreen.lastch[y] = maxx - VScreen.y[y];
    }
}

void VClear()
{
    screrase();
    VScreen.clear = 1;
}

static void screrase()
{
    register int y;
    register CELL *sp, *end, *maxx;
    register int minx;

    for (y = 0; y < LINES; y++) {
        minx = NOCHANGE;
        maxx = 0;
        end  = &VScreen.y[y][COLS];
        for (sp = VScreen.y[y]; sp < end; sp++)
            if (*sp != ' ') {
                maxx = sp;
                if (minx == NOCHANGE)
                    minx = sp - VScreen.y[y];
                *sp = ' ';
            }
        if (minx != NOCHANGE) {
            if (VScreen.firstch[y] > minx || VScreen.firstch[y] == NOCHANGE)
                VScreen.firstch[y] = minx;
            if (VScreen.lastch[y] < maxx - VScreen.y[y])
                VScreen.lastch[y] = maxx - VScreen.y[y];
        }
        VScreen.lnum[y] = y;
    }
    VScreen.curx = VScreen.cury = 0;
}

void VPutString(char *str)
{
    while (*str)
        VPutChar(*str++);
}

/* the length of a UTF-8 string in cells: its code points */
int VStrLen(const char *s)
{
    int n = 0;

    for (; *s; ++s)
        if ((*s & 0300) != 0200)
            ++n;
    return n;
}

/* the string past its first n cells */
char *VStrSkip(const char *s, int n)
{
    for (; *s; ++s)
        if ((*s & 0300) != 0200 && --n < 0)
            break;
    return (char *)s;
}

/* at most n cells of a string */
void VPutStringN(const char *s, int n)
{
    const char *e = VStrSkip(s, n);

    while (s < e)
        VPutChar(*s++);
}

#ifndef DECO_NEWT
static char *makecolor(int c, int b)
{
    register char *p = colorp;

    if (C2) {
        strcpy(colorp, CapGoto(CF, b, c));
        while (*colorp++)
            ;
    } else {
#if 1
        if (c >= 8) { /* the bright ones (dark grey too) */
            strcpy(colorp, "\33[1m");
            while (*colorp++)
                ;
        } else {
            strcpy(colorp, "\33[0m");
            while (*colorp++)
                ;
        }
#endif
        strcpy(--colorp, CapGoto(CF, 0, c % 8));
        while (*colorp++)
            ;
        strcpy(--colorp, CapGoto(CB, 0, b));
        while (*colorp++)
            ;
    }
    return (p);
}
#endif

#ifndef DECO_NEWT
static void initcolor()
{
    register int i;

    if (NF <= 0 || NB <= 0 || !CF || (!CB && !C2))
        return;
    if (NF > 16)
        NF = 16;
    if (NB > 16)
        NB = 16;
    if (!MF)
        MF = "0123456789ABCDEF";
    if (!MB)
        MB = "0123456789ABCDEF";
    for (i = 0; i < 16; ++i)
        ctab[i] = btab[i] = -1;
    for (i = 0; i < 16 && i < NF; ++i)
        if (!MF[i])
            break;
        else if (MF[i] >= '0' && MF[i] <= '9')
            ctab[MF[i] - '0'] = i;
        else if (MF[i] >= 'A' && MF[i] <= 'F')
            ctab[MF[i] - 'A' + 10] = i;
    for (i = 0; i < 16 && i < NB; ++i)
        if (!MB[i])
            break;
        else if (MB[i] >= '0' && MB[i] <= '9')
            btab[MB[i] - '0'] = i;
        else if (MF[i] >= 'A' && MF[i] <= 'F')
            btab[MB[i] - 'A' + 10] = i;
    for (i = 1; i < 8; ++i) {
        if (ctab[i] >= 0 && ctab[i + 8] < 0)
            ctab[i + 8] = ctab[i];
        if (ctab[i + 8] >= 0 && ctab[i] < 0)
            ctab[i] = ctab[i + 8];
        if (btab[i] >= 0 && btab[i + 8] < 0)
            btab[i + 8] = btab[i];
        if (btab[i + 8] >= 0 && btab[i] < 0)
            btab[i] = btab[i + 8];
    }
    VSetPalette(7, 0, 0, 7, 15, 0, 15, 12, 3, 0, 0, 6);
}
#endif

#ifndef DECO_NEWT
/* the colours of each role (VSetColor) as six terminal strings, as
 * VSetPalette makes NS, NR, BS, BR, DS, DR: normal, reverse, bold, bold
 * reverse, dim, dim reverse */
static char rolebuf[VCOL_NROLES][384];
static char *rolestr[VCOL_NROLES][6];

void VSetRoleColors(int role, const int colors[12])
{
    char *save;
    int i;

    if (role < 0 || role >= VCOL_NROLES || !NF || !CF)
        return;
    for (i = 0; i < 12; ++i)
        if (colors[i] < 0 || colors[i] > 15 || (i & 1 ? btab[colors[i]] : ctab[colors[i]]) < 0)
            return;
    save   = colorp; /* VSetPalette's buffer */
    colorp = rolebuf[role];
    for (i = 0; i < 6; ++i)
        rolestr[role][i] = makecolor(ctab[colors[2 * i]], btab[colors[2 * i + 1]]);
    colorp = save;
}

/* the string of the role and the attributes of cell c, 0 if the role has
 * no colours */
static char *rolecolor(int c)
{
    static const int idx[8] = { 0, 1, 2, 3, 4, 5, 2, 3 }; /* by DIM BOLD STANDOUT */
    int role                = (c & ROLEMASK) >> ROLESHIFT;
    int a                   = (c & STANDOUT ? 1 : 0) | (c & BOLD ? 2 : 0) | (c & DIM ? 4 : 0);

    return rolestr[role][0] ? rolestr[role][idx[a]] : 0;
}
#endif

#ifndef DECO_NEWT
void VSetPalette(int n, int nb, int nr, int nrb, int b, int bb, int br, int brb, int d, int db,
                 int dr, int drb)
{
    if (!NF || ctab[n] < 0 || ctab[nr] < 0 || btab[nb] < 0 || btab[nrb] < 0)
        return;
    NS = NR = BS = BR = DS = DR = 0;
    if (colorp)
        free(colorbuf);
    colorp = colorbuf = malloc(256);
    NS                = makecolor(ctab[n], btab[nb]);   /* NORMAL - bright green on black */
    NR                = makecolor(ctab[nr], btab[nrb]); /* REVERSE NORMAL - black on bright green */
    if (ctab[b] < 0 || ctab[br] < 0 || btab[bb] < 0 || btab[brb] < 0)
        return;
    BS = makecolor(ctab[b], btab[bb]);   /* BOLD - bright white on black */
    BR = makecolor(ctab[br], btab[brb]); /* REVERSE BOLD - bright white on bright red */
    if (ctab[d] < 0 || ctab[dr] < 0 || btab[db] < 0 || btab[drb] < 0)
        return;
    DS = makecolor(ctab[d], btab[db]);   /* DIM - white on black */
    DR = makecolor(ctab[dr], btab[drb]); /* REVERSE DIM - bright yellow on brown */
}
#endif

#ifndef DECO_NEWT
static void initbold()
{
    if (ME)
        NS = skipdelay(ME);
    if (MD)
        BS = skipdelay(MD);
    if (MH)
        DS = skipdelay(MH);
    if (SO)
        SO = skipdelay(SO);
    else if (MR)
        SO = skipdelay(MR);
}
#endif

#ifndef DECO_NEWT
static void initgraph()
{
    register int i;
    register char *g = 0;

    if (G1)
        g = G1;
    else if (G2)
        g = G2;
    else if (GT) {
        GT[1] = GT[0];
        g     = GT + 1;
    }
    if (g)
        for (i = 0; i < 11 && *g; ++i, ++g)
            linedraw[i] = *g;
    else if (AC) {
        GS = AS;
        GE = AE;
        for (; AC[0] && AC[1]; AC += 2)
            switch (AC[0]) {
            case 'l':
                linedraw[8] = AC[1];
                break;
            case 'q':
                linedraw[0] = AC[1];
                break;
            case 'k':
                linedraw[10] = AC[1];
                break;
            case 'x':
                linedraw[1] = AC[1];
                break;
            case 'j':
                linedraw[4] = AC[1];
                break;
            case 'm':
                linedraw[2] = AC[1];
                break;
            case 'w':
                linedraw[9] = AC[1];
                break;
            case 'u':
                linedraw[7] = AC[1];
                break;
            case 'v':
                linedraw[3] = AC[1];
                break;
            case 't':
                linedraw[5] = AC[1];
                break;
            case 'n':
                linedraw[6] = AC[1];
                break;
            }
    }
}
#endif

#define RESETATTR(a, b, c, f) \
    if (a)                    \
        tputs(b);             \
    else {                    \
        tputs(c);             \
        tputs(f);             \
    }                         \
    break

#ifndef DECO_NEWT
static void resetattr(int c)
{
    char *rc = rolecolor(c);

    if (rc) {
        tputs(rc);
        curscr.flgs = c & (DIM | BOLD | STANDOUT | GRAPH | ROLEMASK);
        return;
    }
    c &= DIM | BOLD | STANDOUT | GRAPH;
    switch (c & (DIM | BOLD | STANDOUT)) {
    case 0:
        RESETATTR(NR, NS, NS, SE);
    case STANDOUT:
        RESETATTR(NR, NR, NS, SO);
    case DIM:
        RESETATTR(DR, DS, DS, SE);
    case DIM | STANDOUT:
        RESETATTR(DR, DR, DS, SO);
    case BOLD:
        RESETATTR(BR, BS, BS, SE);
    case BOLD | STANDOUT:
        RESETATTR(BR, BR, BS, SO);
    }
    curscr.flgs = c;
}
#endif

#define SETATTR(a, b, c)                                \
    if (a)                                              \
        tputs(b);                                       \
    else {                                              \
        if (curscr.flgs & STANDOUT) {                   \
            tputs(SE);                                  \
            tputs(b);                                   \
        } else if ((curscr.flgs & (DIM | BOLD)) != (c)) \
            tputs(b);                                   \
    }                                                   \
    break

#define SETREVA(a, b, c)                         \
    if (a)                                       \
        tputs(a);                                \
    else {                                       \
        if ((curscr.flgs & (DIM | BOLD)) != (c)) \
            tputs(b);                            \
        if (!(curscr.flgs & STANDOUT))           \
            tputs(SO);                           \
    }                                            \
    break

#ifndef DECO_NEWT
static void setattr(int c)
{
    char *rc = rolecolor(c);

    if (rc) {
        /* the colours of a role: one string sets both and the attribute */
        c &= DIM | BOLD | STANDOUT | GRAPH | ROLEMASK;
        if ((c & GRAPH) != (curscr.flgs & GRAPH))
            tputs(c & GRAPH ? GS : GE);
        if ((c & ~GRAPH) != (curscr.flgs & ~GRAPH))
            tputs(rc);
        curscr.flgs = c;
        return;
    }
    c &= DIM | BOLD | STANDOUT | GRAPH;
    if ((c & GRAPH) != (curscr.flgs & GRAPH)) {
        if (c & GRAPH) {
            tputs(GS);
            resetattr(c);
            return;
        } else {
            tputs(GE);
        }
    }
    if ((c & (DIM | BOLD | STANDOUT)) != (curscr.flgs & (DIM | BOLD | STANDOUT)))
        switch (c & (DIM | BOLD | STANDOUT)) {
        case 0:
            SETATTR(NR, NS, 0);
        case STANDOUT:
            SETREVA(NR, NS, 0);
        case DIM:
            SETATTR(DR, DS, DIM);
        case DIM | STANDOUT:
            SETREVA(DR, DS, DIM);
        case BOLD:
            SETATTR(BR, BS, BOLD);
        case BOLD | STANDOUT:
            SETREVA(BR, BS, BOLD);
        }
    curscr.flgs = c;
}
#endif

int VSetColor(int role)
{
    int old = (VScreen.flgs & ROLEMASK) >> ROLESHIFT;

    VScreen.flgs = (VScreen.flgs & ~ROLEMASK) | ((role << ROLESHIFT) & ROLEMASK);
    return old;
}

int VStandOut()
{
    if (!SO && !NR)
        return (0);
    VScreen.flgs |= STANDOUT;
    return (1);
}

void VStandEnd()
{
    VScreen.flgs &= ~STANDOUT;
}

void VSetNormal()
{
    prevattr = VScreen.flgs;
    VScreen.flgs &= ~(BOLD | DIM);
}

void VSetPrev()
{
    VScreen.flgs &= ~(BOLD | DIM);
    VScreen.flgs |= prevattr & (BOLD | DIM);
}

int VSetDim()
{
    prevattr = VScreen.flgs & (BOLD | DIM);
    VScreen.flgs &= ~(BOLD | DIM);
    if (!DS)
        return (0);
    if (BlackWhite)
        return (0);
    VScreen.flgs |= DIM;
    return (1);
}

int VSetBold()
{
    prevattr = VScreen.flgs & (BOLD | DIM);
    VScreen.flgs &= ~(BOLD | DIM);
    if (!BS)
        return (0);
    if (BlackWhite)
        return (0);
    VScreen.flgs |= BOLD;
    return (1);
}

/* the UTF-8 sequence VPutChar is in the middle of */
static int utf8need, utf8cp;

/*
 * A character: a code point, or a byte of UTF-8 (0200-0377, also as a
 * negative char), gathered to its code point; the control characters
 * below ' ' set attributes and move.
 */
void VPutChar(int c)
{
    register int x, y;

    if (c < 0)
        c &= 0377;
    if (c >= 0200 && c <= 0377) {
        if (c >= 0300) { /* a lead byte */
            utf8need = c >= 0360 ? 3 : c >= 0340 ? 2 : 1;
            utf8cp   = c & (c >= 0360 ? 007 : c >= 0340 ? 017 : 037);
            return;
        }
        if (!utf8need) /* a continuation byte alone */
            c = 0xfffd;
        else {
            utf8cp = utf8cp << 6 | (c & 077);
            if (--utf8need)
                return;
            c = utf8cp;
        }
    } else
        utf8need = 0;
    if (c > CHARMASK)
        c = 0xfffd;
    x = VScreen.curx;
    y = VScreen.cury;
    switch (c) {
    case '\16':
        VStandOut();
        return;
    case '\17':
        VStandEnd();
        return;
    case '\1':
        VSetDim();
        return;
    case '\2':
        VSetNormal();
        return;
    case '\3':
        VSetBold();
        return;
    case '\t':
        x += (8 - (x & 07));
        break;
    default:
        pokechr(y, x++, c | VScreen.flgs);
        break;
    case '\n':
        ++y;
    case '\r':
        x = 0;
        break;
    case '\b':
        if (x == 0)
            return;
        --x;
        break;
    }
    if (x >= COLS) {
        x = 0;
        if (++y >= LINES)
            y = 0;
    }
    VScreen.curx = x;
    VScreen.cury = y;
}

static void pokechr(int y, int x, int c)
{
    if (VScreen.y[y][x] == c)
        return;
    if (VScreen.firstch[y] == NOCHANGE)
        VScreen.firstch[y] = VScreen.lastch[y] = x;
    else if (x < VScreen.firstch[y])
        VScreen.firstch[y] = x;
    else if (x > VScreen.lastch[y])
        VScreen.lastch[y] = x;
    VScreen.y[y][x] = c;
}

static char *skipdelay(char *cp)
{
    while (*cp >= '0' && *cp <= '9')
        ++cp;
    if (*cp == '.') {
        ++cp;
        while (*cp >= '0' && *cp <= '9')
            ++cp;
    }
    if (*cp == '*')
        ++cp;
    return (cp);
}

#ifndef DECO_NEWT
static void tputs(char *cp)
{
    register int c;

    if (!cp)
        return;
    cp = skipdelay(cp);
    while ((c = *cp++))
        qputch(c);
}
#endif

#ifndef DECO_NEWT
static void cyr(int on)
{
    if (on) {
        if (!cyroutput) {
            tputs(Cs);
            cyroutput = 1;
        }
    } else {
        if (cyroutput) {
            tputs(Ce);
            cyroutput = 0;
        }
    }
}
#endif

#ifndef DECO_NEWT
static void putch(int c)
{
    c &= CHARMASK;
    if (TtyUpperCase && c >= 'a' && c <= 'z')
        c += 'A' - 'a';
    if (c < 0200) {
        if (Cy && c > ' ' && c <= '~')
            cyr(0);
        qputch(c);
        return;
    }
    /* the terminal speaks UTF-8; the KOI8 switching (CY) is history */
    if (c < 0x800) {
        qputch(0300 | c >> 6);
    } else {
        if (c < 0x10000)
            qputch(0340 | c >> 12);
        else {
            qputch(0360 | c >> 18);
            qputch(0200 | (c >> 12 & 077));
        }
        qputch(0200 | (c >> 6 & 077));
    }
    qputch(0200 | (c & 077));
}
#endif

#ifndef DECO_NEWT
void VFlush()
{
    if (outptr > outbuf)
        write(1, outbuf, (unsigned)(outptr - outbuf));
    outptr = outbuf;
}
#endif

#ifndef DECO_NEWT
void VBeep()
{
    beepflag = 1;
}
#endif

CURSOR VGetCursor()
{
    return ((CURSOR)((long)VScreen.cury << 16 | VScreen.curx));
}

void VSetCursor(CURSOR c)
{
    VMove((int)(c >> 16), (int)c & 0xffff);
}

BOX *VGetBox(int y, int x, int ny, int nx)
{
    register int xx, yy;
    register CELL *p, *q;
    BOX *box;

    box      = (BOX *)malloc((int)(sizeof(BOX)));
    box->y   = y;
    box->x   = x;
    box->ny  = ny;
    box->nx  = nx;
    box->mem = (CELL *)malloc((int)(ny * nx * sizeof(CELL)));

    for (yy = 0; yy < ny; ++yy) {
        p = &VScreen.y[yy + y][x];
        q = &box->mem[yy * nx];
        for (xx = 0; xx < nx; ++xx)
            *q++ = *p++;
    }
    return (box);
}

void VUngetBox(BOX *box)
{
    register int xx, yy;
    register CELL *q;

    for (yy = 0; yy < box->ny; ++yy) {
        q = &box->mem[yy * box->nx];
        for (xx = 0; xx < box->nx; ++xx, ++q)
            pokechr(box->y + yy, box->x + xx, *q);
    }
}

void VPrintBox(BOX *box)
{
    register int xx, yy;
    register CELL *q;

    for (yy = 0; yy < box->ny; ++yy) {
        q = &box->mem[yy * box->nx];
        for (xx = 0; xx < box->nx; ++xx)
            pokechr(box->y + yy, box->x + xx, (*q++ & CHARMASK) | VScreen.flgs);
    }
}

void VFreeBox(BOX *box)
{
    free(box->mem);
    free(box);
}

void VClearBox(int r, int c, int nr, int nc)
{
    register int i;

    for (; --nr >= 0; ++r)
        for (i = nc; --i >= 0;)
            pokechr(r, c + i, ' ' | (VScreen.flgs & ROLEMASK));
}

void VFillBox(int r, int c, int nr, int nc, int sym)
{
    register int i;

    for (; --nr >= 0; ++r)
        for (i = nc; --i >= 0;)
            pokechr(r, c + i, sym | VScreen.flgs);
}

void VHorLine(int r, int c, int nc)
{
    register int sym;

    sym = (linedraw[0] & CHARMASK) | GRAPH | VScreen.flgs;
    while (--nc >= 0)
        pokechr(r, c++, sym);
}

void VVertLine(int c, int r, int nr)
{
    register int sym;

    sym = (linedraw[1] & CHARMASK) | GRAPH | VScreen.flgs;
    while (--nr >= 0)
        pokechr(r++, c, sym);
}

void VCorner(int r, int c, int n)
{
    static short map[9] = { 8, 9, 10, 5, 6, 7, 2, 3, 4 };

    switch (n) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        n = (linedraw[map[n - 1]] & CHARMASK) | GRAPH;
        break;
    default:
        n = '?';
        break;
    }
    pokechr(r, c, n | VScreen.flgs);
}

/*
 * Lines of Unicode box drawing, single or double (LINE_SINGLE,
 * LINE_DOUBLE), when the screen speaks UTF-8 (VUnicode); else the line
 * drawing of the terminal, single only.
 */
int VUnicode;

/* the box drawing character with arms up, down, left, right, each none,
 * single or double: [u * 27 + d * 9 + l * 3 + r], 0 where Unicode has
 * none (an axis of two kinds) */
static const unsigned short boxchars[81] = {
    0x0000, 0x2576, 0x0000, 0x2574, 0x2500, 0x0000, 0x0000, 0x0000, 0x2550, 0x2577, 0x250c, 0x2552,
    0x2510, 0x252c, 0x0000, 0x2555, 0x0000, 0x2564, 0x0000, 0x2553, 0x2554, 0x2556, 0x2565, 0x0000,
    0x2557, 0x0000, 0x2566, 0x2575, 0x2514, 0x2558, 0x2518, 0x2534, 0x0000, 0x255b, 0x0000, 0x2567,
    0x2502, 0x251c, 0x255e, 0x2524, 0x253c, 0x0000, 0x2561, 0x0000, 0x256a, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2559, 0x255a, 0x255c, 0x2568, 0x0000,
    0x255d, 0x0000, 0x2569, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x2551, 0x255f, 0x2560, 0x2562, 0x256b, 0x0000, 0x2563, 0x0000, 0x256c,
};

static int boxchar(int u, int d, int l, int r)
{
    int ch = boxchars[u * 27 + d * 9 + l * 3 + r];

    if (!ch) { /* an axis of two kinds: the stronger one for both arms */
        if (l && r)
            l = r = l > r ? l : r;
        if (u && d)
            u = d = u > d ? u : d;
        ch = boxchars[u * 27 + d * 9 + l * 3 + r];
    }
    return ch ? ch : '+';
}

/* the terminal's own line drawing for a joint: VCorner's numbers, by
 * which arms there are (as the keypad: 1 lower left ... 9 upper right) */
static int acsjoint(int u, int d, int l, int r)
{
    static const signed char corner[16] = {
        /* u d l r as bits 8 4 2 1 */
        -1, -1, -1, -1, -1, 1, 3, 2, -1, 7, 9, 8, -1, 4, 6, 5,
    };
    int n = corner[(u ? 8 : 0) | (d ? 4 : 0) | (l ? 2 : 0) | (r ? 1 : 0)];

    if (n < 0) /* a line end or a straight line */
        return (u || d) ? (linedraw[1] & CHARMASK) | GRAPH : (linedraw[0] & CHARMASK) | GRAPH;
    {
        static const short map[9] = { 8, 9, 10, 5, 6, 7, 2, 3, 4 };

        return (linedraw[map[n - 1]] & CHARMASK) | GRAPH;
    }
}

/* a joint of lines at r, c: the arms up, down, left, right */
void VJoint(int r, int c, int u, int d, int l, int rt)
{
    pokechr(r, c, (VUnicode ? boxchar(u, d, l, rt) : acsjoint(u, d, l, rt)) | VScreen.flgs);
}

/* a line of n cells from r, c: across (vert 0) or down (vert 1) */
void VLine(int r, int c, int n, int vert, int kind)
{
    int sym = VUnicode ? (vert ? boxchar(kind, kind, 0, 0) : boxchar(0, 0, kind, kind))
                       : (linedraw[vert] & CHARMASK) | GRAPH;

    sym |= VScreen.flgs;
    while (--n >= 0)
        if (vert)
            pokechr(r++, c, sym);
        else
            pokechr(r, c++, sym);
}

/* a box of nr x nc cells, its frame single or double */
void VFrame(int r, int c, int nr, int nc, int kind)
{
    VLine(r, c + 1, nc - 2, 0, kind);
    VLine(r + nr - 1, c + 1, nc - 2, 0, kind);
    VLine(r + 1, c, nr - 2, 1, kind);
    VLine(r + 1, c + nc - 1, nr - 2, 1, kind);
    VJoint(r, c, 0, kind, 0, kind);
    VJoint(r, c + nc - 1, 0, kind, kind, 0);
    VJoint(r + nr - 1, c, kind, 0, 0, kind);
    VJoint(r + nr - 1, c + nc - 1, kind, 0, kind, 0);
}

void VDrawBox(int r, int c, int nr, int nc)
{
    if (VUnicode) { /* dialogs, menus and lists: double, as VC */
        VFrame(r, c, nr, nc, LINE_DOUBLE);
        return;
    }
    VHorLine(r, c + 1, nc - 2);
    VHorLine(r + nr - 1, c + 1, nc - 2);
    VVertLine(c, r + 1, nr - 2);
    VVertLine(c + nc - 1, r + 1, nr - 2);
    VCorner(r, c, 1);
    VCorner(r, c + nc - 1, 3);
    VCorner(r + nr - 1, c, 7);
    VCorner(r + nr - 1, c + nc - 1, 9);
}

#define ADDSTR(a)                \
    if (a) {                     \
        strcpy(d, skipdelay(a)); \
        while (*d)               \
            ++d;                 \
    }

#define ADDREVERSE(a, b) \
    if (reverse == (a))  \
        continue;        \
    ADDSTR(b)            \
    reverse = (a);

#define ADDCOLOR(a, b, c)       \
    if (bright == (a))          \
        continue;               \
    ADDSTR(reverse ? (b) : (c)) \
    bright = (a);

void VExpandString(char *s, char *d)
{
    /* expand string s to d substituting \1, \2, \3, \16, \17 */
    register int c;
    int bright; /* 1=bold, 0=normal, 2=dim */
    int reverse;

    bright  = 0;
    reverse = 0;
    while ((c = *s++))
        switch (c) {
        case '\17':
            ADDREVERSE(0, SE);
            break;
        case '\16':
            ADDREVERSE(1, SO);
            break;
        case '\1':
            ADDCOLOR(2, DR, DS);
            break;
        case '\2':
            ADDCOLOR(0, NR, NS);
            break;
        case '\3':
            ADDCOLOR(1, BR, BS);
            break;
        default:
            *d++ = c;
            break;
        }
    *d = 0;
}

void VPrint(char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    VPutString(buf);
}

#ifdef notdef
static void tstp()
{
    tputs(CapGoto(CM, 0, LINES - 1));
    VRestore();
    kill(0, SIGSTOP);
    VReopen();
    VRedraw();
}
#endif

#ifdef DOC
#include <stdio.h>

void _prscreen(void)
{
    static char filename[] = "screenX", fname[128];
    static count           = 0;
    register FILE *fd;
    register int y, x;
    CELL c, a, m;
    char *toname;

    filename[6] = 'A' + count;
    toname      = EnvGet("DECOSCREEN");
    if (!toname)
        toname = ".";
    strcpy(fname, toname);
    strcat(fname, "/");
    strcat(fname, filename);
    fd = fopen(fname, "w");
    if (!fd)
        return;
    ++count;
    a = 0;
    m = BOLD | DIM;
    for (y = 0; y < LINES; ++y) {
        for (x = 0; x < COLS; ++x) {
            c = curscr.y[y][x];
            if ((a & GRAPH) != (c & GRAPH))
                putc(c & GRAPH ? cntrl('[') : cntrl('\\'), fd);
            if ((a & STANDOUT) != (c & STANDOUT))
                putc(c & STANDOUT ? cntrl('n') : cntrl('o'), fd);
            if ((a & m) != (c & m))
                switch (c & m) {
                case BOLD:
                    putc(3, fd);
                    break;
                case DIM:
                    putc(1, fd);
                    break;
                case 0:
                    putc(2, fd);
                    break;
                }
            putc(c & GRAPH ? _graphsym(c) : c, fd);
            a = c;
        }
        putc('\n', fd);
    }
    fclose(fd);
}

static int _graphsym(int c)
{
    register i;

    c &= 0xff;
    for (i = 0; i < 11; ++i)
        if (c == linedraw[i])
            switch (i) {
            case 0: /* horisontal line */
                return ('-');
            case 1: /* vertical line */
                return ('|');
            case 2:  /* lower left corner */
            case 3:  /* lower center */
            case 4:  /* lower right corner */
            case 5:  /* left center */
            case 6:  /* center cross */
            case 7:  /* right center */
            case 8:  /* upper left corner */
            case 9:  /* upper center */
            case 10: /* upper right corner */
                return ('0' + i - 1);
            }
    return ('?');
}
#endif

#ifdef DECO_NEWT
/*
 * The screen on newt (S-Lang's screen management, which newt draws with):
 * the virtual screen above stays, VSync writes its changed cells with
 * SLsmg, which sends only what differs; newt's windows (the dialogs of
 * choice.c) save and restore what they cover.
 */
#include <newt.h>
#include <slang.h>

/* S-Lang colour objects of the six attributes, past newt's own, for each
 * colour role */
enum { C_NORMAL = 64, C_REVERSE, C_BOLD, C_BOLDREV, C_DIM, C_DIMREV };
#define C_ROLE(r) (6 * (r))

static const char *const colornames[16] = {
    "black",     "blue",          "green",  "cyan",       "red",         "magenta",
    "brown",     "lightgray",     "gray",   "brightblue", "brightgreen", "brightcyan",
    "brightred", "brightmagenta", "yellow", "white",
};

static int newt_on;

/* newt with GonWare's mouse patch would take the mouse reports itself
 * (presses only); the commander reads them (mouse.c) */
static void newtnomouse(int on)
{
    if (on)
        setenv("NEWT_NOMOUSE", "1", 1);
    else
        unsetenv("NEWT_NOMOUSE");
}

static void newt_color(int obj, int fg, int bg)
{
    SLtt_set_color(obj, NULL, (char *)colornames[fg & 15], (char *)colornames[bg & 15]);
}

int VInit()
{
    if (!newt_on) {
        newtnomouse(1);
        newtInit();
        newtnomouse(0);
        VMouse(1);
        newt_on = 1;
    }
    newtGetScreenSize(&COLS, &LINES);
    if (LINES <= 6 || COLS <= 30)
        return (0);
    /* the line drawing characters in S-Lang's alternate set; Unicode
     * box drawing when S-Lang speaks UTF-8 */
    memcpy(linedraw, "qxmvjtnulwk", 11);
    VUnicode = SLutf8_enable(-1) > 0;
    SO = NR = BS = DS = "";
    VSetPalette(7, 0, 0, 7, 15, 0, 15, 12, 3, 0, 0, 6);
    if (curscr.y)
        delwin(&curscr);
    if (VScreen.y)
        delwin(&VScreen);
    if (!newwin(&curscr) || !newwin(&VScreen)) {
        VClose();
        return (0);
    }
    VScreen.clear = 1;
    return (1);
}

void VOpen()
{
}

/* keys without the commander's screen: the terminal raw, nothing drawn
 * (the screen of the programs, Ctrl-O) */
static int user_raw;

void VUserScreen()
{
    if (newt_on) {
        VMouse(0);
        newtSuspend();
        newt_on = 0;
    }
    if (!user_raw) {
        SLang_init_tty(0, 0, 0);
        user_raw = 1;
    }
}

static void userend(void)
{
    if (user_raw) {
        SLang_reset_tty();
        user_raw = 0;
    }
}

void VReopen()
{
    userend();
    if (!newt_on) {
        newtnomouse(1);
        newtResume();
        newtnomouse(0);
        VMouse(1);
        newt_on = 1;
    }
    VScreen.clear = 1;
}

void VClose()
{
    userend();
    if (newt_on) {
        VMouse(0);
        newtFinished();
        newt_on = 0;
    }
}

/* the terminal for a program run from the commander */
void VRestore()
{
    userend();
    if (newt_on) {
        VMouse(0);
        newtSuspend();
        newt_on = 0;
    }
}

/* colors: fg and bg of normal, reverse, bold, bold reverse, dim, dim
 * reverse */
void VSetRoleColors(int role, const int colors[12])
{
    int i;

    if (role < 0 || role >= VCOL_NROLES)
        return;
    for (i = 0; i < 6; ++i)
        newt_color(C_NORMAL + C_ROLE(role) + i, colors[2 * i], colors[2 * i + 1]);
}

/* the colours of the default role (the "palette" of the init file) */
void VSetPalette(int n, int nb, int nr, int nrb, int b, int bb, int br, int brb, int d, int db,
                 int dr, int drb)
{
    const int c[12] = { n, nb, nr, nrb, b, bb, br, brb, d, db, dr, drb };

    VSetRoleColors(VCOL_DEFAULT, c);
}

static int color_of(int c)
{
    int rev  = (c & STANDOUT) != 0;
    int role = C_ROLE((c & ROLEMASK) >> ROLESHIFT);

    if (c & BOLD)
        return role + (rev ? C_BOLDREV : C_BOLD);
    if (c & DIM)
        return role + (rev ? C_DIMREV : C_DIM);
    return role + (rev ? C_REVERSE : C_NORMAL);
}

static void newt_line(int y)
{
    int x, last, attr = -1, graph = 0;
    CELL c;

    x    = VScreen.firstch[y];
    last = VScreen.lastch[y];
    if (x == NOCHANGE)
        return;
    SLsmg_gotorc(y, x);
    for (; x <= last && x < COLS; ++x) {
        c = VScreen.y[y][x];
        if ((c & (BOLD | DIM | STANDOUT | ROLEMASK)) != attr) {
            attr = c & (BOLD | DIM | STANDOUT | ROLEMASK);
            SLsmg_set_color(color_of(c));
        }
        if (((c & GRAPH) != 0) != graph) {
            graph = (c & GRAPH) != 0;
            SLsmg_set_char_set(graph);
        }
        SLsmg_write_char((SLwchar_Type)(c & CHARMASK));
    }
    if (graph)
        SLsmg_set_char_set(0);
    VScreen.firstch[y] = NOCHANGE;
}

void VSyncLine(int wy)
{
    newt_line(wy);
}

void VSync()
{
    int y;

    if (VScreen.clear) {
        SLsmg_set_color(C_NORMAL);
        SLsmg_cls();
        VScreen.clear = 0;
        for (y = 0; y < LINES; y++) {
            VScreen.firstch[y] = 0;
            VScreen.lastch[y]  = COLS - 1;
        }
    }
    for (y = 0; y < LINES; y++) {
        newt_line(y);
        VScreen.lnum[y] = y;
    }
    SLsmg_gotorc(VScreen.cury, VScreen.curx);
    if (beepflag) {
        SLtt_beep();
        beepflag = 0;
    }
    SLsmg_refresh();
}

void VRedraw()
{
    SLsmg_touch_lines(0, LINES);
    VScreen.clear = 1;
    VSync();
}

void VFlush()
{
    if (newt_on) /* not on the programs' screen (VUserScreen) */
        SLsmg_refresh();
}

void VBeep()
{
    beepflag = 1;
}
#endif
