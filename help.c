#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "deco.h"
#include "env.h"
#include "scr.h"

#define CS         34
#define HELPW      72 /* the width of the help box */
#define HELPDIR    "/usr/lib/deco/help/"
#define LCLHELPDIR "/usr/local/lib/deco/help/"

struct helptab {
    char row;
    char col;
    char bold;
    char *str; /* the line as shown, made by inithelp */
};

/* a line of help: the text (translated), the keys after it (not) */
struct hline {
    const char *text, *keys;
};

struct help {
    char *name;
    struct helptab *htab;
    struct hline *mesg;
    char *file; /* the help file read, if it was */
};

static void inithelp(struct help *hp);

static struct hline genmesg[] = {
    { CMDR_NAME ", " CMDR_SHORTCOPY, 0 },
    { N_(" Cursor Arrows "), 0 },
    { N_("Left"), "^[ l" },
    { N_("Right"), "^[ r" },
    { N_("Up"), "^[ u" },
    { N_("Down"), "^[ d" },
    { N_(" Cursor Movement "), 0 },
    { N_("Char left"), "^S, ^A" },
    { N_("Char right"), "^D" },
    { N_(" Panels "), 0 },
    { N_("Switch panels"), "^I, <Tab>" },
    { N_("Panels on/off"), "^O, ^P" },
    { N_("Swap panels"), "^U" },
    { N_("Toggle status"), "^L" },
    { N_("Full screen"), "^F" },
    { N_("Double width"), "^W" },
    { N_("The \"+\" and \"-\" keys select and unselect groups of files."), 0 },
    { N_(" Delete "), 0 },
    { N_("Char left"), "^H, <BS>" },
    { N_("Under cursor"), "^G, <Del>" },
    { N_("Line"), "^Y" },
    { N_(" Other "), 0 },
    { N_("History"), "^B" },
    { N_("Home directory"), "^\\" },
    { N_("Reread directory"), "^R" },
    { N_("Select/Unselect"), "^T, <Ins>" },
    { N_("Previous command"), "^E" },
    { N_("Next command"), "^X" },
    { N_("Run file or"), 0 },
    { N_("Enter directory"), "^M, <Ret>" },
    { N_("Use file name"), "^J, <LF>" },
    { N_("Function keys"), "^[ 0..9" },
    { 0, 0 },
};

static struct helptab gentab[] = {
    { 1, 0, 0, 0 },
    /*  2, */
    { 3, 2, 1, 0 },
    { 4, 6, 0, 0 },
    { 5, 6, 0, 0 },
    { 6, 6, 0, 0 },
    { 7, 6, 0, 0 },
    { 8, 2, 1, 0 },
    { 9, 6, 0, 0 },
    { 10, 6, 0, 0 },
    { 11, 2, 1, 0 },
    { 12, 6, 0, 0 },
    { 13, 6, 0, 0 },
    { 14, 6, 0, 0 },
    { 15, 6, 0, 0 },
    { 16, 6, 0, 0 },
    { 17, 6, 0, 0 },
    /* 18, */
    { 19, 0, 0, 0 },
    /*  2, */
    { 3, CS + 2, 1, 0 },
    { 4, CS + 6, 0, 0 },
    { 5, CS + 6, 0, 0 },
    { 6, CS + 6, 0, 0 },
    { 7, CS + 2, 1, 0 },
    { 8, CS + 6, 0, 0 },
    { 9, CS + 6, 0, 0 },
    { 10, CS + 6, 0, 0 },
    { 11, CS + 6, 0, 0 },
    { 12, CS + 6, 0, 0 },
    { 13, CS + 6, 0, 0 },
    { 14, CS + 6, 0, 0 },
    { 15, CS + 6, 0, 0 },
    { 16, CS + 6, 0, 0 },
    { 17, CS + 6, 0, 0 },
    { 0, 0, 0, 0 },
};

static struct hline editmesg[] = {
    { CMDR_NAME " Editor, " CMDR_SHORTCOPY, 0 },
    { N_(" Cursor Movement "), 0 },
    { N_("Char Left"), "^A" },
    { N_("Char Right"), "^D" },
    { N_("Line Up"), "^E" },
    { N_("Line Down"), "^X" },
    { N_("Word Left"), "^W, ^Left" },
    { N_("Word Right"), "^F, ^Right" },
    { N_("Prev Page"), "^R" },
    { N_("Next Page"), "^B" },
    { N_("Begin of line"), "Home" },
    { N_("End of line"), "End" },
    { N_("Top of file"), "F5, ^Home" },
    { N_("End of file"), "F6, ^End" },
    { N_("Top of screen"), "F8" },
    { N_(" Search "), 0 },
    { N_("Search string"), "F7" },
    { N_("Search again"), "S-F7" },
    { N_(" Delete "), 0 },
    { N_("Char left"), "^H, <BS>" },
    { N_("Under cursor"), "^G, <Del>" },
    { N_("Word left"), "M-<BS>" },
    { N_("Word right"), "^<Del>" },
    { N_("Line"), "^Y" },
    { N_("To end of line"), "^K" },
    { N_("Undelete"), "^U" },
    { N_(" Other "), 0 },
    { N_("Redraw screen"), "^]" },
    { N_("Quote next char"), "^V" },
    { N_("Save file"), "F2" },
    { N_("Save as"), "S-F2" },
    { N_("Save and exit"), "S-F10" },
    { N_("Exit"), "F10, Esc" },
    { N_("Raw mode"), "F3" },
    { N_("Visible spaces"), "F9" },
    { 0, 0 },
};

static struct helptab edittab[] = {
    { 1, 0, 0, 0 },       { 3, 2, 1, 0 },       { 4, 6, 0, 0 },       { 5, 6, 0, 0 },
    { 6, 6, 0, 0 },       { 7, 6, 0, 0 },       { 8, 6, 0, 0 },       { 9, 6, 0, 0 },
    { 10, 6, 0, 0 },      { 11, 6, 0, 0 },      { 12, 6, 0, 0 },      { 13, 6, 0, 0 },
    { 14, 6, 0, 0 },      { 15, 6, 0, 0 },      { 16, 6, 0, 0 },      { 17, 2, 1, 0 },
    { 18, 6, 0, 0 },      { 19, 6, 0, 0 },      { 3, CS + 2, 1, 0 },  { 4, CS + 6, 0, 0 },
    { 5, CS + 6, 0, 0 },  { 6, CS + 6, 0, 0 },  { 7, CS + 6, 0, 0 },  { 8, CS + 6, 0, 0 },
    { 9, CS + 6, 0, 0 },  { 10, CS + 6, 0, 0 }, { 11, CS + 2, 1, 0 }, { 12, CS + 6, 0, 0 },
    { 13, CS + 6, 0, 0 }, { 14, CS + 6, 0, 0 }, { 15, CS + 6, 0, 0 }, { 16, CS + 6, 0, 0 },
    { 17, CS + 6, 0, 0 }, { 18, CS + 6, 0, 0 }, { 19, CS + 6, 0, 0 }, { 0, 0, 0, 0 },
};

static struct hline viewmesg[] = {
    { CMDR_NAME " Viewer, " CMDR_SHORTCOPY, 0 },
    { N_(" Cursor Movement "), 0 },
    { N_("Page Left"), "^A" },
    { N_("Page Right"), "^D" },
    { N_("Line Up"), "^E" },
    { N_("Line Down"), "^X" },
    { N_("Prev Page"), "^R" },
    { N_("Next Page"), "^B" },
    { N_("Top of file"), "Home, F5" },
    { N_("End of file"), "End, F6" },
    { N_(" Search "), 0 },
    { N_("Search string"), "F7" },
    { N_(" Other "), 0 },
    { N_("Redraw screen"), "^]" },
    { N_("Exit"), "^C" },
    { N_(" Modes "), 0 },
    { N_("Raw mode"), "F3" },
    { N_("Visible spaces"), "F9" },
    { N_("Lines longer than 1024 bytes"), 0 },
    { N_("are broken and marked with \"\\\""), 0 },
    { N_("Spaces are displayed as \".\""), 0 },
    { N_("and tabs as \"_\""), 0 },
    { 0, 0 },
};

static struct helptab viewtab[] = {
    { 1, 0, 0, 0 },       { 3, 3, 1, 0 },       { 5, 7, 0, 0 },       { 6, 7, 0, 0 },
    { 7, 7, 0, 0 },       { 8, 7, 0, 0 },       { 9, 7, 0, 0 },       { 10, 7, 0, 0 },
    { 11, 7, 0, 0 },      { 12, 7, 0, 0 },      { 15, 3, 1, 0 },      { 17, 7, 0, 0 },
    { 3, CS + 3, 1, 0 },  { 5, CS + 7, 0, 0 },  { 6, CS + 7, 0, 0 },  { 9, CS + 3, 1, 0 },
    { 11, CS + 7, 0, 0 }, { 12, CS + 7, 0, 0 }, { 15, CS + 3, 0, 0 }, { 16, CS + 3, 0, 0 },
    { 17, CS + 3, 0, 0 }, { 18, CS + 3, 0, 0 }, { 0, 0, 0, 0 },
};

static struct hline hviewmesg[] = {
    { CMDR_NAME " Hex Viewer, " CMDR_SHORTCOPY, 0 },
    { N_(" Cursor Movement "), 0 },
    { N_("Page Left"), "^A" },
    { N_("Page Right"), "^D" },
    { N_("Line Up"), "^E" },
    { N_("Line Down"), "^X" },
    { N_("Prev Page"), "^R" },
    { N_("Next Page"), "^B" },
    { N_("Top of file"), "Home, F5" },
    { N_("End of file"), "End, F6" },
    { N_(" Search "), 0 },
    { N_("Search string"), "F7" },
    { N_(" Other "), 0 },
    { N_("Redraw screen"), "^]" },
    { N_("Exit"), "^C" },
    { N_(" Modes "), 0 },
    { N_("Raw mode"), "F3" },
    { N_("Visible spaces"), "F9" },
    { N_("Spaces are displayed as \".\""), 0 },
    { N_("and tabs as \"_\""), 0 },
    { N_("For hexadecimal search"), 0 },
    { N_("type char codes as \"\\xx\""), 0 },
    { 0, 0 },
};

static struct helptab hviewtab[] = {
    { 1, 0, 0, 0 },       { 3, 3, 1, 0 },       { 5, 7, 0, 0 },       { 6, 7, 0, 0 },
    { 7, 7, 0, 0 },       { 8, 7, 0, 0 },       { 9, 7, 0, 0 },       { 10, 7, 0, 0 },
    { 11, 7, 0, 0 },      { 12, 7, 0, 0 },      { 15, 3, 1, 0 },      { 17, 7, 0, 0 },
    { 3, CS + 3, 1, 0 },  { 5, CS + 7, 0, 0 },  { 6, CS + 7, 0, 0 },  { 9, CS + 3, 1, 0 },
    { 11, CS + 7, 0, 0 }, { 12, CS + 7, 0, 0 }, { 15, CS + 3, 0, 0 }, { 16, CS + 3, 0, 0 },
    { 17, CS + 3, 0, 0 }, { 18, CS + 3, 0, 0 }, { 0, 0, 0, 0 },
};

static struct help help[] = {
    { "general", gentab, genmesg },
    { "edit", edittab, editmesg },
    { "view", viewtab, viewmesg },
    { "hexview", hviewtab, hviewmesg },
    { 0, 0, 0 },
};

struct help *findhelp(char *name)
{
    register struct help *p;

    for (p = help; p->name; ++p)
        if (!strcmp(p->name, name))
            return (p);
    return (0);
}

void genhelp()
{
    runhelp("general");
}

void runhelp(char *name)
{
    register int r, c, w, h;
    BOX *box;
    register struct helptab *p;
    struct help *hp;
    int oldc;

    hp = findhelp(name);
    if (!hp)
        return;
    p = hp->htab;
    if (!p->str)
        inithelp(hp);
    w = HELPW;
    h = 21;
    r = 2;
    c = (COLS - w) / 2;

    oldc = VSetColor(VCOL_DIALOG);
    box  = VGetBox(r, c - 1, h, w + 2); /* save box */
    VStandOut();
    VSetDim();
    VFillBox(r, c - 1, h, w + 2, ' ');                               /* clear */
    VDrawBox(r, c, h, w);                                            /* draw margins */
    VMPutString(r, c + (w - VStrLen(_(" Help "))) / 2, _(" Help ")); /* head */

    for (; p->row; ++p) {
        if (p->row >= h)
            continue;
        if (p->bold) {
            VSetBold();
            VStandEnd();
        }
        if (!p->col)
            VMPutString(r + p->row, c + (w - VStrLen(p->str)) / 2, p->str);
        else
            VMPutString(r + p->row, c + p->col, p->str);
        if (p->bold) {
            VSetDim();
            VStandOut();
        }
    }
    VStandEnd();
    VSetNormal();
    for (;;) {
        hidecursor();
        VSync();
        switch (KeyGet()) {
        default:
            break;
        case cntrl(']'): /* redraw screen */
            VRedraw();
            continue;
        }
        break;
    }
    VUngetBox(box);
    VFreeBox(box);
    VSetColor(oldc);
}

static int findhelpfile(char *filename, char *name, char *dir)
{
    register char *s;

    s = filename; /* build dir/name */
    strcpy(s, dir);
    s += strlen(s);
    switch (lang) {
    case ENG:
        *s++ = 'e';
        *s++ = '.';
        break;
    case RUS:
        *s++ = 'r';
        *s++ = '.';
        break;
    case UKR:
        *s++ = 'u';
        *s++ = '.';
        break;
    case DEU:
        *s++ = 'd';
        *s++ = '.';
        break;
    case FRA:
        *s++ = 'f';
        *s++ = '.';
        break;
    }
    strcpy(s, name);
    return (!access(filename, 4)); /* can I read it ? */
}

/* the width of the text before the keys, at least */
#define KEYCOL 17

/*
 * The lines of a help in the language now: the built-in ones through
 * gettext, the text padded to KEYCOL cells before its keys; Russian and
 * Ukrainian (no catalogue yet) from their help files, a line each, '@'
 * a space.
 */
static void inithelp(struct help *hp)
{
    struct helptab *p;
    struct hline *q;
    char filename[80], *b, *t;
    int n, fd, k, ncols = 0, cols[4], keycol[4], keys[4];

    if (lang == RUS || lang == UKR) {
        if (!findhelpfile(filename, hp->name, LCLHELPDIR))
            findhelpfile(filename, hp->name, HELPDIR);
        fd = open(filename, 0);
        if (fd >= 0 && (hp->file = malloc(4096))) {
            n                       = read(fd, hp->file, 4095);
            hp->file[n > 0 ? n : 0] = 0;
            b                       = hp->file;
            for (p = hp->htab; p->row && *b; ++p) {
                for (p->str = b; *b && *b != '\n'; ++b)
                    if (*b == '@')
                        *b = ' ';
                if (*b)
                    *b++ = 0;
            }
            for (; p->row; ++p)
                p->str = "";
            close(fd);
            return;
        }
        if (fd >= 0)
            close(fd);
    }
    /* the keys of a column after its longest text, at least at KEYCOL */
    for (p = hp->htab, q = hp->mesg; p->row && q->text; ++p, ++q)
        if (q->keys) {
            n = VStrLen(_(q->text)) + 1;
            for (k = 0; k < ncols && cols[k] != p->col; ++k)
                ;
            if (k == ncols && ncols < 4) {
                cols[ncols]     = p->col;
                keys[ncols]     = 0;
                keycol[ncols++] = KEYCOL;
            }
            if (k < ncols && n > keycol[k])
                keycol[k] = n;
            if (k < ncols && (int)strlen(q->keys) > keys[k])
                keys[k] = strlen(q->keys);
        }
    for (k = 0; k < ncols; ++k) /* the keys inside the frame of HELPW */
        if (cols[k] + keycol[k] + keys[k] > HELPW - 2)
            keycol[k] = HELPW - 2 - cols[k] - keys[k];
    for (p = hp->htab, q = hp->mesg; p->row && q->text; ++p, ++q) {
        t = _(q->text);
        if (!q->keys) {
            p->str = t;
            continue;
        }
        for (k = 0; k < ncols && cols[k] != p->col; ++k)
            ;
        n = (k < ncols ? keycol[k] : KEYCOL) - VStrLen(t);
        if (n < 1)
            n = 1;
        p->str = malloc(strlen(t) + n + strlen(q->keys) + 1);
        if (!p->str) {
            p->str = t;
            continue;
        }
        strcpy(p->str, t);
        memset(p->str + strlen(t), ' ', n);
        strcpy(p->str + strlen(t) + n, q->keys);
    }
}

/* the language changed: the help lines anew when shown next */
void helpreset()
{
    struct help *hp;
    struct helptab *p;
    struct hline *q;

    for (hp = help; hp->name; ++hp) {
        if (!hp->htab->str)
            continue;
        for (p = hp->htab, q = hp->mesg; p->row; ++p) {
            if (!hp->file && q->text && q->keys)
                free(p->str);
            if (q->text)
                ++q;
            p->str = 0;
        }
        free(hp->file);
        hp->file = 0;
    }
}
