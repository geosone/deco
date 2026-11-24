#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "deco.h"
#include "rec.h"
#include "scr.h"

static int editmodflg;

/* the text deleted last, for Ctrl-U; undelline: a whole line */
static char *undel;
static int undellen, undelline;

static void keepdel(const char *s, int n, int line)
{
    free(undel);
    undel     = n > 0 ? malloc(n) : 0;
    undellen  = undel ? n : 0;
    undelline = line;
    if (undel)
        memcpy(undel, s, n);
}

extern char viewsrch[SEARCHSZ]; /* search string */
extern int viewraw;             /* raw mode */
extern int viewtabs;            /* visible spaces mode */
extern int viewfd;              /* file descriptor */
extern REC *viewrec;

static void clearline(REC *r, int line);
static void joinlines(REC *r, int line);
static void breakline(REC *r, int line, int off);
static void cleartoeoln(REC *r, int line, int off);
static int wordend(LINE *p, int off);
static int wordbeg(LINE *p, int off);
static int ischar(int c);

static int charcode(int l, int n)
{
    LINE *p;

    p = RecGet(viewrec, l);
    if (!p)
        return (0);
    if (n >= p->len)
        return ('\n');
    if (!viewraw) { /* the code point of a character in UTF-8 */
        const unsigned char *s = (const unsigned char *)p->ptr + n;
        int k                  = utf8len(p->ptr + n, p->ptr + p->len), c;

        if (k) {
            c = *s & (0177 >> k);
            while (--k > 0)
                c = c << 6 | (*++s & 077);
            return c;
        }
    }
    return (p->ptr[n] & 0xff);
}

static void edithead(char *filename, int line, int col, int off)
{
    register int i;
    char lc[64], cc[32];

    VSetDim();
    VStandOut();
    VMove(0, 0);
    for (i = COLS; --i >= 0; VPutChar(' '))
        ;
    if (editmodflg) /* changed since saved */
        VMPutString(0, 0, "*");
    VMPutString(0, 1, _("File "));
    VPutString(filename);
    VPrint(_("  Lines %d"), viewrec->len);
    /* at the right: "Line 1     Col 0  Char U+00F6", whatever the language */
    i = charcode(line, off);
    snprintf(cc, sizeof(cc), _("Char U+%04X"), i);
    snprintf(lc, sizeof(lc), _("Line %-5d Col %d"), line + 1, col);
    VMPutString(0, COLS - 1 - VStrLen(cc), cc);
    VMPutString(0, COLS - 3 - VStrLen(cc) - VStrLen(lc), lc);
    VSetNormal();
    VStandEnd();
}

/* the start of the character off is in (UTF-8) */
static int charstart(int line, int off)
{
    LINE *l = RecGet(viewrec, line);

    if (l && !viewraw)
        while (off > 0 && off < l->len && UTF8CONT(l->ptr[off]))
            --off;
    return off;
}

/* the bytes of the character at off (UTF-8) */
static int charlen(int line, int off)
{
    LINE *l = RecGet(viewrec, line);
    int n;

    if (!l || viewraw || off >= l->len)
        return 1;
    n = utf8len(l->ptr + off, l->ptr + l->len);
    return n ? n : 1;
}

static int lastpos(int n)
{
    register LINE *p;

    p = RecGet(viewrec, n);
    if (!p)
        return (0);
    return (p->len);
}

static const char *const editkeys[10] = { NC_("keybar", "Help"),   NC_("keybar", "Save"),
                                          NC_("keybar", "Raw"),    NC_("keybar", "Hex"),
                                          NC_("keybar", "Top"),    NC_("keybar", "Bottom"),
                                          NC_("keybar", "Search"), NC_("keybar", "Home"),
                                          NC_("keybar", "Spaces"), NC_("keybar", "Quit") };

/*
 * The hex mode (F4): the text as the bytes it is saved as, each line and
 * its end (0a), sixteen to a row as the hex viewer shows them. Typing
 * overwrites a byte, in the hex field as two hex digits, in the text
 * field as the character; the line ends stay, and no byte becomes one,
 * so the lines under the editor do not move. Tab changes the field, F4
 * goes back to the text at the same place.
 */
static long *hstart; /* the offset of each line, and the end */
static long hsize;   /* the bytes */

static void hexindex(void)
{
    int i;

    free(hstart);
    hstart = malloc((viewrec->len + 1) * sizeof(long));
    hsize  = 0;
    for (i = 0; hstart && i < viewrec->len; ++i) {
        hstart[i] = hsize;
        hsize += RecLen(viewrec, i) + 1;
    }
    if (hstart)
        hstart[i] = hsize;
}

/* the line of byte o and the offset in it */
static int hexline(long o, int *off)
{
    int lo = 0, hi = viewrec->len - 1, m;

    while (lo < hi) {
        m = (lo + hi + 1) / 2;
        if (hstart[m] <= o)
            lo = m;
        else
            hi = m - 1;
    }
    *off = (int)(o - hstart[lo]);
    return lo;
}

static int hexget(long o)
{
    int off, l = hexline(o, &off);
    LINE *p = RecGet(viewrec, l);

    return !p || off >= p->len ? '\n' : p->ptr[off] & 0xff;
}

/* byte o is v now; -1 for a line end or a new one */
static int hexput(long o, int v)
{
    int off, l = hexline(o, &off);
    LINE *p = RecGet(viewrec, l);

    if (!p || off >= p->len || v == '\n')
        return -1;
    p->ptr[off] = (char)v;
    RecPut(p, p->len);
    editmodflg = 1;
    return 0;
}

static void hexrow(long row, int y)
{
    long o = row * 16;
    int i, n = hsize - o > 16 ? 16 : (int)(hsize - o);

    VMove(y, 0);
    for (i = 0; i < 16; ++i)
        if (i < n) {
            int c = hexget(o + i);

            VPutChar("0123456789abcdef"[c >> 4]);
            VPutChar("0123456789abcdef"[c & 0xf]);
            VPutChar(' ');
        } else
            VPrint("   ");
    for (i = 0; i < n; ++i) {
        viewchar(hexget(o + i));
        if (i < n - 1)
            VPutChar(' ');
    }
    VClearLine();
}

static void hexhead(char *name, long pos)
{
    char b[64];
    int i, c = pos < hsize ? hexget(pos) : 0;

    VSetDim();
    VStandOut();
    VMove(0, 0);
    for (i = COLS; --i >= 0; VPutChar(' '))
        ;
    if (editmodflg)
        VMPutString(0, 0, "*");
    VMPutString(0, 1, _("File "));
    VPutString(name);
    VPrint("  %s", _("Hex"));
    snprintf(b, sizeof(b), _("Byte %ld of %ld  %02x"), pos, hsize, c);
    VMPutString(0, COLS - 1 - VStrLen(b), b);
    VSetNormal();
    VStandEnd();
}

static const char *const hexkeys[10] = { NC_("keybar", "Help"),
                                         NC_("keybar", "Save"),
                                         "",
                                         NC_("keybar", "Text"),
                                         NC_("keybar", "Top"),
                                         NC_("keybar", "Bottom"),
                                         "",
                                         "",
                                         "",
                                         NC_("keybar", "Quit") };

/* the hex mode from line *pl, offset *po, and where it left them; the key
 * that ended it */
static int hexedit(char *filename, char *basename, int *pl, int *po)
{
    int rows = LINES - 2, field = 0, nib = 0, c, d, y;
    long pos, base = -1, newbase, row;

    hexindex();
    if (!hstart)
        return meta('D');
    pos = *pl < viewrec->len ? hstart[*pl] + *po : 0;
    if (pos >= hsize)
        pos = hsize ? hsize - 1 : 0;
    drawkeys(hexkeys);
    newbase = pos / 16 - rows / 2;
    for (;;) {
        row = pos / 16;
        if (newbase > (hsize + 15) / 16 - rows)
            newbase = (hsize + 15) / 16 - rows;
        if (newbase < 0)
            newbase = 0;
        if (row < newbase)
            newbase = row;
        if (row >= newbase + rows)
            newbase = row - rows + 1;
        if (newbase != base) {
            base = newbase;
            for (y = 0; y < rows; ++y)
                hexrow(base + y, y + 1);
        }
        hexhead(basename, pos);
        VMove((int)(row - base) + 1, field ? 48 + (int)(pos % 16) * 2 : (int)(pos % 16) * 3 + nib);
        VSync();
        switch (c = KeyGet()) {
        case meta('A'): /* f1 */
            runhelp("edithex");
            continue;
        case cntrl(']'):
            VRedraw();
            continue;
        case cntrl('I'): /* the other field */
            field ^= 1;
            nib = 0;
            continue;
        case meta('B'): /* f2 - save */
            message(_(" Save "), _("Saving file '%s'..."), basename);
            if (RecSave(viewrec, filename) == 0)
                editmodflg = 0;
            endmesg();
            continue;
        case meta('l'):
            if (nib)
                nib = 0;
            else if (pos > 0)
                --pos;
            continue;
        case meta('r'):
            if (pos < hsize - 1)
                ++pos;
            nib = 0;
            continue;
        case meta('u'):
            if (pos >= 16)
                pos -= 16;
            continue;
        case meta('d'):
            if (pos + 16 < hsize)
                pos += 16;
            continue;
        case meta('p'):
            pos -= 16L * (rows - 1);
            if (pos < 0)
                pos = 0;
            newbase -= rows - 1;
            continue;
        case meta('n'):
            pos += 16L * (rows - 1);
            if (pos >= hsize)
                pos = hsize ? hsize - 1 : 0;
            newbase += rows - 1;
            continue;
        case meta('h'):
            pos -= pos % 16;
            nib = 0;
            continue;
        case meta('e'):
            pos = pos - pos % 16 + 15;
            if (pos >= hsize)
                pos = hsize ? hsize - 1 : 0;
            nib = 0;
            continue;
        case KEY_CHOME:
        case KEY_CPGUP:
        case meta('E'):
            pos = nib = 0;
            continue;
        case KEY_CEND:
        case KEY_CPGDN:
        case meta('F'):
            pos = hsize ? hsize - 1 : 0;
            nib = 0;
            continue;
        default:
            /* F4 back; F10, Esc and the other keys of the text: back,
             * and the text does them */
            if (c > 0377 || c == cntrl('[') || c == cntrl('C'))
                break;
            if (c < ' ' || c == 0177 || pos >= hsize) {
                VBeep();
                continue;
            }
            if (field) { /* the text field: the character */
                d = c;
                if (hexput(pos, d) < 0) {
                    VBeep();
                    continue;
                }
                hexrow(row, (int)(row - base) + 1);
                if (pos < hsize - 1)
                    ++pos;
                continue;
            }
            if (c >= '0' && c <= '9')
                d = c - '0';
            else if ((c | 040) >= 'a' && (c | 040) <= 'f')
                d = (c | 040) - 'a' + 10;
            else {
                VBeep();
                continue;
            }
            d = nib ? (hexget(pos) & 0xf0) | d : (hexget(pos) & 0x0f) | d << 4;
            if (hexput(pos, d) < 0) {
                VBeep();
                continue;
            }
            hexrow(row, (int)(row - base) + 1);
            if (!nib)
                nib = 1;
            else {
                nib = 0;
                if (pos < hsize - 1)
                    ++pos;
            }
            continue;
        }
        break;
    }
    *pl = hsize ? hexline(pos, po) : 0;
    if (!hsize)
        *po = 0;
    return c;
}

static void edit1(int d, char *filename, char *basename);

/* the editor, in its colours */
void editfile(int d, char *filename, char *basename)
{
    int oldc = VSetColor(VCOL_VIEW);

    edit1(d, filename, basename);
    VSetColor(oldc);
}

static void edit1(int d, char *filename, char *basename)
{
    static char asname[1024]; /* the name saved under, Shift-F2 */
    LINE *lp;
    int i, n;
    register int baseline, basecol;
    int stepud = LINES - 3; /* step up-down */
    int sline, soff;        /* search position */
    int ccol, lastccol, c;

    viewfd  = d;
    viewrec = RecOpen(d, 1);
    if (!viewrec)
        return;
    editmodflg = 0;
    if (viewrec->broken) {
        if (0 != getchoice(1, _(" Edit "), _("File contains too long lines."), 0,
                           _(" Break lines "), _(" Cancel editing "), 0))
            return;
        RecBreak(viewrec);
        editmodflg = 1;
    }
    baseline = basecol = 0;
    sline = ccol = 0;
    VClearBox(1, 0, LINES - 2, COLS);
    drawkeys(editkeys);
    for (;;) {
        viewpage(baseline, basecol);
        for (;;) {
            soff = offset(sline, ccol);
            if (soff > lastpos(sline))
                --soff;
            lastccol = column(sline, soff);
            if (lastccol < basecol || lastccol >= basecol + COLS) {
                basecol = lastccol / 40 * 40 - 40;
                if (basecol < 0)
                    basecol = 0;
                break;
            }
            edithead(basename, sline, lastccol, soff);
            VMove(sline - baseline + 1, lastccol - basecol);
            VSync();
            switch (c = KeyGet()) {
            case meta('A'): /* f1 */
                runhelp("edit");
                continue;
            case cntrl(']'): /* redraw screen */
                VRedraw();
                continue;
            case cntrl('Q'):
            case cntrl('V'): /* quote next char */
                VPutString("\3\16#\2\17");
                ccol = column(sline, soff);
                VMove(sline - baseline + 1, ccol - basecol);
                VSync();
                c = KeyGetChar();
                goto inschar;
            case KEY_ABS: /* Alt-Backspace: the word left */
                if (!soff)
                    goto backspace;
                lp = RecGet(viewrec, sline);
                i  = wordbeg(lp, soff);
                n  = soff - i;
                keepdel(lp->ptr + i, n, 0);
                while (n-- > 0)
                    RecDelChar(viewrec, sline, i);
                soff       = i;
                ccol       = column(sline, soff);
                editmodflg = 1;
                viewline(sline, basecol, sline - baseline + 1);
                continue;
            case KEY_CDEL: /* Ctrl-Del: the word right */
                if (soff >= lastpos(sline))
                    goto delchar;
                lp = RecGet(viewrec, sline);
                n  = wordend(lp, soff) - soff;
                keepdel(lp->ptr + soff, n, 0);
                while (n-- > 0)
                    RecDelChar(viewrec, sline, soff);
                editmodflg = 1;
                viewline(sline, basecol, sline - baseline + 1);
                continue;
            case cntrl('U'): /* undelete */
                if (!undellen && !undelline)
                    continue;
                editmodflg = 1;
                if (viewrec->len <= 0)
                    RecInsLine(viewrec, 0);
                if (undelline) { /* the line back above */
                    RecInsLine(viewrec, sline);
                    if (undellen && (lp = RecGet(viewrec, sline))) {
                        lp->ptr = malloc(undellen);
                        memcpy(lp->ptr, undel, undellen);
                        RecPut(lp, undellen);
                    }
                    ccol = 0;
                    break;
                }
                for (i = 0; i < undellen; ++i)
                    RecInsChar(viewrec, sline, soff + i, undel[i] & 0xff);
                ccol = column(sline, soff + undellen);
                viewline(sline, basecol, sline - baseline + 1);
                continue;
            case meta('b'): /* back space */
            backspace:
                if (soff) {
                    soff       = charstart(sline, soff - 1);
                    ccol       = column(sline, soff);
                    editmodflg = 1;
                    for (c = charlen(sline, soff); c > 0; --c)
                        RecDelChar(viewrec, sline, soff);
                    viewline(sline, basecol, sline - baseline + 1);
                    continue;
                }
                if (!sline)
                    continue;
                --sline;
                soff = lastpos(sline);
                ccol = column(sline, soff);
            join:
                editmodflg = 1;
                joinlines(viewrec, sline);
                RecDelLine(viewrec, sline + 1);
                if (ccol < basecol || ccol >= basecol + COLS) {
                    basecol = ccol < 40 ? 0 : ((ccol - 40) / 40 * 40);
                    break;
                }
                if (sline < baseline) {
                    --baseline;
                    viewline(baseline, basecol, 1);
                    continue;
                }
                if (sline < baseline + LINES - 4) {
                    if (soff)
                        VDelLine(2 + sline - baseline);
                    else
                        VDelLine(1 + sline - baseline);
                    VInsLine(LINES - 2);
                    viewline(sline, basecol, sline - baseline + 1);
                    viewline(baseline + LINES - 3, basecol, LINES - 2);
                    continue;
                }
                if (sline == baseline + LINES - 4) {
                    viewline(sline, basecol, LINES - 3);
                    viewline(sline + 1, basecol, LINES - 2);
                    continue;
                }
                /* now sline equals to baseline+LINES-3 */
                viewline(sline, basecol, LINES - 2);
                continue;
            case cntrl('G'): /* delete char */
            delchar:
                if (soff == lastpos(sline)) {
                    if (sline == viewrec->len - 1)
                        continue;
                    goto join;
                }
                editmodflg = 1;
                for (c = charlen(sline, soff); c > 0; --c)
                    RecDelChar(viewrec, sline, soff);
                viewline(sline, basecol, sline - baseline + 1);
                continue;
            case cntrl('K'): /* clear to end of line */
                if (soff == lastpos(sline))
                    continue;
                editmodflg = 1;
                lp         = RecGet(viewrec, sline);
                keepdel(lp->ptr + soff, lp->len - soff, 0);
                cleartoeoln(viewrec, sline, soff);
                viewline(sline, basecol, sline - baseline + 1);
                continue;
            case cntrl('Y'): /* delete line */
                editmodflg = 1;
                ccol       = 0;
                if ((lp = RecGet(viewrec, sline)))
                    /* the last line is only cleared: Ctrl-U fills it again */
                    keepdel(lp->ptr, lp->len, sline < viewrec->len - 1);
                if (sline == viewrec->len - 1) {
                    clearline(viewrec, sline);
                    viewline(sline, basecol, sline - baseline + 1);
                    continue;
                }
                RecDelLine(viewrec, sline);
                if (sline < baseline + LINES - 3) {
                    VDelLine(1 + sline - baseline);
                    VInsLine(LINES - 2);
                }
                viewline(baseline + LINES - 3, basecol, LINES - 2);
                continue;
            case cntrl('J'):
            case cntrl('M'): /* insert new line */
                editmodflg = 1;
                if (viewrec->len <= 0)
                    RecInsLine(viewrec, 0);
                breakline(viewrec, sline, soff);
                ccol = 0;
                ++sline;
                if (sline < baseline + LINES - 2) {
                    VDelLine(LINES - 2);
                    if (soff)
                        VInsLine(1 + sline - baseline);
                    else
                        VInsLine(sline - baseline);
                    viewline(sline - 1, basecol, sline - baseline);
                    viewline(sline, basecol, 1 + sline - baseline);
                    continue;
                }
                ++baseline;
                VDelLine(1);
                VInsLine(LINES - 2);
                viewline(sline - 1, basecol, LINES - 3);
                viewline(sline, basecol, LINES - 2);
                continue;
            default: /* insert char */
                if ((c < ' ' && c != '\t') || c > 0377) {
                    VBeep();
                    continue;
                }
            inschar:
                if (viewrec->len <= 0)
                    RecInsLine(viewrec, 0);
                RecInsChar(viewrec, sline, soff, c);
                editmodflg = 1;
                viewline(sline, basecol, sline - baseline + 1);
                ccol = column(sline, soff + 1);
                continue;
            case meta('B'): /* f2 - save */
                message(_(" Save "), _("Saving file '%s'..."), basename);
                if (RecSave(viewrec, filename) == 0)
                    editmodflg = 0;
                endmesg();
                continue;
            case KEY_SF(2): { /* shift-f2 - save as */
                char *p = getstring(60, filename, _(" Save as "), _("Save the file as"));

                if (!p || !*p)
                    continue;
                if (strcmp(p, filename) && !access(p, F_OK) &&
                    getchoice(1, _(" Save as "), _("File exists"), p, _(" Overwrite "),
                              _(" Cancel "), 0))
                    continue;
                snprintf(asname, sizeof(asname), "%s", p);
                message(_(" Save "), _("Saving file '%s'..."), asname);
                i = strcmp(asname, filename) ? RecSaveAs(viewrec, asname)
                                             : RecSave(viewrec, asname);
                endmesg();
                if (i < 0)
                    continue;
                filename = basename = asname;
                editmodflg          = 0;
                continue;
            }
            case meta('D'): /* f4 - hex */
                c = hexedit(filename, basename, &sline, &soff);
                drawkeys(editkeys);
                ccol = column(sline, soff);
                if (sline < baseline || sline >= baseline + LINES - 2) {
                    baseline = sline - (LINES - 2) / 2;
                    if (baseline > viewrec->len - (LINES - 2))
                        baseline = viewrec->len - (LINES - 2);
                    if (baseline < 0)
                        baseline = 0;
                }
                basecol = ccol < 40 ? 0 : ((ccol - 40) / 40 * 40);
                if (c != meta('D'))
                    KeyUnget(c); /* F10, Esc ...: the text does it */
                break;
            case meta('C'): /* f3 - raw */
                viewraw ^= 1;
                break;
            case meta('I'): /* f9 - spaces */
                viewtabs ^= 1;
                break;
            case meta('H'): /* f8 - home */
                if (sline == baseline && ccol == 0)
                    continue;
                sline = baseline;
                ccol  = 0;
                break;
            case KEY_CHOME:
            case KEY_CPGUP:
            case meta('E'): /* f5 - top */
                if (sline == 0 && baseline == 0 && ccol == 0)
                    continue;
                sline = baseline = 0;
                ccol             = 0;
                break;
            case KEY_CEND:
            case KEY_CPGDN:
            case meta('F'): /* f6 - bottom */
                if (soff == lastpos(sline) && sline >= viewrec->len - 1)
                    continue;
                baseline = viewrec->len - (LINES - 2);
                if (baseline < 0)
                    baseline = 0;
                sline = viewrec->len - 1;
                if (sline < 0)
                    sline = 0;
                soff = lastpos(sline);
                ccol = column(sline, soff);
                if (ccol < basecol || ccol >= basecol + COLS)
                    basecol = ccol < 40 ? 0 : ((ccol - 40) / 40 * 40);
                break;
            case KEY_SF(10): /* shift-f10 - save and quit */
                if (editmodflg && RecSave(viewrec, filename) < 0)
                    continue;
                goto leave;
            case cntrl('C'):
            case cntrl('['):
            case meta('J'): /* f0 - quit */
                if (editmodflg)
                    switch (getchoice(1, _(" Edit "), _("You've made changes since the last save."),
                                      0, _(" Save "), _(" Don't save "), _(" Continue editing "))) {
                    case 0:
                        if (RecSave(viewrec, filename) < 0)
                            continue;
                    case 1:
                        break;
                    default:
                        continue;
                    }
            leave:
                VClearBox(1, 0, LINES - 2, COLS);
                RecClose(viewrec);
                return;
            case KEY_SF(7):   /* shift-f7 - search again */
            case meta('G'): { /* f7 - search */
                register char *p = viewsrch;

                if (c == meta('G') || !*viewsrch) {
                    if (!(p = getstring(40, viewsrch, _(" Search "), _("Search for the string"))))
                        continue;
                    snprintf(viewsrch, sizeof(viewsrch), "%s", p);
                    p = viewsrch;
                }
                if (!search(p, sline, soff + 1, &sline, &soff))
                    continue;
                if (baseline > sline || baseline + LINES - 2 <= sline)
                    baseline = sline;
                if (baseline > viewrec->len - (LINES - 2))
                    baseline = viewrec->len - (LINES - 2);
                if (baseline < 0)
                    baseline = 0;
                ccol    = column(sline, soff);
                basecol = ccol < 40 ? 0 : ((ccol + strlen(p) - 41) / 40 * 40);
                break;
            }
            case cntrl('E'): /* up */
            case meta('u'):  /* up */
                if (sline <= 0)
                    continue;
                --sline;
                if (baseline <= sline)
                    continue;
                --baseline;
                VDelLine(LINES - 2);
                VInsLine(1);
                viewline(baseline, basecol, 1);
                continue;
            case cntrl('X'): /* down */
            case meta('d'):  /* down */
                if (sline >= viewrec->len - 1)
                    continue;
                ++sline;
                if (baseline + (LINES - 2) > sline)
                    continue;
                ++baseline;
                VDelLine(1);
                VInsLine(LINES - 2);
                viewline(baseline + LINES - 3, basecol, LINES - 2);
                continue;
            case KEY_CRIGHT:
            case cntrl('F'): /* word right */
                soff = wordend(RecGet(viewrec, sline), soff);
                ccol = column(sline, soff);
                continue;
            case KEY_CLEFT:
            case cntrl('W'): /* word left */
                soff = wordbeg(RecGet(viewrec, sline), soff);
                ccol = column(sline, soff);
                continue;
            case cntrl('D'):
            case meta('r'): /* right */
                if (soff == lastpos(sline)) {
                    if (sline == viewrec->len - 1)
                        continue;
                    ++sline;
                    ccol = 0;
                    if (baseline + LINES - 2 <= sline) {
                        ++baseline;
                        VDelLine(1);
                        VInsLine(LINES - 2);
                        viewline(baseline + LINES - 3, basecol, LINES - 2);
                    }
                    continue;
                }
                ccol = column(sline, ++soff);
                continue;
            case cntrl('A'):
            case cntrl('S'):
            case meta('l'): /* left */
                if (soff <= 0) {
                    if (sline <= 0)
                        continue;
                    --sline;
                    soff = lastpos(sline);
                    ccol = column(sline, soff);
                    if (ccol < basecol || ccol >= basecol + COLS) {
                        basecol = ccol < 40 ? 0 : ((ccol - 40) / 40 * 40);
                        if (baseline > sline)
                            baseline = sline;
                        break;
                    }
                    if (baseline > sline) {
                        baseline = sline;
                        VDelLine(LINES - 2);
                        VInsLine(1);
                        viewline(baseline, basecol, 1);
                    }
                    continue;
                }
                soff = charstart(sline, soff - 1);
                ccol = column(sline, soff);
                continue;
            case cntrl('B'):
            case meta('n'): /* next page */
                if (baseline >= viewrec->len - (LINES - 2))
                    continue;
                baseline += stepud;
                sline += stepud;
                if (baseline > viewrec->len - (LINES - 2)) {
                    sline -= baseline - (viewrec->len - (LINES - 2));
                    baseline = viewrec->len - (LINES - 2);
                }
                break;
            case cntrl('R'):
            case meta('p'): /* prev page */
                if (baseline <= 0)
                    continue;
                baseline -= stepud;
                sline -= stepud;
                if (baseline < 0) {
                    sline -= baseline;
                    baseline = 0;
                }
                break;
            case meta('h'): /* home */
                if (!ccol)
                    continue;
                ccol = basecol = 0;
                break;
            case meta('e'): /* end */
                c = lastpos(sline);
                if (soff == c)
                    continue;
                soff = c;
                ccol = column(sline, soff);
                if (ccol < basecol || ccol >= basecol + COLS)
                    basecol = ccol < 40 ? 0 : ((ccol - 40) / 40 * 40);
                break;
            }
            break;
        }
    }
}

static void clearline(REC *r, int line)
{
    register LINE *p;

    if (!(p = RecGet(r, line)))
        return;
    if (p->len <= 0)
        return;
    free(p->ptr);
    p->ptr = "";
    RecPut(p, 0);
}

static void joinlines(REC *r, int line)
{
    register LINE *p, *q;
    char *s;

    if (!(q = RecGet(r, line + 1)) || !q->len)
        return;
    if (!(p = RecGet(r, line)))
        return;
    if (!p->len) {
        p->ptr = q->ptr;
        q->ptr = "";
        RecPut(p, q->len);
        RecPut(q, 0);
        return;
    }
    s = malloc(p->len + q->len);
    memcpy(s, p->ptr, p->len);
    memcpy(s + p->len, q->ptr, q->len);
    free(p->ptr);
    free(q->ptr);
    p->ptr = s;
    q->ptr = "";
    RecPut(p, p->len + q->len);
    RecPut(q, 0);
}

static void breakline(REC *r, int line, int off)
{
    register LINE *p, *q;
    char *s;

    if (off <= 0) {
        RecInsLine(r, line);
        return;
    }
    if (!(p = RecGet(r, line)))
        return;
    RecInsLine(r, line + 1);
    if (off >= p->len)
        return;
    if (!(q = RecGet(r, line + 1)))
        return;
    q->ptr = malloc(p->len - off);
    memcpy(q->ptr, p->ptr + off, p->len - off);
    RecPut(q, p->len - off);
    s = malloc(off);
    memcpy(s, p->ptr, off);
    free(p->ptr);
    p->ptr = s;
    RecPut(p, off);
}

static void cleartoeoln(REC *r, int line, int off)
{
    register LINE *p;
    char *s;

    if (!(p = RecGet(r, line)))
        return;
    if (off >= p->len)
        return;
    if (off <= 0) {
        p->ptr = "";
        RecPut(p, 0);
        return;
    }
    s = malloc(off);
    memcpy(s, p->ptr, off);
    free(p->ptr);
    p->ptr = s;
    RecPut(p, off);
}

static int wordend(LINE *p, int off)
{
    register char *s;

    if (off >= p->len)
        return (p->len);
    s = p->ptr + off;
    while (off < p->len && !ischar(*s))
        ++s, ++off;
    if (off >= p->len)
        return (p->len);
    while (off < p->len && ischar(*s))
        ++s, ++off;
    return (off);
}

static int wordbeg(LINE *p, int off)
{
    register char *s;

    if (off <= 0)
        return (0);
    --off;
    s = p->ptr + off;
    while (off > 0 && !ischar(*s))
        --s, --off;
    if (off <= 0)
        return (0);
    while (off >= 0 && ischar(*s))
        --s, --off;
    if (off < 0)
        return (0);
    return (off + 1);
}

static int ischar(int c)
{
    c &= 0xff;
    if (c >= 'A' && c <= 'Z')
        return (1);
    if (c >= 'a' && c <= 'z')
        return (1);
    if (c >= '0' && c <= '9')
        return (1);
    if (c >= 0200 && c <= 0377)
        return (1);
    if (c == '_' || c == '$')
        return (1);
    return (0);
}
